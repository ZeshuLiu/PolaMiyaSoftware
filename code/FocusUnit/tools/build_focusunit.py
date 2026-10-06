"""Build the complete Cortex-M0 firmware without changing generated makefiles.

PowerShell: python tools/build_focusunit.py --configuration Release
An STM32CubeIDE GNU Tools for STM32 installation or --toolchain is required.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def find_toolchain(explicit):
    if explicit:
        return Path(explicit)
    installed = Path("D:/Software/STMicroelectronics/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins")
    candidates = sorted(installed.glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3*/tools/bin"))
    if candidates:
        return candidates[-1]
    compiler = shutil.which("arm-none-eabi-gcc")
    if compiler:
        return Path(compiler).parent
    raise SystemExit("Use --toolchain <GNU Tools for STM32 bin directory>")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--toolchain")
    args = parser.parse_args()
    toolchain = find_toolchain(args.toolchain)
    suffix = ".exe" if os.name == "nt" else ""
    gcc = str(toolchain / ("arm-none-eabi-gcc" + suffix))
    out = ROOT / ".build" / args.configuration
    out.mkdir(parents=True, exist_ok=True)
    includes = ["Core/Inc", "Drivers/STM32F0xx_HAL_Driver/Inc",
                "Drivers/STM32F0xx_HAL_Driver/Inc/Legacy",
                "Drivers/CMSIS/Device/ST/STM32F0xx/Include", "Drivers/CMSIS/Include"]
    # Match the CMake firmware: size optimization and whole-program LTO.
    # Debug retains g3 symbols, though inlining affects source-level stepping.
    common = ["-mcpu=cortex-m0", "-mthumb", "-mfloat-abi=soft", "-std=gnu11", "-Os",
              "-g3" if args.configuration == "Debug" else "-g", "-DUSE_HAL_DRIVER",
              "-DSTM32F030x6", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra",
              "-fstack-usage", "-flto", "--specs=nano.specs"]
    if args.configuration == "Debug":
        common.append("-DDEBUG")
    common += ["-I" + str(ROOT / path) for path in includes]
    sources = sorted((ROOT / "Core/Src").rglob("*.c"))
    sources += sorted(p for p in (ROOT / "Drivers/STM32F0xx_HAL_Driver/Src").glob("*.c")
                      if "template" not in p.name)
    sources += sorted((ROOT / "Core/Startup").glob("*.s"))
    objects = []
    for source in sources:
        target = out / source.relative_to(ROOT).with_suffix(".o")
        target.parent.mkdir(parents=True, exist_ok=True)
        flags = ["-Wno-unused-parameter"] if "Drivers" in source.relative_to(ROOT).parts else ["-Werror"]
        subprocess.run([gcc, *common, *flags, "-c", str(source), "-o", str(target)], check=True)
        objects.append(target)
    elf = out / "FocusUnit.elf"
    map_file = out / "FocusUnit.map"
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + p.as_posix() + '"' for p in objects), encoding="utf-8")
    subprocess.run([gcc, "@" + str(response), "-mcpu=cortex-m0", "-mthumb", "-mfloat-abi=soft",
                    "-T" + str(ROOT / "STM32F030F4PX_FLASH.ld"), "--specs=nano.specs",
                    "--specs=nosys.specs", "-Os", "-flto", "-Wl,--gc-sections", "-Wl,--print-memory-usage",
                    "-Wl,-Map=" + str(map_file), "-Wl,--start-group", "-lc", "-lm",
                    "-Wl,--end-group", "-o", str(elf)], check=True)
    for fmt, extension in [("binary", "bin"), ("ihex", "hex")]:
        subprocess.run([str(toolchain / ("arm-none-eabi-objcopy" + suffix)), "-O", fmt,
                        str(elf), str(out / ("FocusUnit." + extension))], check=True)
    size = subprocess.check_output([str(toolchain / ("arm-none-eabi-size" + suffix)), str(elf)], text=True)
    print(size.strip())
    (out / "build_report.json").write_text(json.dumps({
        "configuration": args.configuration, "optimization": "-Os", "lto": True,
        "toolchain": str(toolchain),
        "sources": [p.relative_to(ROOT).as_posix() for p in sources], "size_output": size,
        "elf": str(elf), "map": str(map_file)
    }, indent=2), encoding="utf-8")
    print("Built:", elf)


if __name__ == "__main__":
    main()
