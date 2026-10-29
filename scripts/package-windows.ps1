$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$stage = Join-Path $root 'dist/AllMusicPlayer-Windows-x64'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
cmake --install build/desktop-release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'CMake install failed' }
windeployqt --release --no-translations --no-compiler-runtime --qmldir ui "$stage/allmusic-player.exe"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }

# App-local VC runtime: no compiler/Qt installation needed on the user's PC.
$crt = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
if (!(Test-Path "$crt/msvcp140.dll")) { throw "VC143 runtime not found: $crt" }
Copy-Item "$crt/*.dll" $stage
Copy-Item docs/WINDOWS-TEST.md "$stage/README.md"
Copy-Item docs/DEPENDENCIES.md "$stage/DEPENDENCIES.md"
python scripts/collect-licenses.py build/license-sources "$stage/licenses"
if ($LASTEXITCODE -ne 0) { throw 'License collection failed' }
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
    $process = Start-Process "$stage/allmusic-player.exe" -ArgumentList '--smoke-test', "$root/build/package-smoke.png" -WorkingDirectory $stage -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Packaged app timed out' }
    if ($process.ExitCode -ne 0) { throw "Packaged app failed: $($process.ExitCode)" }
} finally {
    $env:PATH = $savedPath
}
Compress-Archive -Path "$stage/*" -DestinationPath dist/AllMusicPlayer-Windows-x64.zip -Force
