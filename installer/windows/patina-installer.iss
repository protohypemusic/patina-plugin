; ============================================================
; Patina VST3 Plugin Installer — Windows
; Built with Inno Setup 6 (https://jrsoftware.org/isinfo.php)
;
; Compile:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" patina-installer.iss
;
; Output:
;   ../output/Patina-1.0.0-Windows.exe
; ============================================================

#define MyAppName      "Patina"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "Futureproof Music School"
#define MyAppURL       "https://futureproofmusicschool.com"

; Path to build artifacts (relative to this .iss file)
#define BuildRoot      "..\..\build\Patina_artefacts\Release"

[Setup]
AppId={{B7E3F1A2-4C5D-4E6F-8A9B-0C1D2E3F4A5B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={commoncf}\VST3
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=..\output
OutputBaseFilename=Patina-{#MyAppVersion}-Windows
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
CreateUninstallRegKey=yes
UninstallDisplayName={#MyAppName}

; --- Branding ---
WizardStyle=modern
WizardSizePercent=100
SetupIconFile=compiler:SetupClassicIcon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ============================================================
; Files to install — VST3 only
; ============================================================
[Files]
Source: "{#BuildRoot}\VST3\Patina.vst3\Contents\x86_64-win\Patina.vst3"; DestDir: "{commoncf}\VST3\Patina.vst3\Contents\x86_64-win"; Flags: ignoreversion
Source: "{#BuildRoot}\VST3\Patina.vst3\Contents\Resources\moduleinfo.json"; DestDir: "{commoncf}\VST3\Patina.vst3\Contents\Resources"; Flags: ignoreversion

; ============================================================
; Uninstall — clean up VST3 from common files
; ============================================================
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Patina.vst3"

; ============================================================
; Custom messages
; ============================================================
[Messages]
WelcomeLabel1=Welcome to the Patina Installer
WelcomeLabel2=This will install Patina {#MyAppVersion} on your computer.%n%nPatina is a lo-fi texture plugin by Futureproof Music School. It adds noise, wobble, distortion, space, flux, and filter effects to shape your sound.%n%nClick Next to continue.
FinishedHeadingLabel=Patina is installed!
FinishedLabel=Patina {#MyAppVersion} has been installed on your computer.%n%nVST3 location: C:\Program Files\Common Files\VST3\Patina.vst3%n%nOpen your DAW and rescan plugins to start using Patina.%n%nVisit futureproofmusicschool.com for tutorials and updates.
