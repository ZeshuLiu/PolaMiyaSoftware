"""Build and run the ADC module's host-side HAL-stub tests."""

from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = Path(__file__).resolve().parent
CC = shutil.which("gcc") or shutil.which("clang")
MSVC = shutil.which("cl") or Path(
    r"D:\Software\VisualStudio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
)
MSVC_ENV = Path(r"D:\Software\VisualStudio\VC\Auxiliary\Build\vcvars64.bat")

with tempfile.TemporaryDirectory(prefix="focus-adc-test-") as tmp:
    tmp = Path(tmp)
    exe = tmp / "test_adc.exe"
    sources = [
        TEST_DIR / "test_adc.c",
        ROOT / "Core" / "Src" / "FocusUnit" / "Adc" / "focus_adc.c",
    ]
    includes = [TEST_DIR / "stubs", ROOT / "Core" / "Inc"]

    if CC:
        subprocess.run(
            [
                CC,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                *(f"-I{path}" for path in includes),
                *(str(path) for path in sources),
                "-o",
                str(exe),
            ],
            check=True,
        )
        subprocess.run([str(exe)], check=True)
    elif Path(MSVC).is_file() and MSVC_ENV.is_file():
        batch = tmp / "build_and_run.cmd"
        include_args = " ".join(f'/I"{path}"' for path in includes)
        source_args = " ".join(f'"{path}"' for path in sources)
        batch.write_text(
            "@echo off\r\n"
            f'call "{MSVC_ENV}" >nul\r\n'
            "if errorlevel 1 exit /b %errorlevel%\r\n"
            f'cl /nologo /std:c11 /W4 /WX {include_args} /Fe:"{exe}" '
            f"{source_args}\r\n"
            "if errorlevel 1 exit /b %errorlevel%\r\n"
            f'"{exe}"\r\n'
            "exit /b %errorlevel%\r\n",
            encoding="ascii",
        )
        subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=tmp, check=True)
    else:
        raise SystemExit("A host gcc, clang, or configured MSVC compiler is required.")
print("ADC host tests passed.")
