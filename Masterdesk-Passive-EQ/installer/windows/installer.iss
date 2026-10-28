; Inno Setup script: Basemental MasterDesk Mastering Suite (VST3 + standalone), Windows x64
; Compile:  iscc /DAppVersion=1.0.0 /DArtefacts=..\..\build\PassiveMasterEQ_artefacts\Release /DOutDir=..\..\dist installer.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\build\PassiveMasterEQ_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif
#define AppName "MasterDesk Mastering Suite"
#define OldName "Masterdesk Passive EQ"

[Setup]
AppId={{6B8E3C1A-4F2D-4B7E-9A51-3C9D2E7F0B14}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Basemental
DefaultDirName={autopf}\Basemental\{#AppName}
DefaultGroupName=Basemental
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=MasterDesk-Mastering-Suite-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Basemental {#AppName}

[Types]
Name: "full";   Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Cubase, Ableton Live, Studio One, Reaper, FL Studio, Bitwig...)"; Types: full custom
Name: "app";  Description: "Standalone app (runs without a DAW)"; Types: full

[Files]
Source: "{#Artefacts}\VST3\{#AppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\{#AppName}.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[InstallDelete]
; copies installed under the old name (versions up to 1.3.0)
Type: filesandordirs; Name: "{commoncf64}\VST3\{#OldName}.vst3"
Type: filesandordirs; Name: "{autopf}\Basemental\{#OldName}"
Type: files; Name: "{autoprograms}\{#OldName}.lnk"

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Components: app

[Messages]
FinishedLabel=Setup has installed [name] on your computer.%n%nRescan plug-ins in your DAW, then insert MasterDesk Mastering Suite (by Basemental) on your master bus. Press HELP ? inside the plug-in for a guide to every control.
