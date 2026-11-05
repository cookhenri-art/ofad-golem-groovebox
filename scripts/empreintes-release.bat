@echo off
rem Groovebox - OFAD - AGPL-3.0-or-later
rem v0.46.1 (audit F4) : ecrit _livraisons\SHA256SUMS.txt, l'empreinte SHA-256 de chaque fichier a
rem publier (installateur .exe, APK, archives Mac .zip, notices .pdf, sources .tar.gz).
rem Ce fichier est a joindre a CHAQUE release GitHub, en meme temps que les fichiers qu'il decrit :
rem les utilisateurs peuvent ainsi verifier que leur telechargement n'a pas ete modifie.
rem Format compatible avec "sha256sum -c" (Linux) et "shasum -a 256 -c" (Mac).
rem Usage : scripts\empreintes-release.bat            (dossier _livraisons)
rem         scripts\empreintes-release.bat <dossier>  (autre dossier, ex. fichiers telecharges du Mac)
setlocal
set "ROOT=%~dp0.."
set "DIR=%~1"
if "%DIR%"=="" set "DIR=%ROOT%\_livraisons"
if not exist "%DIR%" (
  echo Dossier introuvable : %DIR%
  exit /b 1
)
rem contre-verification : le dossier passe par une variable d environnement (une apostrophe dans le
rem chemin ne peut plus casser ni detourner la commande PowerShell)
set "GOLEM_SUMS_DIR=%DIR%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$d = Resolve-Path -LiteralPath $env:GOLEM_SUMS_DIR;" ^
  "$f = Get-ChildItem -LiteralPath $d -File | Where-Object { $_.Extension -in '.exe','.apk','.zip','.ipa','.pdf','.gz','.dmg' } | Sort-Object Name;" ^
  "if (-not $f) { Write-Host 'Aucun fichier a decrire.'; exit 1 }" ^
  "$lines = foreach ($x in $f) { (Get-FileHash -LiteralPath $x.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $x.Name };" ^
  "[IO.File]::WriteAllLines((Join-Path $d 'SHA256SUMS.txt'), [string[]]$lines);" ^
  "$lines | ForEach-Object { Write-Host $_ }"
if errorlevel 1 exit /b 1
echo.
echo Empreintes ecrites : %DIR%\SHA256SUMS.txt (a joindre a la release GitHub)
exit /b 0
