#define MyAppName "ColorForge"

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#ifndef MyProjectDir
  #define MyProjectDir "."
#endif

#ifndef MyBuildDir
  #define MyBuildDir AddBackslash(MyProjectDir) + "build\RelWithDebInfo"
#endif

#ifndef MyOutputDir
  #define MyOutputDir AddBackslash(MyProjectDir) + "release"
#endif

#ifndef MyIconFile
  #define MyIconFile AddBackslash(MyProjectDir) + "assets\icon\ColorForgeIcon.ico"
#endif

[Setup]
AppId={{9D48F1B3-6E6D-4D4C-94E8-77E92014C2A2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=ColorForge
DefaultDirName={code:GetObsDefaultDir}
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir={#MyOutputDir}
OutputBaseFilename=colorforge-windows-installer-v{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile={#MyIconFile}
UninstallDisplayIcon={app}\data\obs-plugins\obs-colorforge\ColorForgeIcon.ico

[Files]
Source: "{#MyBuildDir}\obs-colorforge.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#MyProjectDir}\data\effects\rgb-curves.effect"; DestDir: "{app}\data\obs-plugins\obs-colorforge\effects"; Flags: ignoreversion
Source: "{#MyProjectDir}\data\effects\hue-curves.effect"; DestDir: "{app}\data\obs-plugins\obs-colorforge\effects"; Flags: ignoreversion
Source: "{#MyProjectDir}\data\effects\color-range-correction.effect"; DestDir: "{app}\data\obs-plugins\obs-colorforge\effects"; Flags: ignoreversion
Source: "{#MyProjectDir}\assets\icon\ColorForgeIcon.png"; DestDir: "{app}\data\obs-plugins\obs-colorforge"; Flags: ignoreversion
Source: "{#MyProjectDir}\assets\icon\ColorForgeIcon.ico"; DestDir: "{app}\data\obs-plugins\obs-colorforge"; Flags: ignoreversion

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function IsObsFolder(Path: string): Boolean;
begin
  Result :=
    FileExists(AddBackslash(Path) + 'bin\64bit\obs64.exe') or
    DirExists(AddBackslash(Path) + 'obs-plugins\64bit');
end;

function GetObsDefaultDir(Value: string): string;
var
  Candidate: string;
begin
  Candidate := ExpandConstant('{autopf}\obs-studio');
  if IsObsFolder(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  Candidate := ExpandConstant('{autopf32}\obs-studio');
  if IsObsFolder(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  Result := ExpandConstant('{autopf}\obs-studio');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = wpSelectDir then begin
    if not IsObsFolder(WizardDirValue) then begin
      Result :=
        MsgBox(
          'The selected folder does not look like an OBS Studio installation.' + #13#10 +
          'ColorForge installs into the OBS root folder that contains "obs-plugins" and "data".' + #13#10#13#10 +
          'Continue anyway?',
          mbConfirmation,
          MB_YESNO
        ) = IDYES;
    end;
  end;
end;
