; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!
;
; customer: rustam@panin.su


#define YANDEX_VID_PREFIX 52
#define YANDEX_VID_INDEX 87

#include "FlylinkDC_Def.hss"

[Setup]
OutputBaseFilename=SetupFlylinkDC-novosib-homenet
WizardImageFile=setup-1.bmp
WizardSmallImageFile=setup-2.bmp

[Components]
Name: "program"; Description: "Program Files"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus"; Description: "����-��������� �� ����"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus\vip_novosib_homenet"; Description: "�.����������� ���� 'HomeNet'"; Flags: exclusive

#include "FlylinkDC-x86.hss"
[Files]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_novosib_homenet\Favorites.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
Source: "vip_novosib_homenet\DCPlusPlus.xml"; DestDir: "{code:fUser}";  Flags: onlyifdoesntexist ignoreversion

Source: "vip_novosib_homenet\PortalBrowser\Magnetida.xml"; DestDir: "{app}\PortalBrowser\XMLUpdate";  Flags: ignoreversion; AfterInstall: MyAfterInstall
Source: "vip_novosib_homenet\PortalBrowser\Magnetida.ru\icons_large.bmp"; DestDir: "{app}\PortalBrowser\Magnetida.ru"; Flags: ignoreversion
Source: "vip_novosib_homenet\PortalBrowser\Magnetida.ru\icons_small.bmp"; DestDir: "{app}\PortalBrowser\Magnetida.ru"; Flags: ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


Source: "..\compiled\Settings\common\IPTrust.ini"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#include "inc_finally.hss"
