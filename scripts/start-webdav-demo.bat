@echo off
setlocal
cd /d "%~dp0"
start "AllMusic test WebDAV" "%~dp0webdav-mock.exe" --root "%~dp0audio-fixtures" --port 8765
timeout /t 2 /nobreak >nul
start "" "%~dp0allmusic-player.exe"
endlocal
