/**
* �������� � ���������� "�������" / Page "Queue".
*/

/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"

#include "Resource.h"

#include "QueuePage.h"
#include "CommandDlg.h"

PropPage::TextItem QueuePage::texts[] =
{
	{ IDC_SETTINGS_SEGMENT, ResourceManager::SETTINGS_SEGMENT },
	{ IDC_SETTINGS_ANTI_FRAG_MAX, ResourceManager::SETTINGS_ANTI_FRAG_MAX },
	{ IDC_SETTINGS_ANTI_FRAG_FRAME, ResourceManager::SETTINGS_ANTI_FRAG_FRAME },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_AUTOSEGMENT, ResourceManager::SETTINGS_AUTO_SEARCH },
	{ IDC_DONTBEGIN, ResourceManager::DONT_ADD_SEGMENT_TEXT },
	{ IDC_MULTISOURCE, ResourceManager::ENABLE_MULTI_SOURCE },
	{ IDC_MINUTES, ResourceManager::MINUTES },
	{ IDC_KBPS, ResourceManager::KBPS },
	{ IDC_CHUNKCOUNT, ResourceManager::TEXT_MANUAL },
	
	{ IDC_ON_DOWNLOADING_SEGMENT, ResourceManager::ON_DOWNLOADING_SEGMENT },    // [+] SSA
	/*      // Add Combo
	    { IDC_ON_DOWNLOAD_ASK, ResourceManager::ON_DOWNLOAD_ASK },                  // [+] SSA
	    { IDC_ON_DOWNLOAD_REPLACE, ResourceManager::ON_DOWNLOAD_REPLACE },          // [+] SSA
	    { IDC_ON_DOWNLOAD_AUTORENAME, ResourceManager::ON_DOWNLOAD_AUTORENAME },    // [+] SSA
	    { IDC_ON_DOWNLOAD_SKIP, ResourceManager::ON_DOWNLOAD_SKIP },
	*/
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item QueuePage::items[] =
{
	{ IDC_AUTOSEGMENT, SettingsManager::AUTO_SEARCH, PropPage::T_BOOL },
	{ IDC_DONTBEGIN, SettingsManager::DONT_BEGIN_SEGMENT, PropPage::T_BOOL },
	{ IDC_BEGIN_EDIT, SettingsManager::DONT_BEGIN_SEGMENT_SPEED, PropPage::T_INT },
	{ IDC_AUTO_SEARCH_EDIT, SettingsManager::AUTO_SEARCH_TIME, PropPage::T_INT },
	{ IDC_CHUNKCOUNT, SettingsManager::SEGMENTS_MANUAL, PropPage::T_BOOL },
	{ IDC_SEG_NUMBER, SettingsManager::NUMBER_OF_SEGMENTS, PropPage::T_INT },
	{ IDC_SETTINGS_ANTI_FRAG, SettingsManager::ANTI_FRAG_MAX, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem QueuePage::optionItems[] =
{
	{ SettingsManager::MULTI_CHUNK, ResourceManager::ENABLE_MULTI_SOURCE },
	{ SettingsManager::AUTO_SEARCH_AUTO_MATCH, ResourceManager::SETTINGS_AUTO_SEARCH_AUTO_MATCH },
	{ SettingsManager::SKIP_ZERO_BYTE, ResourceManager::SETTINGS_SKIP_ZERO_BYTE },
//[-]PPA ����� ��������� � ��������� ���������� ����� (�������� DVD - �����)
// TODO ��������� ����������� ������ �����������! �������������� ������ ����������� ����� �� ��� ���������
//	{ SettingsManager::DONT_DL_ALREADY_SHARED, ResourceManager::SETTINGS_DONT_DL_ALREADY_SHARED },
	{ SettingsManager::ANTI_FRAG, ResourceManager::SETTINGS_ANTI_FRAG },
	// [-] merge
	//{ SettingsManager::ADVANCED_RESUME, ResourceManager::SETTINGS_ADVANCED_RESUME },
	{ SettingsManager::REALTIME_QUEUE_UPDATE, ResourceManager::QUEUE_UPDATING },
	//{ SettingsManager::ONLY_DL_TTH_FILES, ResourceManager::SETTINGS_ONLY_DL_TTH_FILES },
	{ SettingsManager::OVERLAP_CHUNKS, ResourceManager::OVERLAP_CHUNKS },
	{ SettingsManager::KEEP_FINISHED_FILES_OPTION, ResourceManager::KEEP_FINISHED_FILES_OPTION },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT QueuePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items, 0, 0);
	PropPage::read((HWND)*this, items, optionItems, GetDlgItem(IDC_OTHER_QUEUE_OPTIONS));
	
	ctrlList.Attach(GetDlgItem(IDC_OTHER_QUEUE_OPTIONS)); // [+] IRainman
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_SEG_NUMBER_SPIN));
	spin.SetRange32(1, 200);  //[!]PPA
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_AUTO_SEARCH_SPIN));
	spin.SetRange32(1, 60);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_BEGIN_SPIN));
	spin.SetRange32(2, 100000);
	spin.Detach();
	
	ctrlMultiSource.Attach(GetDlgItem(IDC_MULTISOURCE_COMBO));
	ctrlMultiSource.AddString(CTSTRING(DISABLED));
	ctrlMultiSource.AddString(CTSTRING(AUTOMATIC));
	ctrlMultiSource.SetCurSel(SETTING(MULTI_CHUNK));
	
	// Do specialized reading here
	
	// Add Combo: Download Action - if file exist
	downlaskClick.Attach(GetDlgItem(IDC_DOWNLOAD_ASK_COMBO));
	downlaskClick.AddString(CTSTRING(ON_DOWNLOAD_ASK));
	downlaskClick.AddString(CTSTRING(ON_DOWNLOAD_REPLACE));
	downlaskClick.AddString(CTSTRING(ON_DOWNLOAD_AUTORENAME));
	downlaskClick.AddString(CTSTRING(ON_DOWNLOAD_SKIP));
	downlaskClick.SetCurSel(0);         // Set default, if parameter not set in Settings
	
	switch (SETTING(ON_DOWNLOAD_SETTING))
	{
		case SettingsManager::ON_DOWNLOAD_ASK:
			//      CheckDlgButton(IDC_ON_DOWNLOAD_ASK, BST_CHECKED);
			downlaskClick.SetCurSel(0); // Add Combo
			break;
		case SettingsManager::ON_DOWNLOAD_REPLACE:
			//      CheckDlgButton(IDC_ON_DOWNLOAD_REPLACE, BST_CHECKED);
			downlaskClick.SetCurSel(1); // Add
			break;
		case SettingsManager::ON_DOWNLOAD_RENAME:
			//      CheckDlgButton(IDC_ON_DOWNLOAD_AUTORENAME, BST_CHECKED);
			downlaskClick.SetCurSel(2); // Add
			break;
		case SettingsManager::ON_DOWNLOAD_SKIP:
			//      CheckDlgButton(IDC_ON_DOWNLOAD_SKIP, BST_CHECKED);
			downlaskClick.SetCurSel(3); // Add
			break;
		default:                        // Add Combo
			downlaskClick.SetCurSel(0);
			break;
	}
	downlaskClick.Detach();             // Add Combo
	
	return TRUE;
}

void QueuePage::write()
{
	settings->set(SettingsManager::MULTI_CHUNK, ctrlMultiSource.GetCurSel());
	
	PropPage::write((HWND)*this, items, 0, 0);
	PropPage::write((HWND)*this, items, optionItems, GetDlgItem(IDC_OTHER_QUEUE_OPTIONS));
	
	int ct = SettingsManager::ON_DOWNLOAD_ASK;
	/*
	    if (IsDlgButtonChecked(IDC_ON_DOWNLOAD_ASK))
	        ct = SettingsManager::ON_DOWNLOAD_ASK;
	    else if (IsDlgButtonChecked(IDC_ON_DOWNLOAD_REPLACE))
	        ct = SettingsManager::ON_DOWNLOAD_REPLACE;
	    else if (IsDlgButtonChecked(IDC_ON_DOWNLOAD_AUTORENAME))
	        ct = SettingsManager::ON_DOWNLOAD_RENAME;
	    else if (IsDlgButtonChecked(IDC_ON_DOWNLOAD_SKIP))
	        ct = SettingsManager::ON_DOWNLOAD_SKIP;
	*/
	// Add Combo
	CComboBox downlaskClick;
	downlaskClick.Attach(GetDlgItem(IDC_DOWNLOAD_ASK_COMBO));
	switch (downlaskClick.GetCurSel())
	{
		case 0:
			ct = SettingsManager::ON_DOWNLOAD_ASK;
			break;
		case 1:
			ct = SettingsManager::ON_DOWNLOAD_REPLACE;
			break;
		case 2:
			ct = SettingsManager::ON_DOWNLOAD_RENAME;
			break;
		case 3:
			ct = SettingsManager::ON_DOWNLOAD_SKIP;
			break;
	}
	downlaskClick.Detach();
	// ~Add Combo
	
	settings->set(SettingsManager::ON_DOWNLOAD_SETTING, ct);
}