@echo off
rem [Claude 2026-10-31] Stops the live test dashboard.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dashboard.ps1" off
timeout /t 4 >nul
