# Writes the made-up albums used for the README screenshots to testdata\screenshots.
# Every name is invented and every cover is drawn here, so the screenshots hold nothing copyrighted.
# The audio is silence with realistic lengths; the files are sized at once, so writing them is quick (about 1.3 GB).
#
#   A  Morning Lights    one folder, one artist, one quality
#   B  City of Echoes    a main folder with Disc 1 and Disc 2 subfolders, one artist, one quality
#   C  Northern Suite    one album whose bonus tracks have a higher sample rate
#   D  Late Night Radio  one album with tracks by three artists
#
# Then, from PowerShell: & foobar2000_x64\foobar2000.exe /add testdata\screenshots

param([string] $OutDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'testdata\screenshots'))

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-Id3Frame([string] $id, [string] $text) {
	$body = [byte[]](@([byte]0) + [Text.Encoding]::GetEncoding(28591).GetBytes($text))
	$size = [BitConverter]::GetBytes([uint32]$body.Length)
	[array]::Reverse($size)
	return [Text.Encoding]::ASCII.GetBytes($id) + $size + [byte[]](0, 0) + $body
}

# The tags go in an ID3v2.3 chunk, because RIFF INFO has no disc number.
function New-Id3Chunk([hashtable] $tags) {
	$frames = (New-Id3Frame 'TIT2' $tags.Title) + (New-Id3Frame 'TPE1' $tags.Artist) + (New-Id3Frame 'TALB' $tags.Album) +
		(New-Id3Frame 'TYER' $tags.Date) + (New-Id3Frame 'TRCK' "$($tags.Track)") + (New-Id3Frame 'TCON' $tags.Genre)
	if ($tags.Disc) { $frames += New-Id3Frame 'TPOS' $tags.Disc }
	$n = $frames.Length
	$syncsafe = [byte[]]((($n -shr 21) -band 0x7F), (($n -shr 14) -band 0x7F), (($n -shr 7) -band 0x7F), ($n -band 0x7F))
	$tag = [Text.Encoding]::ASCII.GetBytes('ID3') + [byte[]](3, 0, 0) + $syncsafe + $frames
	if ($tag.Length % 2) { $tag += [byte]0 }
	return [Text.Encoding]::ASCII.GetBytes('id3 ') + [BitConverter]::GetBytes([uint32]$tag.Length) + $tag
}

# 16-bit stereo PCM of the given length, all zero.
function Write-SilentTrack([string] $path, [int] $sampleRate, [int] $seconds, [hashtable] $tags) {
	$channels = 2
	$dataLength = [uint32]($sampleRate * 2 * $channels * $seconds)
	$format = [BitConverter]::GetBytes([uint16]1) + [BitConverter]::GetBytes([uint16]$channels) +
		[BitConverter]::GetBytes([uint32]$sampleRate) + [BitConverter]::GetBytes([uint32]($sampleRate * 2 * $channels)) +
		[BitConverter]::GetBytes([uint16](2 * $channels)) + [BitConverter]::GetBytes([uint16]16)
	$head = [Text.Encoding]::ASCII.GetBytes('WAVE') +
		[Text.Encoding]::ASCII.GetBytes('fmt ') + [BitConverter]::GetBytes([uint32]$format.Length) + $format +
		(New-Id3Chunk $tags) +
		[Text.Encoding]::ASCII.GetBytes('data') + [BitConverter]::GetBytes($dataLength)
	$riff = [Text.Encoding]::ASCII.GetBytes('RIFF') + [BitConverter]::GetBytes([uint32]($head.Length + $dataLength)) + $head
	$stream = [IO.File]::Create($path)
	try {
		$stream.Write($riff, 0, $riff.Length)
		$stream.SetLength($riff.Length + $dataLength)
	} finally {
		$stream.Dispose()
	}
}

function Write-Cover([string] $folder, [string] $title, [string] $artist, [int[]] $from, [int[]] $to) {
	$size = 600
	$bitmap = New-Object Drawing.Bitmap $size, $size
	$graphics = [Drawing.Graphics]::FromImage($bitmap)
	$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAlias
	$rect = New-Object Drawing.Rectangle 0, 0, $size, $size
	$brush = New-Object Drawing.Drawing2D.LinearGradientBrush $rect, ([Drawing.Color]::FromArgb($from[0], $from[1], $from[2])), ([Drawing.Color]::FromArgb($to[0], $to[1], $to[2])), 45.0
	$graphics.FillRectangle($brush, $rect)
	# A few soft circles, so the covers do not look like plain swatches.
	for ($i = 0; $i -lt 5; $i++) {
		$circle = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(28, 255, 255, 255))
		$d = 160 + 70 * $i
		$graphics.FillEllipse($circle, $size - $d * 0.8, $size * 0.15 + 20 * $i, $d, $d)
		$circle.Dispose()
	}
	$titleFont = New-Object Drawing.Font 'Segoe UI Semibold', 44
	$artistFont = New-Object Drawing.Font 'Segoe UI', 26
	$layout = New-Object Drawing.RectangleF 40, 360, ($size - 80), 140
	$graphics.DrawString($title, $titleFont, [Drawing.Brushes]::White, $layout)
	$graphics.DrawString($artist, $artistFont, (New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(220, 255, 255, 255))), 44, 520)
	$graphics.Dispose()
	$bitmap.Save((Join-Path $folder 'cover.jpg'), [Drawing.Imaging.ImageFormat]::Jpeg)
	$bitmap.Dispose()
}

# Track: Title, Seconds, optional Artist, Rate, Disc (folder name and tag), Track number.
$albums = @(
	@{ Folder = '01 - Morning Lights'; Album = 'Morning Lights'; Artist = 'Marlow Tide'; Date = '2021'; Genre = 'Indie Pop'; From = 255, 170, 90; To = 200, 70, 110; Tracks = @(
		@{ Title = 'Window Seat'; Seconds = 214 },
		@{ Title = 'Coffee at Six (Remastered)'; Seconds = 187 },
		@{ Title = 'Paper Sun'; Seconds = 243 },
		@{ Title = 'Quiet Street'; Seconds = 198 },
		@{ Title = 'Late Bus [Live]'; Seconds = 266 },
		@{ Title = 'Morning Lights'; Seconds = 301 }) },
	@{ Folder = '02 - City of Echoes'; Album = 'City of Echoes'; Artist = 'Kestrel Avenue'; Date = '2019'; Genre = 'Alternative'; From = 40, 60, 120; To = 20, 150, 170; Tracks = @(
		@{ Title = 'Neon Rain'; Seconds = 232; Disc = '1/2'; Track = 1 },
		@{ Title = 'Overpass'; Seconds = 205; Disc = '1/2'; Track = 2 },
		@{ Title = 'Signal Lost'; Seconds = 251; Disc = '1/2'; Track = 3 },
		@{ Title = 'Subway Choir'; Seconds = 219; Disc = '1/2'; Track = 4 },
		@{ Title = 'Rooftops'; Seconds = 274; Disc = '1/2'; Track = 5 },
		@{ Title = 'After Hours'; Seconds = 246; Disc = '2/2'; Track = 1 },
		@{ Title = 'Static Hearts (Acoustic)'; Seconds = 193; Disc = '2/2'; Track = 2 },
		@{ Title = 'Harbor Wind'; Seconds = 228; Disc = '2/2'; Track = 3 },
		@{ Title = 'Echoes'; Seconds = 352; Disc = '2/2'; Track = 4 }) },
	@{ Folder = '03 - Northern Suite (Deluxe Edition)'; Album = 'Northern Suite (Deluxe Edition)'; Artist = 'Opal Signal'; Date = '2023'; Genre = 'Ambient'; From = 30, 90, 80; To = 110, 200, 170; Tracks = @(
		@{ Title = 'Prelude'; Seconds = 142 },
		@{ Title = 'Aurora'; Seconds = 288 },
		@{ Title = 'Frozen Lake'; Seconds = 256 },
		@{ Title = 'Polar Night'; Seconds = 311 },
		@{ Title = 'Thaw'; Seconds = 239 },
		@{ Title = 'Aurora (Hi-Res Mix)'; Seconds = 290; Rate = 96000 },
		@{ Title = 'Frozen Lake (Hi-Res Mix)'; Seconds = 258; Rate = 96000 },
		@{ Title = 'Polar Night (Hi-Res Mix)'; Seconds = 314; Rate = 96000 }) },
	@{ Folder = '04 - Late Night Radio'; Album = 'Late Night Radio'; Artist = 'Various Artists'; Date = '2024'; Genre = 'Soundtrack'; From = 70, 30, 110; To = 230, 90, 120; Tracks = @(
		@{ Title = 'Station Identification'; Seconds = 96; Artist = 'Northbound Choir' },
		@{ Title = 'Midnight Request'; Seconds = 224; Artist = 'Northbound Choir' },
		@{ Title = 'Lighthouse'; Seconds = 247; Artist = 'Northbound Choir' },
		@{ Title = 'Velvet Static'; Seconds = 213; Artist = 'June Carrow' },
		@{ Title = 'Two A.M. (Radio Edit)'; Seconds = 186; Artist = 'June Carrow' },
		@{ Title = 'Long Drive Home'; Seconds = 262; Artist = 'Harbor Kids' },
		@{ Title = 'Sign Off'; Seconds = 131; Artist = 'Harbor Kids' }) }
)

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
foreach ($album in $albums) {
	$albumFolder = Join-Path $OutDir $album.Folder
	New-Item -ItemType Directory -Force $albumFolder | Out-Null
	Write-Cover $albumFolder $album.Album $album.Artist $album.From $album.To
	$number = 0
	foreach ($track in $album.Tracks) {
		$number++
		$folder = $albumFolder
		if ($track.Disc) {
			$folder = Join-Path $albumFolder ('Disc ' + $track.Disc.Split('/')[0])
			if (-not (Test-Path $folder)) {
				New-Item -ItemType Directory $folder | Out-Null
				Copy-Item (Join-Path $albumFolder 'cover.jpg') $folder
			}
		}
		$trackNumber = if ($track.Track) { $track.Track } else { $number }
		$artist = if ($track.Artist) { $track.Artist } else { $album.Artist }
		$rate = if ($track.Rate) { $track.Rate } else { 44100 }
		$tags = @{ Title = $track.Title; Artist = $artist; Album = $album.Album; Date = $album.Date; Genre = $album.Genre; Track = $trackNumber; Disc = $track.Disc }
		$file = Join-Path $folder ('{0:D2} - {1}.wav' -f $trackNumber, ($track.Title -replace '[\\/:*?"<>|]', ''))
		Write-SilentTrack $file $rate $track.Seconds $tags
	}
}
Write-Output "Wrote $($albums.Count) albums to $OutDir"
