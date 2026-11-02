@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem Double-clic : compile tout (comme COMPILER-ET-LANCER), puis fabrique l installateur
rem _livraisons\Groovebox-Installation-<version>.exe a envoyer a d autres PC.
rem Prerequis en plus de la compilation : Inno Setup 7 ou 6 (gratuit). S il manque, ce script
rem dit comment l installer.
title Groovebox - fabrication de l installateur
setlocal
set "ROOT=%~dp0"

rem 1. Compilation (interface, application, plugin VST3, tests)
call "%ROOT%scripts\build-windows.bat"
if errorlevel 1 goto :fail

rem 2. Version lue dans CMakeLists.txt
set "VER="
for /f "tokens=3" %%v in ('findstr /c:"project(Groovebox VERSION" "%ROOT%CMakeLists.txt"') do set "VER=%%v"
if not defined VER (
  echo Version introuvable dans CMakeLists.txt
  goto :fail
)

rem 3. Archive des sources de cette version (licence AGPL : elles accompagnent le programme)
if not exist "%ROOT%_livraisons" mkdir "%ROOT%_livraisons"
if not exist "%ROOT%_livraisons\groovebox-v%VER%.tar.gz" (
  echo Archive des sources groovebox-v%VER%.tar.gz absente de _livraisons : l installateur sera fait sans.
)

rem 4. Inno Setup 7 (ou 6) : version 7 cherchee d abord, puis 6
set "ISCC=%ProgramFiles%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo.
  echo Inno Setup n est pas installe. Telechargez-le sur la page officielle :
  echo     https://jrsoftware.org/isdl.php
  echo innosetup-7.x.x-x64.exe (recommande^), installez avec les choix par defaut,
  echo puis relancez CREER-INSTALLATEUR.bat
  goto :fail
)

rem 5. Composants Microsoft Visual C++ fournis avec Visual Studio (installes chez les collegues)
set "REDIST="
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%p in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%p"
if defined VSPATH for /f "delims=" %%r in ('dir /s /b "%VSPATH%\VC\Redist\MSVC\vc_redist.x64.exe" 2^>nul') do set "REDIST=%%r"
rem (sans bloc entre parentheses : un chemin contenant "(x86)" le casserait)
if not defined REDIST goto :noredist
echo Composants Visual C++ : "%REDIST%"
"%ISCC%" /DAppVersion=%VER% "/DVCRedist=%REDIST%" "%ROOT%scripts\installer\Groovebox.iss"
if errorlevel 1 goto :fail
goto :done
:noredist
echo Composants Visual C++ introuvables : l installateur sera fait sans eux.
"%ISCC%" /DAppVersion=%VER% "%ROOT%scripts\installer\Groovebox.iss"
if errorlevel 1 goto :fail

:done

echo.
echo Installateur pret : _livraisons\Groovebox-Installation-%VER%.exe
echo C est ce fichier qu il faut envoyer.
explorer /select,"%ROOT%_livraisons\Groovebox-Installation-%VER%.exe"
pause
exit /b 0

:fail
echo.
echo La fabrication a echoue. Envoie une capture de cette fenetre a Claude.
pause
exit /b 1
