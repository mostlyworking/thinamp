@echo off
rem Double-click to install Thinamp. Runs install.ps1 from this folder.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
pause
