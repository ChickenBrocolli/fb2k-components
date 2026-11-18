param(
	[ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -products * -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio 2022 Build Tools.' }

& $msbuild (Join-Path $repo 'components\foo_groupplaylist\tests\group_model_tests.vcxproj') /p:Configuration=$Configuration /p:Platform=x64 /m /nologo /v:minimal '/clp:ErrorsOnly'
if ($LASTEXITCODE -ne 0) { throw "Test build failed with exit code $LASTEXITCODE." }
& (Join-Path $repo "build\tests\$Configuration\group_model_tests.exe")
exit $LASTEXITCODE
