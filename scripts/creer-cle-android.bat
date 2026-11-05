@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem v0.46.1 (audit F5) : cree UNE SEULE FOIS la cle de publication Android de GOLEM.
rem   - la cle et son reglage sont ranges dans %USERPROFILE%\.ofad-golem (jamais dans le projet,
rem     jamais dans GitHub, jamais dans l'archive des sources) ;
rem   - build-android.bat l'utilise ensuite automatiquement pour signer l'APK ;
rem   - SAUVEGARDER golem-release.jks (cle USB) et le mot de passe (gestionnaire de mots de passe),
rem     SEPAREMENT : sans eux, plus aucune mise a jour ne s'installe par-dessus l'ancienne ; ensemble
rem     sur la meme cle USB, celui qui la trouve peut signer une fausse mise a jour.
rem   - keystore.properties contient le mot de passe en clair : il reste dans ce dossier, on ne le
rem     copie nulle part (contre-verification du 4 nov. 2026).
rem Le premier APK signe avec cette cle ne s'installe pas par-dessus un APK signe avec la cle de
rem debogage : il faut desinstaller l'ancien une fois (sauvegarder ses projets avant).
setlocal
set "DIR=%USERPROFILE%\.ofad-golem"
set "JKS=%DIR%\golem-release.jks"

if exist "%JKS%" (
  echo La cle existe deja : %JKS%
  echo Rien n'est modifie. Pensez a la sauvegarder.
  pause
  exit /b 0
)

if "%JAVA_HOME%"=="" if exist "C:\Program Files\Android\Android Studio\jbr\bin\keytool.exe" set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
if not exist "%JAVA_HOME%\bin\keytool.exe" (
  echo keytool introuvable : installer Android Studio, ou definir JAVA_HOME.
  pause
  exit /b 1
)

echo Mot de passe de la cle : 12 caracteres au moins, LETTRES ET CHIFFRES SEULEMENT
echo (les signes comme ^& %% ^^ ! casseraient ce script). Notez-le dans un gestionnaire de mots de passe.
set "PASS="
set /p "PASS=Mot de passe : "
if "%PASS%"=="" goto :court
set "CHECK=%PASS:~11,1%"
if "%CHECK%"=="" goto :court

if not exist "%DIR%" mkdir "%DIR%"
"%JAVA_HOME%\bin\keytool.exe" -genkeypair -v -keystore "%JKS%" -storetype PKCS12 -alias golem ^
  -keyalg RSA -keysize 4096 -validity 10000 -storepass "%PASS%" -keypass "%PASS%" ^
  -dname "CN=OFAD GOLEM, O=One For All Distribution, C=FR"
if errorlevel 1 (
  echo La creation de la cle a echoue.
  pause
  exit /b 1
)

set "JKSFWD=%JKS:\=/%"
> "%DIR%\keystore.properties" echo storeFile=%JKSFWD%
>> "%DIR%\keystore.properties" echo storePassword=%PASS%
>> "%DIR%\keystore.properties" echo keyAlias=golem
>> "%DIR%\keystore.properties" echo keyPassword=%PASS%
>> "%DIR%\keystore.properties" echo storeType=pkcs12

echo.
echo Cle creee : %JKS%
echo Reglage   : %DIR%\keystore.properties
echo.
echo A FAIRE MAINTENANT : copier golem-release.jks sur une cle USB, et noter le mot de passe dans un
echo gestionnaire de mots de passe (PAS sur la meme cle USB ; ne pas copier keystore.properties,
echo qui contient le mot de passe en clair). Empreinte du certificat (a garder) :
"%JAVA_HOME%\bin\keytool.exe" -list -v -keystore "%JKS%" -storepass "%PASS%" 2>nul | findstr /c:"SHA256"
set "PASS="
pause
exit /b 0

:court
echo Mot de passe trop court (12 caracteres au moins).
pause
exit /b 1
