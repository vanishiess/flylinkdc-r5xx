; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!
;
; customer: support@ionitcom.ru

#define YANDEX_VID_PREFIX 52
#define YANDEX_VID_INDEX 45

#include "FlylinkDC_Def.hss"

[Setup]
OutputBaseFilename=SetupFlylinkDC-ionit-telecom
WizardImageFile=vip_ionit\setup-1.bmp
WizardSmallImageFile=setup-2.bmp

[Components]
Name: "program"; Description: "Program Files"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus"; Description: "����-��������� �� ����"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus\vip_ionit"; Description: "���� '�����-�������'"; Flags: exclusive

#include "FlylinkDC-x86.hss"
[Files]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_ionit\Favorites.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
Source: "vip_ionit\DCPlusPlus.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


Source: "..\compiled\Settings\common\IPTrust.ini"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#include "inc_finally.hss"