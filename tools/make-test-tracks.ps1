param(
	[string] $OutDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'testdata'),
	[int] $CompilationAlbums = 150,
	[int] $CompilationTracksPerAlbum = 20
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-ToneBytes([int] $sampleRate, [double] $seconds, [double] $frequency) {
	$count = [int]($sampleRate * $seconds)
	$samples = New-Object byte[] ($count * 2)
	for ($i = 0; $i -lt $count; $i++) {
		$value = [int16](6000 * [math]::Sin(2 * [math]::PI * $frequency * $i / $sampleRate))
		[BitConverter]::GetBytes($value).CopyTo($samples, $i * 2)
	}
	return ,$samples
}

function New-InfoField([string] $id, [string] $text) {
	$bytes = [Text.Encoding]::UTF8.GetBytes($text) + [byte]0
	if ($bytes.Length % 2) { $bytes += [byte]0 }
	return [Text.Encoding]::ASCII.GetBytes($id) + [BitConverter]::GetBytes([uint32]$bytes.Length) + $bytes
}

function New-Id3Frame([string] $id, [string] $text) {
	$body = [byte[]](@([byte]0) + [Text.Encoding]::GetEncoding(28591).GetBytes($text))
	$size = [BitConverter]::GetBytes([uint32]$body.Length)
	[array]::Reverse($size)
	return [Text.Encoding]::ASCII.GetBytes($id) + $size + [byte[]](0, 0) + $body
}

# RIFF INFO has no disc number field, so an album with discs carries its tags in an ID3v2.3 chunk.
function New-Id3Chunk([hashtable] $tags) {
	$frames = (New-Id3Frame 'TIT2' $tags.Title) + (New-Id3Frame 'TPE1' $tags.Artist) + (New-Id3Frame 'TALB' $tags.Album) +
		(New-Id3Frame 'TYER' $tags.Date) + (New-Id3Frame 'TRCK' "$($tags.Track)") + (New-Id3Frame 'TPOS' $tags.Disc)
	$n = $frames.Length
	$syncsafe = [byte[]]((($n -shr 21) -band 0x7F), (($n -shr 14) -band 0x7F), (($n -shr 7) -band 0x7F), ($n -band 0x7F))
	$tag = [Text.Encoding]::ASCII.GetBytes('ID3') + [byte[]](3, 0, 0) + $syncsafe + $frames
	if ($tag.Length % 2) { $tag += [byte]0 }
	return [Text.Encoding]::ASCII.GetBytes('id3 ') + [BitConverter]::GetBytes([uint32]$tag.Length) + $tag
}

function Write-Track([string] $path, [int] $sampleRate, [int] $channels, [byte[]] $samples, [hashtable] $tags) {
	$info = [Text.Encoding]::ASCII.GetBytes('INFO') +
		(New-InfoField 'INAM' $tags.Title) +
		(New-InfoField 'IART' $tags.Artist) +
		(New-InfoField 'IPRD' $tags.Album) +
		(New-InfoField 'ICRD' $tags.Date) +
		(New-InfoField 'ITRK' "$($tags.Track)")
	$format = [BitConverter]::GetBytes([uint16]1) + [BitConverter]::GetBytes([uint16]$channels) +
		[BitConverter]::GetBytes([uint32]$sampleRate) + [BitConverter]::GetBytes([uint32]($sampleRate * 2 * $channels)) +
		[BitConverter]::GetBytes([uint16](2 * $channels)) + [BitConverter]::GetBytes([uint16]16)
	$tagChunk = if ($tags.Disc) { New-Id3Chunk $tags } else { [Text.Encoding]::ASCII.GetBytes('LIST') + [BitConverter]::GetBytes([uint32]$info.Length) + $info }
	$body = [Text.Encoding]::ASCII.GetBytes('WAVE') +
		[Text.Encoding]::ASCII.GetBytes('fmt ') + [BitConverter]::GetBytes([uint32]$format.Length) + $format +
		$tagChunk +
		[Text.Encoding]::ASCII.GetBytes('data') + [BitConverter]::GetBytes([uint32]$samples.Length) + $samples
	[IO.File]::WriteAllBytes($path, [byte[]]([Text.Encoding]::ASCII.GetBytes('RIFF') + [BitConverter]::GetBytes([uint32]$body.Length) + $body))
}

function Write-Cover([string] $folder, [string] $label, [int] $index) {
	$bitmap = New-Object Drawing.Bitmap 300, 300
	$graphics = [Drawing.Graphics]::FromImage($bitmap)
	$hue = ($index * 47) % 360
	$color = [Drawing.Color]::FromArgb(255, [int](128 + 100 * [math]::Cos($hue * [math]::PI / 180)), [int](128 + 100 * [math]::Cos(($hue + 120) * [math]::PI / 180)), [int](128 + 100 * [math]::Cos(($hue + 240) * [math]::PI / 180)))
	$graphics.Clear($color)
	$font = New-Object Drawing.Font 'Arial', 90, ([Drawing.FontStyle]::Bold)
	$graphics.DrawString($label, $font, [Drawing.Brushes]::White, 20, 80)
	$graphics.Dispose()
	$bitmap.Save((Join-Path $folder 'cover.jpg'), [Drawing.Imaging.ImageFormat]::Jpeg)
	$bitmap.Dispose()
}

function Write-Album([string] $root, [int] $albumIndex, [hashtable] $album) {
	$folder = Join-Path $root ('{0:D3} - {1}' -f $albumIndex, $album.Album)
	New-Item -ItemType Directory -Force $folder | Out-Null
	Write-Cover $folder $album.Label $albumIndex
	$trackNumber = 0
	$fileIndex = 0
	foreach ($track in $album.Tracks) {
		$trackNumber++
		$rate = if ($track.Rate) { $track.Rate } else { 11025 }
		$channels = if ($track.Channels) { $track.Channels } else { 1 }
		if ($track.Track) { $trackNumber = $track.Track }
		$tags = @{ Title = $track.Title; Artist = $track.Artist; Album = $album.Album; Date = $album.Date; Track = $trackNumber; Disc = $track.Disc }
		$samples = $script:tones["$rate-$($album.Seconds)"]
		if ($channels -eq 2) { $samples = [byte[]]($samples + $samples) }
		$fileIndex++
		Write-Track (Join-Path $folder ('{0:D2} - {1}.wav' -f $fileIndex, $track.Title)) $rate $channels $samples $tags
	}
}

$script:tones = @{
	'11025-8' = New-ToneBytes 11025 8 330
	'22050-8' = New-ToneBytes 22050 8 330
	'11025-0.2' = New-ToneBytes 11025 0.2 330
}

$albums = @(
	@{ Album = 'Harbor Lights'; Label = 'HL'; Date = '2019'; Seconds = 8; Tracks = @(
		@{ Artist = 'North Pier'; Title = 'First Tide' },
		@{ Artist = 'North Pier'; Title = 'Lantern Row' },
		@{ Artist = 'Glass Ferry'; Title = 'Salt Line' },
		@{ Artist = 'Glass Ferry'; Title = 'Low Water' }) },
	@{ Album = 'Paper Maps'; Label = 'PM'; Date = '2021'; Seconds = 8; Tracks = @(
		@{ Artist = 'Tin Compass'; Title = 'Fold One' },
		@{ Artist = 'Tin Compass'; Title = 'Fold Two' },
		@{ Artist = 'Tin Compass'; Title = 'Legend' },
		@{ Artist = 'Tin Compass'; Title = 'Scale Bar' },
		@{ Artist = 'Tin Compass'; Title = 'Grid North' },
		@{ Artist = 'Tin Compass'; Title = 'Contour' },
		@{ Artist = 'Tin Compass'; Title = 'Margin Notes' },
		@{ Artist = 'Tin Compass'; Title = 'Last Sheet' }) },
	@{ Album = 'Night Shift'; Label = 'NS'; Date = '2023'; Seconds = 8; Tracks = @(
		@{ Artist = 'Quiet Dock'; Title = 'Clock In' },
		@{ Artist = 'Slow Crane'; Title = 'Break Room' },
		@{ Artist = 'Quiet Dock'; Title = 'Last Bus' },
		@{ Artist = 'Quiet Dock'; Title = 'Clock Out' }) },
	@{ Album = 'Mixed Rates'; Label = 'MR'; Date = '2024'; Seconds = 8; Tracks = @(
		@{ Artist = 'One Voice'; Title = 'Low Rate A' },
		@{ Artist = 'One Voice'; Title = 'Low Rate B' },
		@{ Artist = 'One Voice'; Title = 'High Rate'; Rate = 22050 },
		@{ Artist = 'One Voice'; Title = 'Low Rate C' }) },
	@{ Album = 'Mixed Bitrate'; Label = 'MB'; Date = '2025'; Seconds = 8; Tracks = @(
		@{ Artist = 'One Voice'; Title = 'Mono A' },
		@{ Artist = 'One Voice'; Title = 'Stereo'; Channels = 2 },
		@{ Artist = 'One Voice'; Title = 'Mono B' }) },
	@{ Album = 'Two Discs'; Label = 'TD'; Date = '2020'; Seconds = 8; Tracks = @(
		@{ Artist = 'Twin Reel'; Title = 'Side One Opener'; Disc = '1/2'; Track = 1 },
		@{ Artist = 'Twin Reel'; Title = 'Side One Middle'; Disc = '1/2'; Track = 2 },
		@{ Artist = 'Twin Reel'; Title = 'Side One Closer'; Disc = '1/2'; Track = 3 },
		@{ Artist = 'Twin Reel'; Title = 'Side Two Opener'; Disc = '2/2'; Track = 1 },
		@{ Artist = 'Twin Reel'; Title = 'Side Two Middle'; Disc = '2/2'; Track = 2 },
		@{ Artist = 'Twin Reel'; Title = 'Side Two Closer'; Disc = '2/2'; Track = 3 }) }
)

$small = Join-Path $OutDir 'tracks'
if (Test-Path $small) { Remove-Item $small -Recurse -Force }
$albumIndex = 0
foreach ($album in $albums) { $albumIndex++; Write-Album $small $albumIndex $album }
Write-Output "Wrote $albumIndex albums to $small"

if ($CompilationAlbums -gt 0) {
	$artists = 'Amber Row', 'Blue Kiln', 'Cedar Lane', 'Dune Radio', 'Echo Yard', 'Fern Motel', 'Grey Harbor'
	$large = Join-Path $OutDir 'compilation'
	if (Test-Path $large) { Remove-Item $large -Recurse -Force }
	for ($a = 1; $a -le $CompilationAlbums; $a++) {
		$tracks = for ($t = 1; $t -le $CompilationTracksPerAlbum; $t++) {
			@{ Artist = $artists[($a * 3 + $t * 5 + [int]($t / 4)) % $artists.Count]; Title = "Theme $t" }
		}
		Write-Album $large $a @{ Album = "Sound Collection Vol.$a"; Label = "$a"; Date = "$(1990 + $a % 30)"; Seconds = 0.2; Tracks = $tracks }
	}
	Write-Output "Wrote $CompilationAlbums compilation albums ($($CompilationAlbums * $CompilationTracksPerAlbum) tracks) to $large"
}
