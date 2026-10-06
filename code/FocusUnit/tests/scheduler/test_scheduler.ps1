param([string]$VcVars = 'D:\Software\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskBuild = Join-Path $env:TEMP ('focus_scheduler_test_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskBuild | Out-Null
$taskExe = Join-Path $taskBuild 'test_scheduler.exe'
$taskObj = Join-Path $taskBuild 'test_scheduler.obj'
$taskSource = Join-Path $PSScriptRoot 'test_scheduler.c'
$taskStub = Join-Path $PSScriptRoot 'stub'
$taskInc = Join-Path $taskRoot 'Core\Inc'
if (!(Test-Path -LiteralPath $VcVars)) { throw "Missing MSVC environment: $VcVars" }
$taskCommand = 'call "' + $VcVars + '" >nul && cl /nologo /std:c11 /W4 /WX /I"' + $taskStub + '" /I"' + $taskInc + '" "' + $taskSource + '" /Fo"' + $taskObj + '" /Fe"' + $taskExe + '" && "' + $taskExe + '"'
& cmd.exe /d /s /c $taskCommand
if ($LASTEXITCODE -ne 0) { throw "Scheduler host test failed: $LASTEXITCODE" }
