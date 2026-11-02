@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem Compile l'interface React puis le plugin VST3 et l'application autonome (Release).
rem Usage : scripts\build-windows.bat [chemin\vers\JUCE]
rem Prerequis : Node.js, Visual Studio (charge de travail C++), WebView2 (scripts\install-webview2.ps1).

setlocal
set "ROOT=%~dp0.."
set "JUCE_DIR=%~1"
if "%JUCE_DIR%"=="" set "JUCE_DIR=%ROOT%\..\JUCE\juce-8.0.12-windows\JUCE"

if not exist "%JUCE_DIR%\CMakeLists.txt" (
  echo JUCE introuvable : "%JUCE_DIR%"
  echo Passe le chemin du dossier JUCE en argument.
  exit /b 1
)

rem CMake : celui du dossier JUCE s'il existe, sinon celui du PATH.
set "CMAKE_BIN=%ROOT%\..\JUCE\cmake-4.3.0-rc2-windows-x86_64\cmake-4.3.0-rc2-windows-x86_64\bin"
if exist "%CMAKE_BIN%\cmake.exe" (
  set "CMAKE=%CMAKE_BIN%\cmake.exe"
  set "CTEST=%CMAKE_BIN%\ctest.exe"
) else (
  set "CMAKE=cmake"
  set "CTEST=ctest"
)

pushd "%ROOT%\ui" || exit /b 1
call npm install --no-audit --no-fund || exit /b 1
call npm run build || exit /b 1
popd

rem Pas de -G : CMake choisit la version de Visual Studio installee (2026 chez toi).
"%CMAKE%" -S "%ROOT%" -B "%ROOT%\build" -A x64 -DJUCE_DIR="%JUCE_DIR%" || exit /b 1
"%CMAKE%" --build "%ROOT%\build" --config Release --parallel || exit /b 1
"%CTEST%" --test-dir "%ROOT%\build" -C Release --output-on-failure || exit /b 1

echo.
echo Termine. Fichiers produits :
echo   Application autonome : build\Groovebox_artefacts\Release\Standalone\Groovebox.exe
echo   Plugin VST3          : build\Groovebox_artefacts\Release\VST3\Groovebox.vst3
echo Pour installer le VST3 : copier le dossier Groovebox.vst3 dans "C:\Program Files\Common Files\VST3\"
endlocal
