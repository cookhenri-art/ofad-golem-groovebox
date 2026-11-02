@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem Double-clic : compile tout (interface, application, plugin VST3), puis ouvre l application.
rem La premiere fois, compter 10 a 20 minutes.
title Groovebox - compilation
call "%~dp0scripts\build-windows.bat"
if errorlevel 1 (
  echo.
  echo La compilation a echoue. Envoie une capture de cette fenetre a Claude.
  pause
  exit /b 1
)
echo.
echo Ouverture de Groovebox...
start "" "%~dp0build\Groovebox_artefacts\Release\Standalone\Groovebox.exe"
timeout /t 5 >nul
