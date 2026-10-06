$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outDir = Join-Path $env:TEMP ('focus_encoder_test_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outDir | Out-Null
$out = Join-Path $outDir 'focus_encoder_test.exe'
$cl = 'D:\Software\VisualStudio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe'
$vcvars = 'D:\Software\VisualStudio\VC\Auxiliary\Build\vcvars64.bat'
$stub = Join-Path $PSScriptRoot 'stm32f0xx_hal.h'
$source = Join-Path $root 'Core\Src\FocusUnit\Encoder\focus_encoder.c'
$test = Join-Path $PSScriptRoot 'test_encoder.c'

if ((Test-Path -LiteralPath $cl) -and (Test-Path -LiteralPath $vcvars)) {
  $commandFile = Join-Path $outDir 'run_tests.cmd'
  @(
    '@echo off'
    "call `"$vcvars`" >nul"
    'if errorlevel 1 exit /b %errorlevel%'
    "cd /d `"$outDir`""
    "`"$cl`" /nologo /std:c11 /W4 /WX /I `"$PSScriptRoot`" /I `"$(Join-Path $root 'Core\Inc')`" `"$source`" `"$test`" /Fe:`"$out`""
    'if errorlevel 1 exit /b %errorlevel%'
    "`"$out`""
    'exit /b %errorlevel%'
  ) | Set-Content -LiteralPath $commandFile -Encoding Ascii
  & $env:ComSpec /d /c "`"$commandFile`""
  if ($LASTEXITCODE -ne 0) { throw "encoder MSVC compile or logic test failed (exit $LASTEXITCODE)" }
} elseif (Get-Command gcc -ErrorAction SilentlyContinue) {
  & gcc -std=c99 -Wall -Wextra -Werror -I $PSScriptRoot -I (Join-Path $root 'Core/Inc') $source $test -o $out
  if ($LASTEXITCODE -ne 0) { throw 'encoder test compile failed' }
  & $out
  if ($LASTEXITCODE -ne 0) { throw 'encoder logic test failed' }
} else {
  throw 'No host C compiler found. Install MSVC or gcc to run encoder logic tests.'
}
