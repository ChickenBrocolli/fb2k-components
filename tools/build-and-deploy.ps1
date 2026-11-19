param(
	[ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release',
	[switch] $Start,
	# Checks and builds only, for tools\package.ps1.
	[switch] $NoDeploy
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repo 'components\foo_groupplaylist\src\foo_groupplaylist.vcxproj'
$foobarDir = Join-Path $repo 'foobar2000_x64_test'
$foobarExe = Join-Path $foobarDir 'foobar2000.exe'
$componentDir = Join-Path $foobarDir 'profile\user-components-x64\foo_groupplaylist'
$builtDll = Join-Path $repo "build\$Configuration\foo_groupplaylist.dll"

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -products * -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio 2022 Build Tools.' }

& (Join-Path $PSScriptRoot 'check-ui-text.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Some interface texts have no Korean text. Nothing was deployed.' }

& (Join-Path $PSScriptRoot 'run-tests.ps1') -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Unit tests failed. Nothing was deployed.' }

& $msbuild $project /p:Configuration=$Configuration /p:Platform=x64 /m /nologo /v:minimal '/clp:Summary;ErrorsOnly'
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE." }

if ($NoDeploy) { return }

$running = Get-Process foobar2000 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $foobarExe }
if ($running) {
	& $foobarExe /exit
	if (-not $running.WaitForExit(15000)) { throw 'The test foobar2000 did not exit within 15 seconds. Close it and run again.' }
}

New-Item -ItemType Directory -Force $componentDir | Out-Null
Copy-Item $builtDll $componentDir -Force
Write-Output "Deployed $builtDll to $componentDir"

if ($Start) { Start-Process $foobarExe }
