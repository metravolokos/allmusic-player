# Inspect the actual Qt FFmpeg DLL, record configuration, obtain its source tag.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$qtRoot = (& qmake -query QT_INSTALL_PREFIX).Trim()
$avutil = @(Get-ChildItem "$qtRoot/bin/avutil-*.dll")
if ($avutil.Count -ne 1) { throw 'Expected one Qt FFmpeg avutil DLL' }
$env:ALLMUSIC_AVUTIL = $avutil[0].FullName
python scripts/inspect-ffmpeg.py build/ffmpeg-info.json
if ($LASTEXITCODE -ne 0) { throw 'FFmpeg runtime inspection failed' }
$info = Get-Content build/ffmpeg-info.json -Raw | ConvertFrom-Json
Get-Content build/ffmpeg-info.json | Set-Content build/ffmpeg-info.txt
# Official Qt binary FFmpeg builds may have downstream changes: record exact
# configuration, and include Qt's source/attribution information alongside upstream.
$tag = "n$($info.release)"
git clone --depth 1 --branch $tag https://github.com/FFmpeg/FFmpeg.git build/license-sources/ffmpeg
if ($LASTEXITCODE -ne 0) { throw "Cannot obtain corresponding upstream FFmpeg $tag" }
