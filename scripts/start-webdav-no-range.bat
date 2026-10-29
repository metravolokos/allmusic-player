@echo off
setlocal
cd /d "%~dp0"
start "AllMusic WebDAV (HTTP 200 fallback test)" "%~dp0webdav-mock.exe" --root "%~dp0audio-fixtures" --port 8765 --ignore-range
timeout /t 2 /nobreak >nul
start "" "%~dp0allmusic-player.exe"
endlocal
