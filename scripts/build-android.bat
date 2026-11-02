@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem Compile l'application Android (tablette) : interface React, puis APK (arm64 et armv7).
rem Prerequis : Node.js, Android Studio (SDK, NDK 28.1.13356709 et CMake 3.22.1 installes depuis
rem le SDK Manager), JUCE 8.0.12 dans ..\JUCE\juce-8.0.12-windows\JUCE (comme pour Windows).

setlocal
set "ROOT=%~dp0.."

if "%JAVA_HOME%"=="" if exist "C:\Program Files\Android\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
if "%JAVA_HOME%"=="" (
  echo Java introuvable : installer Android Studio, ou definir JAVA_HOME.
  exit /b 1
)

pushd "%ROOT%\ui" || exit /b 1
call npm install --no-audit --no-fund || exit /b 1
call npm run build || exit /b 1
popd

node "%ROOT%\scripts\gen-uidata.mjs" || exit /b 1

pushd "%ROOT%\android\Builds\Android" || exit /b 1
if not exist local.properties (
  if exist "%LOCALAPPDATA%\Android\Sdk" (
    rem chemin du SDK avec des barres obliques (format attendu par Gradle)
    set "SDKDIR=%LOCALAPPDATA%\Android\Sdk"
    call echo sdk.dir=%%SDKDIR:\=/%%> local.properties
  ) else (
    echo SDK Android introuvable : ouvrir le dossier android\Builds\Android dans Android Studio une premiere fois.
    popd
    exit /b 1
  )
)
call gradlew.bat assembleRelease_Release || (popd & exit /b 1)
popd

echo.
echo Termine. APK : android\Builds\Android\app\build\outputs\apk\release_\release\app-release_-release.apk
echo Installation sur la tablette (debogage USB active) : adb install -r ^<fichier .apk^>
endlocal
