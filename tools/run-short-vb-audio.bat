@echo off
setlocal
set "ROOT=%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\test-vb-audio.ps1" -Profile short -InputDevice 0 -OutputDevice 1
