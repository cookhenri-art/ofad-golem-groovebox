; Groovebox — OFAD — AGPL-3.0-or-later
; Installateur Windows (Inno Setup 6). Ne pas lancer à la main : CREER-INSTALLATEUR.bat
; compile d'abord l'application et le plugin, puis appelle ce script avec la version.
; Installe : l'application (Programmes), le plugin VST3 (dossier commun des VST3, au choix),
; les composants Microsoft Visual C++ s'ils sont fournis, raccourcis, désinstallation, sources.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define Root "..\.."
#define Artefacts Root + "\build\Groovebox_artefacts\Release"

[Setup]
AppId={{82198D1F-D019-4E9C-9136-17E55DD2F681}
AppName=Groovebox
AppVersion={#AppVersion}
AppVerName=Groovebox {#AppVersion}
AppPublisher=One For All Distribution
VersionInfoCompany=One For All Distribution
VersionInfoProductName=Groovebox
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\One For All Distribution\Groovebox
DefaultGroupName=Groovebox
DisableProgramGroupPage=yes
OutputDir={#Root}\_livraisons
OutputBaseFilename=Groovebox-Installation-{#AppVersion}
LicenseFile={#Root}\LICENSE
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\Groovebox.exe
; v0.46 fix6 : l'installateur porte la mascotte OFAD GOLEM (l'application, elle, l'a dans son .exe)
SetupIconFile={#Root}\resources\golem\golem.ico
UninstallDisplayName=Groovebox {#AppVersion}
CloseApplications=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "vst3"; Description: "Installer aussi le plugin VST3 (pour les logiciels de musique : Ableton, FL Studio, Reaper…)"; GroupDescription: "Plugin :"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Artefacts}\Standalone\Groovebox.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Artefacts}\VST3\Groovebox.vst3\*"; DestDir: "{commoncf64}\VST3\Groovebox.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vst3
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "LISEZ-MOI.txt"; DestDir: "{app}"; Flags: ignoreversion
; v0.46 fix9 : notice (bases de l'édition, raccourcis) et catalogue, avec la mascotte
Source: "{#Root}\docs\NOTICE.pdf"; DestDir: "{app}"; DestName: "Notice OFAD GOLEM.pdf"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#Root}\docs\CATALOGUE.pdf"; DestDir: "{app}"; DestName: "Catalogue OFAD GOLEM.pdf"; Flags: ignoreversion skipifsourcedoesntexist
; licence AGPL : les sources de cette version sont livrées avec le programme
Source: "{#Root}\_livraisons\groovebox-v{#AppVersion}.tar.gz"; DestDir: "{app}\Sources"; Flags: ignoreversion skipifsourcedoesntexist
#ifdef VCRedist
Source: "{#VCRedist}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\Groovebox"; Filename: "{app}\Groovebox.exe"
Name: "{group}\Lisez-moi"; Filename: "{app}\LISEZ-MOI.txt"
Name: "{group}\Notice et raccourcis"; Filename: "{app}\Notice OFAD GOLEM.pdf"
Name: "{group}\Catalogue des sons et effets"; Filename: "{app}\Catalogue OFAD GOLEM.pdf"
Name: "{group}\Désinstaller Groovebox"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Groovebox"; Filename: "{app}\Groovebox.exe"; Tasks: desktopicon

[Run]
#ifdef VCRedist
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installation des composants Microsoft Visual C++…"; Flags: waituntilterminated
#endif
Filename: "{app}\Groovebox.exe"; Description: "{cm:LaunchProgram,Groovebox}"; Flags: nowait postinstall skipifsilent

[Code]
// L'interface de Groovebox s'affiche avec Microsoft Edge WebView2, présent d'office sur
// Windows 11 et sur la plupart des Windows 10 à jour. S'il manque, on prévient.
function WebView2Present(): Boolean;
var
  v: String;
begin
  Result := False;
  if RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', v) then
    if (v <> '') and (v <> '0.0.0.0') then Result := True;
  if not Result then
    if RegQueryStringValue(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', v) then
      if (v <> '') and (v <> '0.0.0.0') then Result := True;
  if not Result then
    if RegQueryStringValue(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', v) then
      if (v <> '') and (v <> '0.0.0.0') then Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and (not WebView2Present()) and (not WizardSilent()) then
    MsgBox('Groovebox est installé, mais le composant Microsoft Edge WebView2 Runtime n''a pas été trouvé sur ce PC.'
      + #13#10#13#10 + 'Sans lui, la fenêtre de Groovebox reste vide. Installez « Microsoft Edge WebView2 Runtime » (gratuit, Microsoft) puis relancez Groovebox.',
      mbInformation, MB_OK);
end;
