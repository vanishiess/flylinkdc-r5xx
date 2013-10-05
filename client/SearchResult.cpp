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

#include "stdinc.h"
#include "SearchResult.h"
#include "UploadManager.h"
#include "Text.h"
#include "User.h"
#include "ClientManager.h"
#include "Client.h"
#include "CFlylinkDBManager.h"
#include "ShareManager.h"

SearchResult::SearchResult(const UserPtr& aUser, Types aType, uint8_t aSlots, uint8_t aFreeSlots,
                           int64_t aSize, const string& aFile, const string& aHubName,
                           const string& aHubURL, const string& ip, const TTHValue& aTTH, const string& aToken) :
	file(aFile), hubName(aHubName), hubURL(aHubURL), user(aUser),
	size(aSize), type(aType), slots(aSlots), freeSlots(aFreeSlots), IP(ip),
	tth(aTTH), token(aToken),
	m_is_tth_share(false),
	m_is_tth_remembrance(false),
	m_is_tth_download(false),
	m_is_tth_check(false)
{
}

SearchResult::SearchResult(Types aType, int64_t aSize, const string& aFile, const TTHValue& aTTH) :
	file(aFile), user(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks()), size(aSize), type(aType), slots(UploadManager::getInstance()->getSlots()),
	freeSlots(UploadManager::getInstance()->getFreeSlots()),
	tth(aTTH),
	m_is_tth_remembrance(false),
	m_is_tth_download(false),
	m_is_tth_check(false)
{
	m_is_tth_share = aType == TYPE_FILE; // Constructor for ShareManager
}

void SearchResult::checkTTH()
{
	if (m_is_tth_check == false)
	{
		if (type == TYPE_FILE)
		{
			m_is_tth_share = ShareManager::getInstance()->isTTHShared(tth);
			if (m_is_tth_share == false)
			{
				const auto l_status_file = CFlylinkDBManager::getInstance()->get_status_file(tth);
				if (l_status_file & CFlylinkDBManager::PREVIOUSLY_DOWNLOADED)
					m_is_tth_download = true;
				if (l_status_file & CFlylinkDBManager::PREVIOUSLY_BEEN_IN_SHARE)
					m_is_tth_remembrance = true;
			}
			else
				m_is_tth_remembrance = true;
		}
		m_is_tth_check = true;
	}
}
string SearchResult::toSR(const Client& c) const
{
	// File:        "$SR %s %s%c%s %d/%d%c%s (%s)|"
	// Directory:   "$SR %s %s %d/%d%c%s (%s)|"
	string tmp;
	tmp.reserve(128);
	tmp.append("$SR ", 4);
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//	tmp.append(c.getMyNick());
//#else
	tmp.append(Text::fromUtf8(c.getMyNick(), c.getEncoding()));
//#endif
	tmp.append(1, ' ');
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//	const string& acpFile = file;
//#else
	const string acpFile = Text::fromUtf8(file, c.getEncoding());
//#endif
	if (type == TYPE_FILE)
	{
		tmp.append(acpFile);
		tmp.append(1, '\x05');
		tmp.append(Util::toString(size));
	}
	else
	{
		tmp.append(acpFile, 0, acpFile.length() - 1);
	}
	tmp.append(1, ' ');
	tmp.append(Util::toString(freeSlots));
	tmp.append(1, '/');
	tmp.append(Util::toString(slots));
	tmp.append(1, '\x05');
	tmp.append(g_tth + getTTH().toBase32()); // [!] IRainman opt.
	tmp.append(" (", 2);
	tmp.append(c.getIpPort());
	tmp.append(")|", 2);
	return tmp;
}

AdcCommand SearchResult::toRES(char type) const
{
	AdcCommand cmd(AdcCommand::CMD_RES, type);
	cmd.addParam("SI", Util::toString(size));
	cmd.addParam("SL", Util::toString(freeSlots));
	cmd.addParam("FN", Util::toAdcFile(file));
	cmd.addParam("TR", getTTH().toBase32());
	return cmd;
}

string SearchResult::getFileName() const
{
	if (getType() == TYPE_FILE)
		return Util::getFileName(getFile());
		
	if (getFile().size() < 2)
		return getFile();
		
	string::size_type i = getFile().rfind('\\', getFile().length() - 2);
	if (i == string::npos)
		return getFile();
		
	return getFile().substr(i + 1);
}
