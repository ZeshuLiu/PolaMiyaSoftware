"""Run the actual firmware modules against native C HAL stubs (no board needed)."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
commands = [
    [sys.executable, "tests/adc/run_adc_tests.py"],
    [sys.executable, "tests/test_motor.py"],
    ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests/encoder/test_encoder.ps1"],
    ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tests/scheduler/test_scheduler.ps1"],
]
for command in commands:
    subprocess.run(command, cwd=ROOT, check=True)
print("All four Focus Unit module logic tests passed.")
