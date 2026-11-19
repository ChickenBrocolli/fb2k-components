param([switch] $Compilation)

# Adds the generated test tracks to the active playlist of the test foobar2000.
# Run it from PowerShell. Git Bash rewrites the /add option into a file path,
# and foobar2000 then replaces the playlist and starts playback.

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$foobarExe = Join-Path $repo 'foobar2000_x64_test\foobar2000.exe'

& $foobarExe /add (Join-Path $repo 'testdata\tracks')
if ($Compilation) {
	Start-Sleep -Seconds 3
	& $foobarExe /add (Join-Path $repo 'testdata\compilation')
}
