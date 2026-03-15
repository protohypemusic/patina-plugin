; ================================================================
; Patina VST3 Plugin - Windows Installer
; Built with Inno Setup 6 (https://jrsoftware.org/isinfo.php)
;
; Usage:
;   1. Build Patina in Release mode:
;      cmake --build build --config Release
;   2. Compile this installer:
;      "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\windows\patina-installer.iss
;   3. Output: installer\output\Patina-1.0.0-Windows.exe
; ================================================================

#define MyAppName "Patina"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Max Pote"
#define MyAppURL "https://futureproofmusicschool.com"

[Setup]
AppId={{B7E3F4A2-9C1D-4E5F-A8B6-2D7C9E0F1A3B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\output
OutputBaseFilename=Patina-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full installation (VST3)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full custom; Flags: fixed

[Files]
; VST3 plugin bundle - the entire .vst3 folder
Source: "..\..\build\Patina_artefacts\Release\VST3\Patina.vst3\*"; DestDir: "{commoncf}\VST3\Patina.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Welcome to the {#MyAppName} Setup Wizard
WelcomeLabel2=This will install {#MyAppName} {#MyAppVersion} by {#MyAppPublisher} on your computer.%n%n{#MyAppName} is a lo-fi texture and degradation plugin for adding character to your productions.%n%nVST3 will be installed to the standard plugin directory so your DAW can find it automatically.

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Patina.vst3"
