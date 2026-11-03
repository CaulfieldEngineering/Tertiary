@echo off
rem [Claude 2026-10-31] Starts the live test dashboard in the background and opens it.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dashboard.ps1" on
timeout /t 4 >nul
