$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location (Split-Path $PSScriptRoot -Parent)
$stage = Join-Path (Get-Location) 'dist/WebDav001A-Windows-x64'
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($name in @('webdav-tests', 'webdav-probe', 'webdav-mock')) {
    Copy-Item "build/desktop-release/$name.exe" $stage
    windeployqt --release --no-translations --no-compiler-runtime "$stage/$name.exe"
    if ($LASTEXITCODE -ne 0) { throw "Deployment failed: $name" }
}
$crt = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
Copy-Item "$crt/*.dll" $stage
Copy-Item docs/001-A-REPORT.md "$stage/README.md"
Copy-Item docs/DEPENDENCIES.md $stage
Copy-Item dist/AllMusicPlayer-Windows-x64/licenses "$stage/licenses" -Recurse
$savedPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = ''
    & "$stage/webdav-tests.exe" -o "${stage}/result.txt,txt"
    if ($LASTEXITCODE -ne 0) { throw 'Packaged WebDAV tests failed' }
} finally { $env:PATH = $savedPath }
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $hash = Get-FileHash $_.FullName -Algorithm SHA256
    "$($hash.Hash)  $([IO.Path]::GetRelativePath($stage, $_.FullName))"
} | Set-Content "$stage/SHA256SUMS.txt"
Compress-Archive -Path "$stage/*" -DestinationPath dist/WebDav001A-Windows-x64.zip -Force
