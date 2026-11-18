# Builds the Release DLL and packs it as dist\foo_groupplaylist-<version>.fb2k-component.
# A .fb2k-component file is a zip archive. foobar2000 64-bit loads the DLL from the x64 folder inside it,
# the same layout the official components use (32-bit DLL at the root, x64/ and arm64ec/ for the others).
# This component has only the x64 build. The license files sit at the root of the archive.

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot 'build-and-deploy.ps1') -Configuration Release -NoDeploy

$componentSource = Get-Content -Raw (Join-Path $repo 'components\foo_groupplaylist\src\component.cpp')
if ($componentSource -notmatch 'DECLARE_COMPONENT_VERSION\("[^"]*",\s*"([^"]+)"') { throw 'No version found in component.cpp.' }
$version = $Matches[1]

# Archive path -> file. Archive paths use forward slashes, as the zip format requires.
$contents = [ordered]@{
	'x64/foo_groupplaylist.dll' = Join-Path $repo 'build\Release\foo_groupplaylist.dll'
	'LICENSE.txt' = Join-Path $repo 'LICENSE'
	'THIRD-PARTY-NOTICES.txt' = Join-Path $repo 'THIRD-PARTY-NOTICES.txt'
}

$dist = Join-Path $repo 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$package = Join-Path $dist "foo_groupplaylist-$version.fb2k-component"
if (Test-Path $package) { Remove-Item -Force $package }
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::Open($package, [IO.Compression.ZipArchiveMode]::Create)
try {
	foreach ($entry in $contents.GetEnumerator()) {
		if ($entry.Key -notlike '*.txt') {
			[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $entry.Value, $entry.Key, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
			continue
		}
		# foobar2000 shows LICENSE.txt in a text box when the package is installed. The box needs Windows line breaks.
		$text = [IO.File]::ReadAllText($entry.Value) -replace "`r?`n", "`r`n"
		$writer = New-Object IO.StreamWriter(($zip.CreateEntry($entry.Key, [IO.Compression.CompressionLevel]::Optimal)).Open(), (New-Object Text.UTF8Encoding($false)))
		try { $writer.Write($text) } finally { $writer.Dispose() }
	}
} finally {
	$zip.Dispose()
}

Write-Output "Packed $package"
$check = [IO.Compression.ZipFile]::OpenRead($package)
try {
	$check.Entries | ForEach-Object { Write-Output "  $($_.FullName)  $($_.Length) bytes" }
} finally {
	$check.Dispose()
}
