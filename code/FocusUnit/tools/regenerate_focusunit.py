"""Regenerate Core from IOC using the installed CubeMX without package downloads.

CubeMX generate-code mode uses Src/Inc regardless of CubeIDE Core layout.
Seed those staging folders with current Core files to preserve USER CODE, then
install only the known generated files. Modules, startup, Drivers, and IDE
metadata are never replaced. The staging directory and complete log are kept.
"""
# 旧集中式 Core 生成流程：先在临时目录生成、验证 USER CODE，再复制六个文件。
# 当前 CMake 工程已拆分 adc/tim/usart；本脚本仍在 main/MSP 中检查外设参数，
# 尚未适配拆分结构。本轮仅补注释，不运行生成或覆盖当前外设文件。
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLUGINS = Path("D:/Software/STMicroelectronics/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins")
FILES = ["Src/main.c", "Src/stm32f0xx_hal_msp.c", "Src/stm32f0xx_it.c",
         "Inc/main.h", "Inc/stm32f0xx_it.h", "Inc/stm32f0xx_hal_conf.h"]
USER = re.compile(r"/\* USER CODE BEGIN (.*?) \*/(.*?)/\* USER CODE END \1 \*/", re.S)


def user_sections(path):
    return dict(USER.findall(path.read_text(encoding="utf-8")))


def validate(stage):
    main = (stage / "Src/main.c").read_text(encoding="utf-8")
    msp = (stage / "Src/stm32f0xx_hal_msp.c").read_text(encoding="utf-8")
    irq = (stage / "Src/stm32f0xx_it.c").read_text(encoding="utf-8")
    required = ["ADC_SAMPLETIME_239CYCLES_5", "ADC_CLOCK_SYNC_PCLK_DIV4", "htim1.Init.Period = 2399;",
                "sConfigOC.Pulse = 2400;", "htim3.Init.Period = 65535;",
                "sConfig.IC1Filter = 4;", "sConfig.IC2Filter = 4;",
                "htim14.Init.Prescaler = 47;", "htim14.Init.Period = 999;",
                "hadc.Init.DiscontinuousConvMode = DISABLE;", "ADC_EOC_SINGLE_CONV"]
    for token in required:
        if token not in main:
            raise RuntimeError("Generated config mismatch: " + token)
    if "HAL_NVIC_EnableIRQ(TIM14_IRQn);" not in msp or "void TIM14_IRQHandler(void)" not in irq:
        raise RuntimeError("TIM14 IRQ was not generated")
    if "GPIO_PULLUP" not in msp:
        raise RuntimeError("Encoder pull-up configuration missing")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mx-dir", type=Path)
    parser.add_argument("--java", type=Path)
    args = parser.parse_args()
    mx = args.mx_dir or next(PLUGINS.glob("com.st.stm32cube.common.mx_6.14.1.*"))
    java = args.java or next(PLUGINS.glob("com.st.stm32cube.ide.jre.win64_*/jre/bin/java.exe"))
    stage = Path(tempfile.mkdtemp(prefix="focusunit-regenerate-"))
    home = stage / "java-home"
    home.mkdir()
    before = {}
    for relative in FILES:
        original = ROOT / "Core" / relative
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        before[relative] = user_sections(original)
    script = stage / "generate.mx"
    script.write_text('\n'.join([
        'config load "' + (ROOT / "FocusUnit.ioc").as_posix() + '"',
        'project couplefilesbyip 0',
        'generate code "' + stage.as_posix() + '"', 'exit', ''
    ]), encoding="ascii")
    log = stage / "cubemx.log"
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run([str(java), "-Duser.home=" + str(home),
            "--add-opens", "java.desktop/java.awt=ALL-UNNAMED",
            "--add-exports", "java.desktop/sun.awt=ALL-UNNAMED",
            "-jar", str(mx / "STM32CubeMX.jar"), "-q", str(script)],
            cwd=mx, stdout=output, stderr=subprocess.STDOUT)
    log_text = log.read_text(encoding="utf-8", errors="replace")
    if result.returncode != 0 or "Code succesfully generated" not in log_text or re.search(r"^KO$", log_text, re.M):
        raise RuntimeError("CubeMX generation failed; inspect " + str(log))
    validate(stage)
    for relative in FILES:
        after = user_sections(stage / relative)
        for name, content in before[relative].items():
            if content.strip() and after.get(name, "").strip() != content.strip():
                raise RuntimeError("USER CODE not preserved: " + relative + " / " + name)
    # All validation is complete before updating any project file.
    for relative in FILES:
        shutil.copy2(stage / relative, ROOT / "Core" / relative)
    print("CubeMX parameters and USER CODE verified. Updated six Core files.")
    print("Staging/log:", stage)


if __name__ == "__main__":
    main()
