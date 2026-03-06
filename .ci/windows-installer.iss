#define MyAppName "Chartchotic"
#define MyAppPublisher "Dichotic Studios"
#define MyAppURL "https://github.com/noahbaxter/chartchotic"

[Setup]
AppId={{E8A3B2C1-4D5F-6789-ABCD-EF0123456789}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={commonpf64}\Common Files\VST3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=Chartchotic-Windows-Installer
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableDirPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\Chartchotic.vst3\*"; DestDir: "{commonpf64}\Common Files\VST3\Chartchotic.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
SetupWindowTitle=Install {#MyAppName} v{#MyAppVersion}
WelcomeLabel2=This will install {#MyAppName} v{#MyAppVersion} VST3 plugin.%n%nThe plugin will be installed to:%n  C:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
