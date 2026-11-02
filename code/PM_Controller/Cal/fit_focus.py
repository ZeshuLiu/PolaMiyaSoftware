#!/usr/bin/env python3
"""Fit focus RAW against object distance using a thin-lens-inspired curve.

Usage (from any working directory):
    python path/to/Cal/fit_focus.py cal90DA724F2FBC.csv
    python path/to/Cal/fit_focus.py path/to/another.csv

Input: two columns, distance in metres then RAW; optional header. Infinity may
be INF, infinity, ∞ or 无穷远. Results are grouped by device in Cal/Result.
Dependencies: numpy, scipy, matplotlib (python -m pip install numpy scipy matplotlib).
No firmware is built, changed or flashed. Original measurements are never edited.
"""
from __future__ import annotations

import argparse
import csv
from dataclasses import asdict, dataclass
import hashlib
import io
import json
import math
from pathlib import Path
import re
import sys

try:
    import numpy as np
    import scipy
    from scipy.optimize import minimize_scalar
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import StrMethodFormatter
except ImportError as error:
    raise SystemExit("Missing dependency. Run: python -m pip install numpy scipy matplotlib\n" + str(error)) from error

REFERENCE = "https://www.edmundoptics.com/knowledge-center/application-notes/imaging/basic-lens-selection/"
SCRIPT_DIR = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Observation:
    line: int
    distance_m: float
    raw: float


@dataclass(frozen=True)
class Model:
    raw_infinity: float
    a_raw_m: float
    b_m: float
    infinity_anchored: bool

    def predict(self, distance_m):
        distance = np.asarray(distance_m, dtype=float)
        if np.any(np.isnan(distance) | (distance <= self.b_m)):
            raise ValueError("Prediction distance must be greater than model pole B.")
        reciprocal = 1.0 / distance  # Positive infinity maps exactly to zero.
        return self.raw_infinity + self.a_raw_m * reciprocal / (1.0 - self.b_m * reciprocal)

    def inverse(self, raw):
        delta = np.asarray(raw, dtype=float) - self.raw_infinity
        result = np.full_like(delta, np.inf)
        np.divide(self.a_raw_m, delta, out=result, where=delta != 0)
        result = np.where(delta == 0, np.inf, self.b_m + result)
        return np.where((delta != 0) & (delta * self.a_raw_m <= 0), np.nan, result)


def resolve_input(filename: str) -> Path:
    candidate = Path(filename).expanduser()
    for path in (candidate, SCRIPT_DIR / candidate):
        if path.is_file():
            return path.resolve()
    raise ValueError(f"CSV not found: {filename}; also searched {SCRIPT_DIR}")


def device_id(stem: str) -> str:
    """Extract the 12-hex MAC serial after cal; keep other filenames usable."""
    match = re.match(r"^cal([0-9a-f]{12})(?=$|[^0-9a-f])", stem, re.IGNORECASE)
    return match.group(1).upper() if match else stem


def read_observations(path: Path, offset_only: bool = False) -> list[Observation]:
    data = path.read_bytes()
    encodings = ("utf-16",) if data.startswith((b"\xff\xfe", b"\xfe\xff")) else ("utf-8-sig", "gb18030")
    for encoding in encodings:
        try:
            content = data.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    else:
        raise ValueError("Cannot decode CSV as UTF-8, UTF-16 or GB18030.")
    try:
        dialect = csv.Sniffer().sniff(content[:4096], delimiters=",;\t")
    except csv.Error:
        dialect = csv.excel
    reader = csv.reader(io.StringIO(content), dialect)
    observations = []
    infinity_labels = {"INF", "+INF", "INFINITY", "+INFINITY", "∞", "无穷远", "无穷"}
    headers = {"distance", "distance_m", "distance(m)", "距离", "物距", "距离(m)", "物距(m)"}
    for row in reader:
        if not row or not any(cell.strip() for cell in row) or row[0].lstrip().startswith("#"):
            continue
        if len(row) != 2:
            raise ValueError(f"Line {reader.line_num}: expected exactly two columns, got {len(row)}.")
        distance_text, raw_text = (cell.strip() for cell in row)
        if not observations and distance_text.lower().replace(" ", "") in headers:
            continue
        distance_text = re.sub(r"\s*(?:m|米)\s*$", "", distance_text, flags=re.IGNORECASE)
        try:
            distance = math.inf if distance_text.upper() in infinity_labels else float(distance_text)
            raw = float(raw_text)
        except ValueError as error:
            raise ValueError(f"Line {reader.line_num}: invalid distance/RAW: {row!r}") from error
        if math.isnan(distance) or distance <= 0 or not math.isfinite(raw):
            raise ValueError(f"Line {reader.line_num}: distance must be positive (or INF); RAW must be finite.")
        observations.append(Observation(reader.line_num, distance, raw))
    finite_distances = {point.distance_m for point in observations if math.isfinite(point.distance_m)}
    if len(finite_distances) < (1 if offset_only else 3):
        raise ValueError("At least one finite distance is required in offset-only mode; full fitting requires three distinct finite distances.")
    if not offset_only and np.ptp([point.raw for point in observations]) == 0:
        raise ValueError("All RAW values are equal; no focus-distance curve can be identified.")
    return observations


def fit_model(distance: np.ndarray, raw: np.ndarray, infinity_raw: float | None) -> Model:
    """Profile least squares: for each B, solve A (and optional R_inf) linearly.

    Constrain 0 <= B <= 0.95 * nearest finite distance. Scan first, then refine
    the best interval to avoid depending on an arbitrary nonlinear initial guess.
    Infinity rows are retained; when present their mean is a fixed anchor.
    """
    q = 1.0 / distance
    upper = 0.95 * float(np.min(distance[np.isfinite(distance)]))

    def solve(b: float) -> tuple[float, float, float]:
        z = q / (1.0 - b * q)
        if infinity_raw is None:
            baseline, a = np.linalg.lstsq(np.column_stack((np.ones_like(z), z)), raw, rcond=None)[0]
        else:
            baseline = infinity_raw
            a = float(np.dot(z, raw - baseline) / np.dot(z, z))
        error = baseline + a * z - raw
        return float(np.dot(error, error)), float(baseline), float(a)

    grid = np.linspace(0.0, upper, 129)
    scores = np.array([solve(float(b))[0] for b in grid])
    best = int(np.argmin(scores))
    lo, hi = float(grid[max(0, best - 1)]), float(grid[min(len(grid) - 1, best + 1)])
    optimum = minimize_scalar(lambda b: solve(float(b))[0], bounds=(lo, hi), method="bounded",
                              options={"xatol": max(1e-12, upper * 1e-11)})
    if not optimum.success:
        raise ValueError(f"Curve optimization did not converge: {optimum.message}")
    b = min((0.0, upper, float(grid[best]), float(optimum.x)), key=lambda value: solve(value)[0])
    _, baseline, a = solve(b)
    if not np.all(np.isfinite([baseline, a, b])) or abs(a) < 1e-12:
        raise ValueError("The data did not identify a finite, invertible curve.")
    return Model(baseline, a, b, infinity_raw is not None)


def fit_offset(distance: np.ndarray, raw: np.ndarray, reference: dict) -> Model:
    a, b = float(reference["a_raw_m"]), float(reference["b_m"])
    if not np.isfinite(a) or not np.isfinite(b) or a == 0 or np.any(distance <= b):
        raise ValueError("Invalid reference parameters or measurement distance at/below reference pole.")
    baseline = float(np.mean(raw - a / (distance - b)))
    return Model(baseline, a, b, False)


def metrics(observed: np.ndarray, predicted: np.ndarray) -> dict:
    error = predicted - observed
    variance = float(np.sum((observed - np.mean(observed)) ** 2))
    return {
        "count": len(observed),
        "rmse_raw": float(np.sqrt(np.mean(error ** 2))),
        "mae_raw": float(np.mean(np.abs(error))),
        "max_absolute_error_raw": float(np.max(np.abs(error))),
        "r_squared": 1.0 - float(np.sum(error ** 2)) / variance if variance > 0 else None,
    }


def finite_loo(distance: np.ndarray, raw: np.ndarray, infinity_raw: float | None) -> np.ndarray:
    """Leave out one finite row; retain the measured infinity anchor in training.

    This is a calibration diagnostic, not an independent repeated measurement.
    An unavailable/out-of-domain prediction remains NaN, never silently removed.
    """
    predicted = np.full(len(raw), np.nan)
    if len(set(distance[np.isfinite(distance)])) < 4:
        return predicted
    for index in np.flatnonzero(np.isfinite(distance)):
        keep = np.arange(len(raw)) != index
        model = fit_model(distance[keep], raw[keep], infinity_raw)
        if distance[index] > model.b_m:
            predicted[index] = float(model.predict(distance[index]))
    return predicted


def distance_label(value: float) -> str:
    if math.isinf(value):
        return "INF"
    return "" if math.isnan(value) else f"{value:.12g}"


def write_tables(output: Path, stem: str, points: list[Observation], model: Model,
                 predicted: np.ndarray, loo: np.ndarray, samples: int) -> None:
    inverse = model.inverse([point.raw for point in points])
    nearest = min(point.distance_m for point in points)
    with (output / f"{stem}_points.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["source_line", "distance_m", "inverse_distance_1_per_m", "raw_measured",
                         "raw_fitted", "residual_raw_fit_minus_measured", "distance_from_measured_raw_m",
                         "inverse_within_model_domain", "loo_raw", "loo_residual_raw"])
        for i, point in enumerate(points):
            valid_inverse = bool(not np.isnan(inverse[i]) and inverse[i] >= nearest)
            writer.writerow([point.line, distance_label(point.distance_m), f"{1.0 / point.distance_m:.12g}",
                             f"{point.raw:.12g}", f"{predicted[i]:.12g}", f"{predicted[i] - point.raw:.12g}",
                             distance_label(float(inverse[i])), valid_inverse,
                             f"{loo[i]:.12g}" if np.isfinite(loo[i]) else "",
                             f"{loo[i] - point.raw:.12g}" if np.isfinite(loo[i]) else ""])
    q = np.linspace(0.0, 1.0 / nearest, samples)
    distance = np.full_like(q, np.inf)
    np.divide(1.0, q, out=distance, where=q != 0)
    curve = model.predict(distance)
    with (output / f"{stem}_curve.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["distance_m", "inverse_distance_1_per_m", "raw_fitted", "raw_nearest_integer"])
        for d, reciprocal, value in zip(distance, q, curve):
            writer.writerow([distance_label(float(d)), f"{reciprocal:.12g}", f"{value:.12g}", int(np.rint(value))])


def plot_result(output: Path, stem: str, distance: np.ndarray, raw: np.ndarray,
                model: Model, predicted: np.ndarray, summary: dict) -> None:
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.spines.top": False, "axes.spines.right": False,
                         "axes.grid": True, "grid.alpha": 0.18, "svg.fonttype": "none"})
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), layout="constrained")
    fig.suptitle(f"Focus calibration: {stem}", fontsize=16, fontweight="bold")
    finite = np.isfinite(distance)
    nearest, farthest = float(np.min(distance[finite])), float(np.max(distance[finite]))
    d_curve = np.linspace(nearest, max(10.0, farthest * 1.12), 1200)
    blue, orange = "#2166ac", "#d68a1f"
    axes[0, 0].plot(d_curve, model.predict(d_curve), color=blue, label="Offset reciprocal fit")
    axes[0, 0].scatter(distance[finite], raw[finite], color=orange, s=35, zorder=3, label="Measurements")
    axes[0, 0].axhline(model.raw_infinity, color="#777777", ls=":", label="Infinity RAW")
    axes[0, 0].set(xlabel="Object distance d (m)", ylabel="Focus position (RAW)", title="Distance to RAW")
    axes[0, 0].legend(fontsize=9)
    q = 1.0 / distance
    q_curve = np.linspace(0.0, 1.0 / nearest, 1200)
    d_inverse = np.full_like(q_curve, np.inf)
    np.divide(1.0, q_curve, out=d_inverse, where=q_curve != 0)
    axes[0, 1].plot(q_curve, model.predict(d_inverse), color=blue)
    axes[0, 1].scatter(q, raw, color=orange, s=35, zorder=3)
    if np.any(~finite):
        axes[0, 1].scatter(q[~finite], raw[~finite], color="#313131", marker="*", s=130, zorder=4, label="Infinity (1/d = 0)")
        axes[0, 1].legend(fontsize=9)
    axes[0, 1].set(xlabel="Inverse object distance 1/d (1/m)", ylabel="Focus position (RAW)", title="Infinity included without a finite-distance substitute")
    error = predicted - raw
    axes[1, 0].axhline(0, color="#777777", lw=1)
    axes[1, 0].scatter(q, error, color=orange, s=35, zorder=3)
    for i in np.argsort(np.abs(error))[-3:]:
        axes[1, 0].annotate(f"{distance_label(float(distance[i]))} m", (q[i], error[i]),
                          xytext=(5, 7), textcoords="offset points", fontsize=9)
    margin = max(50.0, float(np.max(np.abs(error))) * 0.3)
    axes[1, 0].set_ylim(float(np.min(error)) - margin, float(np.max(error)) + margin)
    axes[1, 0].set(xlabel="Inverse object distance 1/d (1/m)", ylabel="Fitted - measured (RAW)", title="Calibration residuals (all source rows retained)")
    for axis in (axes[0, 0], axes[0, 1], axes[1, 0]):
        axis.yaxis.set_major_formatter(StrMethodFormatter("{x:,.0f}"))
    axes[1, 1].axis("off")
    result = summary["finite_fit_metrics"]
    loo = summary["finite_loo_metrics"]
    r_squared = "n/a" if result["r_squared"] is None else f"{result['r_squared']:.6f}"
    notes = [
        "RAW = R_inf + A / (d - B)",
        f"R_inf = {model.raw_infinity:.6f} RAW",
        f"A = {model.a_raw_m:.6f} RAW m",
        f"B = {model.b_m:.9f} m  (effective offset)",
        f"Infinity: {'measured anchor' if model.infinity_anchored else 'estimated asymptote'}",
        f"Finite samples: {result['count']}",
        f"RMSE = {result['rmse_raw']:.2f} RAW",
        f"MAE = {result['mae_raw']:.2f} RAW",
        f"Max absolute error = {result['max_absolute_error_raw']:.2f} RAW",
        f"R-squared = {r_squared}",
        f"Finite leave-one-out RMSE = {loo['rmse_raw']:.2f} RAW" if loo else "Finite leave-one-out: unavailable",
        f"Model domain: d >= {nearest:g} m; infinity at 1/d = 0",
        "B is not an independently measured focal length.",
    ]
    axes[1, 1].text(0, 0.98, "Model and finite-point errors", va="top", fontsize=13, fontweight="bold")
    axes[1, 1].text(0, 0.88, "\n".join(notes), va="top", fontsize=10.5, linespacing=1.5)
    fig.savefig(output / f"{stem}_fit.png", dpi=180)
    fig.savefig(output / f"{stem}_fit.svg")
    plt.close(fig)


def write_report(output: Path, stem: str, points: list[Observation], model: Model,
                 predicted: np.ndarray, summary: dict) -> None:
    result = summary["finite_fit_metrics"]
    loo = summary["finite_loo_metrics"]
    offset_only = summary.get("reference_model") is not None
    anchor = ("固定参考曲线的距离项，用全部输入点等权估计整体偏差；无穷远 RAW 即该偏差" if offset_only else
              "CSV 中无穷远 RAW 的平均值，固定不参与自由优化" if model.infinity_anchored else "文件无 INF 行，无穷远 RAW 为拟合外推值")
    lines = [f"# {stem} 对焦校准拟合", "", "## 曲线", "", "距离 d 的单位为 m，RAW 为编码器位置计数。", "", "```text",
             f"RAW(d) = {model.raw_infinity:.12g} + ({model.a_raw_m:.12g}) / (d - ({model.b_m:.12g}))",
             f"d(RAW) = {model.b_m:.12g} + ({model.a_raw_m:.12g}) / (RAW - ({model.raw_infinity:.12g}))",
             "```", "", f"- 无穷远策略：{anchor}。",
             f"- 模型使用范围：d ≥ {summary['domain']['minimum_distance_m']:g} m；最远实测有限点 {summary['domain']['maximum_measured_finite_distance_m']:g} m。",
             "- 所有原始点均保留，以 RAW 误差等权最小二乘拟合，不自动剔除大残差点。",
             "- B 是拟合的有效距离偏置，可吸收测距基准与简化模型的影响，不等于独立测出的镜头焦距。",
             "", "## 方法", "", "采用用户给定的 RAW 与镜头伸出量线性关系。薄透镜方程 1/f = 1/u + 1/v 可得 v-f = f²/(u-f)，因此用带偏置的反比例曲线拟合 RAW。这是物理启发的近似校准模型，不将复合镜头或成像面测距关系当成完美薄透镜。",
             f"\n方程参考：[Edmund Optics: Basic Lens Selection]({REFERENCE})。",
             "\n优化先扫描 B，每个 B 下线性求解 A（无 INF 时同时求解 R∞），再做有界一维精细优化。B 约束为 [0, 0.95 × 最近实测距离]，避免校准范围内出现极点。INF 按 1/d=0 处理，不替换成某个人为大距离。",
             "", "## 有限距离点误差", "", f"- 样本数：{result['count']}",
             f"- RMSE：{result['rmse_raw']:.6f} RAW", f"- MAE：{result['mae_raw']:.6f} RAW",
             f"- 最大绝对残差：{result['max_absolute_error_raw']:.6f} RAW",
             f"- R²：{result['r_squared']:.9f}" if result['r_squared'] is not None else "- R²：不可定义",
             f"- 有限点留一评估 RMSE：{loo['rmse_raw']:.6f} RAW" if loo else "- 有限点留一评估：不可用，见 JSON 中 unavailable 数量。",
             ("\n留一评估只留出有限点，其余点用于估计整体偏差。" if offset_only else "\n固定的 INF 锚点不计入上述有限点精度指标；留一评估仅留出有限点，INF 锚点仍在训练集中。") + "这些是拟合残差，不代表实机合焦精度或重复性。",
             "", "## 逐点结果", "", "| 物距 m | 实测 RAW | 拟合 RAW | 拟合−实测 RAW |", "| ---: | ---: | ---: | ---: |"]
    for point, value in zip(points, predicted):
        lines.append(f"| {distance_label(point.distance_m)} | {point.raw:g} | {value:.3f} | {value - point.raw:+.3f} |")
    lines += ["", "## 输出与使用", "", "```bash", f"python fit_focus.py {summary['source']['filename']}", "```", "",
              "依赖：numpy、scipy、matplotlib。从任意目录调用脚本均可；参数可以是当前目录路径或 Cal 内的文件名。详细结果输出到 Result/设备序列号，简明公式输出到 Result/Formula；相同输入文件名重新运行会更新对应结果。文件名以 cal 加 12 位 MAC 开头时使用 MAC 作为序列号，其他文件名使用文件名作为文件夹名。",
              "", "- `*_fit.png` / `*_fit.svg`：曲线、倒距离图及残差图。",
              "- `*_model.json`：全精度参数、公式、源数据 SHA256、数值库版本和误差指标。",
              "- `*_points.csv`：原始行号、实测 / 拟合 RAW、残差及反算距离。反算距离可能超出模型域，由独立标志列标注，不自动裁剪为边界值。",
              "- `*_curve.csv`：均匀倒距离采样的拟合曲线，包含无穷远及整数 RAW。", "",
              "源 CSV 保持不变。脚本没有修改、构建或烧录主控固件。"]
    (output / f"{stem}_report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("filename", help="CSV path, or a filename in this script's Cal directory")
    parser.add_argument("--reference-model", help="Reference model JSON: fix its distance term and fit only the device offset")
    parser.add_argument("--points", type=int, default=1001, help="Number of dense curve samples (default: 1001)")
    args = parser.parse_args()
    if args.points < 2:
        parser.error("--points must be at least 2")
    try:
        source = resolve_input(args.filename)
        reference_path = resolve_input(args.reference_model) if args.reference_model else None
        reference = json.loads(reference_path.read_text(encoding="utf-8")) if reference_path else None
        if reference and reference.get("model", {}).get("kind") != "offset_reciprocal":
            raise ValueError("Reference JSON must contain an offset_reciprocal model.")
        points = read_observations(source, offset_only=reference is not None)
        distance = np.array([point.distance_m for point in points])
        raw = np.array([point.raw for point in points])
        finite = np.isfinite(distance)
        anchor = float(np.mean(raw[~finite])) if np.any(~finite) else None
        model = fit_offset(distance, raw, reference["model"]) if reference else fit_model(distance, raw, anchor)
        predicted = model.predict(distance)
        if reference:
            loo = np.full(len(raw), np.nan)
            for index in np.flatnonzero(finite):
                keep = np.arange(len(raw)) != index
                if np.any(keep):
                    loo[index] = float(fit_offset(distance[keep], raw[keep], reference["model"]).predict(distance[index]))
        else:
            loo = finite_loo(distance, raw, anchor)
        loo_unavailable = int(np.sum(~np.isfinite(loo[finite])))
        summary = {
            "source": {"filename": source.name, "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                       "distance_unit": "m", "raw_unit": "encoder count", "rows": len(points)},
            "model": {"kind": "offset_reciprocal", **asdict(model),
                      "forward": "RAW = raw_infinity + a_raw_m / (distance_m - b_m)",
                      "inverse": "distance_m = b_m + a_raw_m / (RAW - raw_infinity)",
                      "infinity_raw_count": int(np.sum(~finite)), "b_is_measured_focal_length": False},
            "domain": {"minimum_distance_m": float(np.min(distance[finite])),
                       "maximum_measured_finite_distance_m": float(np.max(distance[finite])),
                       "maximum_distance_m": None, "infinity_is_measured": model.infinity_anchored},
            "finite_fit_metrics": metrics(raw[finite], predicted[finite]),
            "all_rows_fit_metrics": metrics(raw, predicted),
            "finite_loo_metrics": metrics(raw[finite], loo[finite]) if loo_unavailable == 0 else None,
            "finite_loo_unavailable_count": loo_unavailable,
            "loo_method": "Hold out finite rows; refit offset from remaining rows." if reference else "Hold out finite rows only; retain measured infinity anchor in training.",
            "reference_model": {"filename": str(reference_path), "sha256": hashlib.sha256(reference_path.read_bytes()).hexdigest()} if reference else None,
            "fit_method": {"mode": "offset_only" if reference else "full", "objective": "unweighted squared RAW error",
                           "b_bounds_m": None if reference else [0.0, 0.95 * float(np.min(distance[finite]))]},
            "curve_sampling": {"coordinate": "inverse distance 1/d", "points": args.points,
                               "integer_rounding": "numpy.rint, ties to even"},
            "versions": {"python": sys.version.split()[0], "numpy": np.__version__,
                         "scipy": scipy.__version__, "matplotlib": matplotlib.__version__},
            "reference": REFERENCE,
        }
        stem = source.stem
        serial = device_id(stem)
        result_root = SCRIPT_DIR / "Result"
        output = result_root / serial
        formula_output = result_root / "Formula"
        output.mkdir(parents=True, exist_ok=True)
        formula_output.mkdir(parents=True, exist_ok=True)
        write_tables(output, stem, points, model, predicted, loo, args.points)
        plot_result(output, stem, distance, raw, model, predicted, summary)
        write_report(output, stem, points, model, predicted, summary)
        (output / f"{stem}_model.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        formula = (f"设备序列号：{serial}\n来源：{source.name}\n" + (f"距离项参考：{reference_path.name}\n" if reference else "距离项：本设备完整拟合，作为参考形状时需跨设备验证\n") + "距离 d 单位：m；位置：RAW\n"
                   f"RAW(d) = 整体偏差 + 距离相关项\n整体偏差 = {model.raw_infinity:.12g} RAW\n距离相关项 = ({model.a_raw_m:.12g}) / (d - ({model.b_m:.12g}))\n"
                   f"RAW(d) = {model.raw_infinity:.12g} + ({model.a_raw_m:.12g}) / (d - ({model.b_m:.12g}))\n"
                   f"d(RAW) = {model.b_m:.12g} + ({model.a_raw_m:.12g}) / (RAW - ({model.raw_infinity:.12g}))\n"
                   f"适用范围：d ≥ {summary['domain']['minimum_distance_m']:g} m；最远有限实测点 {summary['domain']['maximum_measured_finite_distance_m']:g} m\n"
                   "无穷远处 RAW = " + f"{model.raw_infinity:.12g}\n")
        (formula_output / f"{stem}_formula.txt").write_text(formula, encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError, csv.Error) as error:
        parser.error(str(error))
    result = summary["finite_fit_metrics"]
    print(f"Input: {source}\nRows: {len(points)} ({int(np.sum(finite))} finite, {int(np.sum(~finite))} infinity)")
    print(f"RAW(d) = {model.raw_infinity:.9g} + ({model.a_raw_m:.12g}) / (d - ({model.b_m:.12g}))")
    print(f"Finite RMSE: {result['rmse_raw']:.3f} RAW; max absolute residual: {result['max_absolute_error_raw']:.3f} RAW")
    print(f"Results: {output}\nFormula: {formula_output / (stem + '_formula.txt')}")


if __name__ == "__main__":
    main()
