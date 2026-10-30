$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location (Split-Path $PSScriptRoot -Parent)
$stage = Join-Path (Get-Location) 'dist/WebDav001A-Windows-x64'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
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
python scripts/collect-licenses.py build/license-sources "$stage/licenses"
if ($LASTEXITCODE -ne 0) { throw 'License collection failed' }
@{
    commit = (& git rev-parse HEAD).Trim()
    qt = (& qmake -query QT_VERSION).Trim()
    run = $env:GITHUB_RUN_ID
    attempt = $env:GITHUB_RUN_ATTEMPT
    package = 'WebDAV loopback test kit'
} | ConvertTo-Json | Set-Content "$stage/BUILD-INFO.json"
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
