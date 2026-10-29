$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$stage = Join-Path $root 'dist/AllMusicPlayer-Windows-x64'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
cmake --install build/desktop-release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'CMake install failed' }
# Deploy the selected Desktop UI and the FFmpeg Multimedia backend.
windeployqt --release --no-translations --no-compiler-runtime --skip-plugin-types imageformats,iconengines --qmldir ui/desktop "$stage/allmusic-player.exe"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }

# Fail closed when Multimedia/plugin/FFmpeg deployment is incomplete.
foreach ($pattern in @('Qt6Multimedia.dll', 'multimedia/ffmpegmediaplugin.dll', 'avcodec-*.dll', 'avformat-*.dll', 'avutil-*.dll', 'swresample-*.dll', 'swscale-*.dll')) {
    if (!(Get-ChildItem (Join-Path $stage $pattern) -ErrorAction SilentlyContinue)) { throw "Missing deployed dependency: $pattern" }
}
# Deploy a console test runner too; it verifies PCM with developer paths removed.
Copy-Item build/desktop-release/playback-tests.exe $stage
windeployqt --release --no-translations --no-compiler-runtime --skip-plugin-types imageformats,iconengines "$stage/playback-tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Playback test deployment failed' }
Copy-Item build/desktop-release/streaming-tests.exe $stage
windeployqt --release --no-translations --no-compiler-runtime --skip-plugin-types imageformats,iconengines "$stage/streaming-tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Streaming test deployment failed' }
Copy-Item build/desktop-release/webdav-mock.exe $stage
windeployqt --release --no-translations --no-compiler-runtime --skip-plugin-types imageformats,iconengines "$stage/webdav-mock.exe"
if ($LASTEXITCODE -ne 0) { throw 'Loopback WebDAV mock deployment failed' }
Copy-Item build/audio-fixtures "$stage/audio-fixtures" -Recurse
Copy-Item docs/AUDIO-TEST.md "$stage/AUDIO-TEST.md"
Copy-Item build/ffmpeg-info.json "$stage/FFMPEG-INFO.json"
Copy-Item build/license-sources/ffmpeg/COPYING* "$stage/"
# Include the inspected upstream source plus Qt attribution/build instructions.
git -C build/license-sources/ffmpeg archive --format=zip --output "$stage/FFmpeg-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'FFmpeg source archive failed' }

Copy-Item build/license-sources/qt5/coin/provisioning "$stage/Qt-FFmpeg-build-scripts" -Recurse

# App-local VC runtime: no compiler/Qt installation needed on the user's PC.
$crt = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
if (!(Test-Path "$crt/msvcp140.dll")) { throw "VC143 runtime not found: $crt" }
Copy-Item "$crt/*.dll" $stage
Copy-Item docs/AUDIO-TEST.md "$stage/README.md"
Copy-Item docs/WEB-DAV-STREAM-TEST.md "$stage/WEB-DAV-STREAM-TEST.md"
Copy-Item scripts/start-webdav-demo.bat "$stage/start-webdav-demo.bat"
Copy-Item docs/DEPENDENCIES.md "$stage/DEPENDENCIES.md"
python scripts/collect-licenses.py build/license-sources "$stage/licenses"
if ($LASTEXITCODE -ne 0) { throw 'License collection failed' }
@{
    commit = (& git rev-parse HEAD).Trim()
    qt = (& qmake -query QT_VERSION).Trim()
    run = $env:GITHUB_RUN_ID
    attempt = $env:GITHUB_RUN_ATTEMPT
    physical_audio = 'NOT TESTED'
} | ConvertTo-Json | Set-Content "$stage/BUILD-INFO.json"
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $hash = Get-FileHash $_.FullName -Algorithm SHA256
    "$($hash.Hash)  $([IO.Path]::GetRelativePath($stage, $_.FullName))"
} | Set-Content "$stage/SHA256SUMS.txt"

# Launch the deployed copy with Qt/developer paths removed.
$savedPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = ''
    $env:QML_IMPORT_PATH = ''
    $env:QML2_IMPORT_PATH = ''
    $env:QT_QUICK_BACKEND = 'software'
    $env:QT_MEDIA_BACKEND = 'ffmpeg'
    $env:QT_FFMPEG_PROTOCOL_WHITELIST = 'file'
    $env:ALLMUSIC_FIXTURES = "$stage/audio-fixtures"
    & "$stage/playback-tests.exe" -o "$root/build/packaged-playback.txt,txt"
    if ($LASTEXITCODE -ne 0) { throw 'Packaged PCM decoding tests failed' }
    & "$stage/streaming-tests.exe" -o "$root/build/packaged-streaming.txt,txt"
    if ($LASTEXITCODE -ne 0) { throw 'Packaged WebDAV streaming tests failed' }
    $process = Start-Process "$stage/allmusic-player.exe" -ArgumentList '--smoke-test', "$root/build/package-smoke.png" -WorkingDirectory $stage -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Packaged app timed out' }
    if ($process.ExitCode -ne 0) { throw "Packaged app failed: $($process.ExitCode)" }
} finally {
    $env:PATH = $savedPath
}
Compress-Archive -Path "$stage/*" -DestinationPath dist/AllMusicPlayer-Windows-x64.zip -Force

