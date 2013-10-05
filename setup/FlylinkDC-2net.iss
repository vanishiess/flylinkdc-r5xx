; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!
;
; customer: amfetamin@pentagonu.net, admin@2free.ru, amfetamin@2net.su

#define YANDEX_VID_PREFIX 52
#define YANDEX_VID_INDEX 2

#include "FlylinkDC_Def.hss"

[Setup]
OutputBaseFilename=SetupFlylinkDC-2net
WizardImageFile=setup-1.bmp
WizardSmallImageFile=setup-2.bmp

[Components]
Name: "program"; Description: "Program Files"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus"; Description: "����-��������� �� ����"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus\vip_2net"; Description: "�. �������, ���������� ������� (Schelkovo-NET)"; Flags: exclusive

#include "FlylinkDC-x86.hss"
[Files]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_2net\Favorites.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
Source: "vip_2net\DCPlusPlus.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Source: "..\compiled\Settings\common\IPTrust.ini"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#include "inc_finally.hss"