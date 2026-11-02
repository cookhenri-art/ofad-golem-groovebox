# Groovebox — OFAD — AGPL-3.0-or-later
# Installe le paquet NuGet Microsoft.Web.WebView2 dans le dossier utilisateur,
# là où JUCE le cherche par défaut (%USERPROFILE%\AppData\Local\PackageManagement\NuGet\Packages).
# À lancer UNE fois, dans PowerShell (pas besoin d'être administrateur).

$ErrorActionPreference = "Stop"

$target = Join-Path $env:USERPROFILE "AppData\Local\PackageManagement\NuGet\Packages"
$existing = Get-ChildItem -Path $target -Directory -Filter "*Microsoft.Web.WebView2*" -ErrorAction SilentlyContinue

if ($existing) {
    Write-Host "WebView2 est déjà installé : $($existing[0].FullName)"
    exit 0
}

if (-not (Get-PackageSource -Name nugetRepository -ErrorAction SilentlyContinue)) {
    Register-PackageSource -ProviderName NuGet -Name nugetRepository -Location "https://www.nuget.org/api/v2" -Force | Out-Null
}

Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -Source nugetRepository -Force

Write-Host "WebView2 installé dans $target"
