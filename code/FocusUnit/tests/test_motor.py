import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
compiler = shutil.which("gcc") or shutil.which("clang")
with tempfile.TemporaryDirectory() as temp:
    exe = Path(temp) / "test_motor.exe"
    sources = [str(ROOT / "tests/motor/test_motor.c")]
    includes = [str(ROOT / "tests/motor/stub"), str(ROOT / "Core/Inc")]
    if compiler:
        cmd = [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror"]
        for include in includes:
            cmd.extend(["-I", include])
        cmd.extend(sources + ["-o", str(exe)])
        subprocess.run(cmd, check=True)
        subprocess.run([str(exe)], check=True)
    else:
        vcvars = Path(r"D:\Software\VisualStudio\VC\Auxiliary\Build\vcvars64.bat")
        if not vcvars.exists():
            raise SystemExit("A native gcc/clang or the configured Visual Studio C compiler is required")
        quoted_includes = " ".join(f'/I "{p}"' for p in includes)
        quoted_sources = " ".join(f'"{p}"' for p in sources)
        batch = Path(temp) / "run_motor_tests.bat"
        batch.write_text(
            f'@call "{vcvars}" >nul\n'
            f'@if errorlevel 1 exit /b %errorlevel%\n'
            f'@cl /nologo /TC /std:c11 /W4 /WX {quoted_includes} '
            f'{quoted_sources} /Fe:"{exe}"\n'
            f'@if errorlevel 1 exit /b %errorlevel%\n'
            f'@"{exe}"\n', encoding="ascii")
        subprocess.run(["cmd.exe", "/d", "/c", str(batch)], check=True, cwd=temp)
print("motor host tests passed")
