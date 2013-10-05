; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!
;
; customer: ingvar_2006@ukr.net

#define YANDEX_VID_PREFIX 52
#define YANDEX_VID_INDEX 26

#include "FlylinkDC_Def.hss"

[Setup]
OutputBaseFilename=SetupFlylinkDC-Dnepr-Uzelok
WizardImageFile=vip_dnepr_uzelok\setup-1.bmp
WizardSmallImageFile=setup-2.bmp

[Components]
Name: "program"; Description: "Program Files"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus"; Description: "����-��������� �� ����"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus\vip_dnepr_uzelok"; Description: "������� �. ��������������, '������'"; Flags: exclusive

#include "FlylinkDC-x86.hss"
[Files]

Source: "vip_dnepr_uzelok\PortalBrowser\Uzelok.xml"; DestDir: "{app}\PortalBrowser\XMLUpdate";  Flags: ignoreversion; AfterInstall: MyAfterInstall
Source: "vip_dnepr_uzelok\PortalBrowser\icons_large.bmp"; DestDir: "{app}\PortalBrowser\uzelok"; Flags: ignoreversion
Source: "vip_dnepr_uzelok\PortalBrowser\icons_small.bmp"; DestDir: "{app}\PortalBrowser\uzelok"; Flags: ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_dnepr_uzelok\Favorites.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
Source: "vip_dnepr_uzelok\DCPlusPlus.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_dnepr_uzelok\IPTrust.ini"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#include "inc_finally.hss"