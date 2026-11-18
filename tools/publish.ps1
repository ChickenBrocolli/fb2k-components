# Copies the public part of this repository to the GitHub repository and pushes it.
#
# This folder is the private workshop: it also holds the development notes in docs\ and the full history.
# The public repository gets only the paths in $publicPaths, as they are in the last commit here (HEAD),
# as one new commit per publish. Its commits use the GitHub noreply address, so no personal e-mail is published.
#
#   tools\publish.ps1 -Message "foo_groupplaylist 0.9.0"
#
# The public copy lives in publish\fb2k-components (ignored by git here).

param(
	[Parameter(Mandatory)] [string] $Message,
	[string] $Remote = 'https://github.com/ChickenBrocolli/fb2k-components.git'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$public = Join-Path $repo 'publish\fb2k-components'
$authorName = 'ChickenBrocolli'
$authorEmail = '56687117+ChickenBrocolli@users.noreply.github.com'
$publicPaths = @('.gitignore', 'Directory.Build.targets', 'LICENSE', 'THIRD-PARTY-NOTICES.txt', 'README.md', 'README.ko.md', 'components', 'tools')

if (git -C $repo status --porcelain -- $publicPaths) { throw 'Some public files have changes that are not committed. Commit them first.' }

if (-not (Test-Path (Join-Path $public '.git'))) {
	New-Item -ItemType Directory -Force $public | Out-Null
	git -C $public init -q -b main
	git -C $public remote add origin $Remote
	if (git ls-remote --heads $Remote main) {
		git -C $public fetch -q origin main
		git -C $public reset -q --soft origin/main
	}
}
git -C $public config user.name $authorName
git -C $public config user.email $authorEmail

# Replace the public copy's files with the committed public files.
Get-ChildItem -Force $public | Where-Object { $_.Name -ne '.git' } | Remove-Item -Recurse -Force
$archive = Join-Path $env:TEMP 'fb2k-components-publish.zip'
git -C $repo archive --format=zip -o $archive HEAD -- $publicPaths
Expand-Archive -Path $archive -DestinationPath $public -Force
Remove-Item $archive

git -C $public add -A
if (-not (git -C $public status --porcelain)) {
	Write-Output 'Nothing changed since the last publish.'
	exit 0
}
git -C $public commit -q -m $Message
foreach ($address in (git -C $public log -1 --format='%ae%n%ce')) {
	if ($address -ne $authorEmail) { throw "The commit has the address $address. Nothing was pushed." }
}
git -C $public push -q -u origin main
Write-Output "Published: $(git -C $public log -1 --format='%h %s')"
