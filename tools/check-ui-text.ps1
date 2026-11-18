# Lists the English interface texts that have no Korean text in components\foo_groupplaylist\src\ui_text.h.
# It reads the dialog texts in foo_groupplaylist.rc, every text on a code line that calls Tr( or TrW(
# or ends with "// ui-text", and the field labels in pattern_tools.cpp.
# Built-in column names are checked by the unit tests. Exit code 1 means some text is missing.

$ErrorActionPreference = 'Stop'
$source = Join-Path (Split-Path -Parent $PSScriptRoot) 'components\foo_groupplaylist\src'
$literal = '"((?:[^"\\]|\\.)*)"'

$keys = @{}
foreach ($line in Get-Content -Encoding UTF8 (Join-Path $source 'ui_text.h')) {
	if ($line -match "^\s*\{ $literal,") { $keys[$Matches[1]] = $true }
}

$wanted = [ordered]@{}
function Want([string] $text, [string] $where) {
	if ($text -match '[A-Za-z]{2}' -and -not $wanted.Contains($text)) { $wanted[$text] = $where }
}

foreach ($line in Get-Content -Encoding UTF8 (Join-Path $source 'foo_groupplaylist.rc')) {
	if ($line -match "^\s*(LTEXT|RTEXT|CTEXT|PUSHBUTTON|DEFPUSHBUTTON|GROUPBOX|AUTOCHECKBOX|CONTROL)\s+$literal") {
		if ($Matches[2] -ne 'px') { Want $Matches[2] 'foo_groupplaylist.rc' }
	}
}

$skip = @('ui_text.h', 'ui_language.cpp', 'ui_language.h')
foreach ($file in Get-ChildItem $source -Include *.cpp, *.h -Recurse | Where-Object { $skip -notcontains $_.Name }) {
	foreach ($line in Get-Content -Encoding UTF8 $file.FullName) {
		if ($line -match '\bTrW?\(' -or $line -match '// ui-text\s*$') {
			foreach ($match in [regex]::Matches($line, $literal)) { Want $match.Groups[1].Value $file.Name }
		}
	}
}
foreach ($line in Get-Content -Encoding UTF8 (Join-Path $source 'pattern_tools.cpp')) {
	if ($line -match "^\s*\{ $literal,") { Want $Matches[1] 'pattern_tools.cpp' }
}

$missing = @($wanted.Keys | Where-Object { -not $keys.ContainsKey($_) })
foreach ($text in $missing) { Write-Output "No Korean text: `"$text`" ($($wanted[$text]))" }
if ($missing.Count -gt 0) { exit 1 }
Write-Output "ui text: all $($wanted.Count) English texts have a Korean text"
exit 0
