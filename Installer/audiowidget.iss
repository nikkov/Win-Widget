; This examples demonstrates how libwdi can be used in an installer script
; to automatically intall USB drivers along with your application.
;
; Requirements: Inno Setup (http://www.jrsoftware.org/isdl.php)
;
; To use this script, do the following:
; - configure libwdi (see config.h)
; - compile wdi-simple.exe
; - customize this script (application strings, wdi-simple.exe parameters, etc.)
; - open this script with Inno Setup
; - compile and run

[Languages]
Name: English; MessagesFile: compiler:Default.isl; LicenseFile: license.txt
Name: Russian; MessagesFile: compiler:Languages\Russian.isl; LicenseFile: license.txt

[Setup]
AppName = Audio-Widget driver
AppVerName = Audio-Widget driver 0.1.2.0
AppPublisher = Nikolay Kovbasa
AppPublisherURL = https://sites.google.com/site/nikkov/home
AppVersion = 0.1.2.0
DefaultDirName = {pf}\Audio-Widget
DefaultGroupName = Audio-Widget
OutputBaseFilename=AWSetup
Compression = lzma
SolidCompression = yes
; Win2000 or higher
MinVersion = 5.0
WizardSmallImageFile=ASIO.bmp

; This installation requires admin priviledges. This is needed to install
; drivers on windows vista and later.
PrivilegesRequired = admin

[Files]
; copy the 32bit wdi installer to the application directory.
; note: this installer also works with 64bit
;Source: "driver_installer.exe"; DestDir: "{app}"; Flags: replacesameversion promptifolder;
Source: "InstallDriver.exe"; DestDir: "{app}"; Flags: replacesameversion promptifolder
Source: "WidgetControl.exe"; DestDir: "{app}"; Flags: replacesameversion promptifolder
Source: "test.ini"; DestDir: "{app}"; Flags: replacesameversion promptifolder
Source: "WidgetTest.exe"; DestDir: "{app}"; Flags: replacesameversion promptifolder
;Source: "registerASIO.cmd"; DestDir: "{app}"; Flags: replacesameversion promptifolder
Source: "asiouac2.dll"; DestDir: "{app}"; Flags: promptifolder regserver replacesameversion
Source: "asiouac2debug.dll"; DestDir: "{app}"; Flags: promptifolder replacesameversion
;this file used only for registration ASIO driver before than user made first connect device
Source: "libusbK.dll"; DestDir: "{app}"; Flags: promptifolder replacesameversion

Source: "usb_driver\Audio-Widget.inf"; DestDir: "{app}\usb_driver"; Flags: promptifolder replacesameversion

Source: "usb_driver\x86\libusb0_x86.dll"; DestDir: "{app}\usb_driver\x86"; Flags: promptifolder replacesameversion
Source: "usb_driver\x86\libusbK_x86.dll"; DestDir: "{app}\usb_driver\x86"; Flags: promptifolder replacesameversion
Source: "usb_driver\x86\WdfCoInstaller01009.dll"; DestDir: "{app}\usb_driver\x86"; Flags: promptifolder replacesameversion
Source: "usb_driver\x86\libusbK.sys"; DestDir: "{app}\usb_driver\x86"; Flags: promptifolder replacesameversion

Source: "usb_driver\amd64\libusb0.dll"; DestDir: "{app}\usb_driver\amd64"; Flags: promptifolder replacesameversion
Source: "usb_driver\amd64\libusbK.dll"; DestDir: "{app}\usb_driver\amd64"; Flags: promptifolder replacesameversion
Source: "usb_driver\amd64\WdfCoInstaller01009.dll"; DestDir: "{app}\usb_driver\amd64"; Flags: promptifolder replacesameversion
Source: "usb_driver\amd64\libusbK.sys"; DestDir: "{app}\usb_driver\amd64"; Flags: promptifolder replacesameversion

[Icons]
Name: {group}\Widget-Control; Filename: {app}\WidgetControl.exe; WorkingDir: {app}; IconFilename: {app}\WidgetControl.exe; IconIndex: 0; Languages: 
Name: "{group}\Uninstall Audio-Widget"; Filename: "{uninstallexe}"


[Run]
; call driver_installer
;
; -n, --name <name>          set the device name
; -f, --inf <name>           set the inf name
; -m, --manufacturer <name>  set the manufacturer name
; -v, --vid <id>             set the vendor ID (VID)
; -p, --pid <id>             set the product ID (PID)
; -i, --iid <id>             set the interface ID (MI)
; -t, --type <driver_type>   set the driver to install
;                            (0=WinUSB, 1=libusb0, 2=libusbK, 3=custom)
; -d, --dest <dir>           set the extraction directory
; -x, --extract              extract files only (don't install)
; -c, --cert <certname>      install certificate <certname> from the
;                            embedded user files as a trusted publisher
;     --stealth-cert         installs certificate above without prompting
; -s, --silent               silent mode
; -b, --progressbar=[HWND]   display a progress bar during install
;                            an optional HWND can be specified
; -l, --log                  set log level (0 = debug, 4 = none)
; -h, --help                 display usage
;
;Filename: "{app}\driver_installer.exe"; Parameters: "--name ""Audio-Widget"" --vid 0x16C0 --pid 0x03E8 --progressbar={wizardhwnd}"; WorkingDir: "{app}"; Flags: runascurrentuser runhidden; StatusMsg: "Installing Audio-Widget driver (this may take a few seconds) ..."
Filename: "{app}\InstallDriver.exe"; WorkingDir: "{app}"; Flags: runascurrentuser; StatusMsg: "Installing Audio-Widget driver (this may take a few seconds) ..."; Tasks: Install_Drv; Languages: English
Filename: "{app}\InstallDriver.exe"; WorkingDir: "{app}"; Flags: runascurrentuser; StatusMsg: "Установка драйверов Audio-Widget (может занять некоторое время) ..."; Tasks: Install_Drv; Languages: Russian

;Filename: "{app}\registerASIO.cmd"; WorkingDir: "{app}"; Flags: runascurrentuser; Description: "Регистрация ASIO драйвера в системе"; Tasks: RegisterASIO_Drv; Languages: Russian
;Filename: "{app}\registerASIO.cmd"; WorkingDir: "{app}"; Flags: runascurrentuser; Description: "Register ASIO driver"; Tasks: RegisterASIO_Drv; Languages: English

[Tasks]
Name: Install_Drv; Description: Установка драйверов USB устройства; Flags: checkablealone; Languages: Russian
Name: Install_Drv; Description: Install USB drivers; Components: ; Flags: checkablealone; Languages: English
;Name: RegisterASIO_Drv; Description: Регистрация ASIO драйвера в системе; Flags: checkablealone; Languages: Russian
;Name: RegisterASIO_Drv; Description: Register ASIO driver; Components: ; Flags: checkablealone; Languages: English

