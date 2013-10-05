; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!
;
; customer: itkeeper@mail.ru


#define YANDEX_VID_PREFIX 52
#define YANDEX_VID_INDEX 65

#include "FlylinkDC_Def.hss"

[Setup]
OutputBaseFilename=SetupFlylinkDC-Lipetsk-lan48
WizardImageFile=vip_lipetsk_media\setup-1.bmp
WizardSmallImageFile=setup-2.bmp

[Components]
Name: "program"; Description: "Program Files"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus"; Description: "����-��������� �� ����"; Types: full compact custom; Flags: fixed
Name: "DCPlusPlus\vip_lipetsk_media"; Description: "�.������ ��������� (www.lan48.ru)"; Flags: exclusive

#include "FlylinkDC-x86.hss"
[Files]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Source: "vip_lipetsk_media\Favorites.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
Source: "vip_lipetsk_media\DCPlusPlus.xml"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Source: "..\compiled\Settings\common\IPTrust.ini"; DestDir: "{code:fUser}"; Flags: onlyifdoesntexist ignoreversion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#include "inc_finally.hss"