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
#include "AdcHub.h"
#include "Client.h"
#include "ClientManager.h"
#include "DetectionManager.h"
#include "UserCommand.h"
#include "CFlylinkDBManager.h"
#include "Wildcards.h"
#include "UserConnection.h"


FastSharedCriticalSection Identity::g_cs;
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
FastCriticalSection CFlyUserRatioInfo::g_cs;
#endif

#ifdef _DEBUG
#define DISALLOW(a, b) { uint16_t tag1 = TAG(name[0], name[1]); uint16_t tag2 = TAG(a, b); dcassert(tag1 != tag2); }
#else
#define DISALLOW(a, b)
#endif

#ifdef _DEBUG
boost::atomic_int User::g_user_counts(0);
boost::atomic_int OnlineUser::g_online_user_counts(0);
#endif

#define DECL_STRING_INFO_DIC(dmk)\
	Identity::StringDictionaryReductionPointers Identity::g_infoDic##dmk;\
	Identity::StringDictionaryIndex Identity::g_infoDicIndex##dmk

STRING_INFO_DIC_LIST();

#undef DECL_STRING_INFO_DIC

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO

tstring User::getDownload()
{
	const auto l_value = getBytesDownload();
	if (l_value)
		return Util::formatBytesW(l_value);
	else
		return Util::emptyStringT;
}

tstring User::getUpload()
{
	const auto l_value = getBytesUpload();
	if (l_value)
		return Util::formatBytesW(l_value);
	else
		return Util::emptyStringT;
}

tstring User::getUDratio()
{
	m_ratio.initRatio(false);
	if (m_ratio.m_download || m_ratio.m_upload)
		return Util::toStringW(m_ratio.m_download ? ((double)m_ratio.m_upload / (double)m_ratio.m_download) : 0) +
		       L" (" + Util::formatBytesW(m_ratio.m_upload) + _T('/') + Util::formatBytesW(m_ratio.m_download) + L")";
	else
		return Util::emptyStringT;
}

CFlyUserRatioInfo::CFlyUserRatioInfo() : m_nick_id(0), m_is_init(false), m_hub_id(0), m_is_ditry(false), m_is_first_item(false)
{
//   dcdebug(" [!!!!!!]   [!!!!!!]  CFlyUserRatioInfo::CFlyUserRatioInfo()  this = %p\r\n", this);
}

CFlyUserRatioInfo::~CFlyUserRatioInfo()
{
	flushRatio();
}


void CFlyUserRatioInfo::setDitry(bool p_value)
{
	//  dcdebug(" [!!!!!!] CFlyUserRatioInfo::setDitry() p_value = %d m_last_ip = %s nick = %s, m_is_ditry = %d m_nick_id = %d m_hub_id = %d m_is_init = %d this = %p\r\n",
	//    int(p_value), m_last_ip.c_str(), m_nick.c_str(), int(m_is_ditry), int(m_nick_id), int(m_hub_id), int(m_is_init), this);
	m_is_ditry = p_value;
}
void CFlyUserRatioInfo::calcLastIP(const string& p_ip, CFlyUploadDownloadPair<uint64_t>& p_value)
{
	if (p_value.m_last_ip_id == 0)
	{
		p_value.m_last_ip_id = CFlylinkDBManager::getInstance()->get_ip_id(p_ip);
		m_last_ip_id = p_value.m_last_ip_id;
		if (m_last_ip != p_ip)
		{
			m_last_ip = p_ip;
		}
	}
}
void CFlyUserRatioInfo::addUpload(const string& p_ip, uint64_t p_size)
{
	initRatio(true);
	//dcassert(is_ratio_init() == true);
	m_upload += p_size;
	FastLock l(g_cs);
	auto& l_u_d_map = m_upload_download_map[p_ip];
	calcLastIP(p_ip, l_u_d_map);
	l_u_d_map.m_upload += p_size;
	setDitry(true);
}
void CFlyUserRatioInfo::addDownload(const string& p_ip, uint64_t p_size)
{
	initRatio(true);
	//dcassert(is_ratio_init() == true);
	m_download += p_size;
	FastLock l(g_cs);
	auto& l_u_d_map = m_upload_download_map[p_ip];
	calcLastIP(p_ip, l_u_d_map);
	l_u_d_map.m_download += p_size;
	setDitry(true);
}

void CFlyUserRatioInfo::flushRatio()
{
	//  dcdebug(" [!!!!!!] CFlyUserRatioInfo::flush() m_last_ip = %s nick = %s, m_is_ditry = %d m_nick_id = %d m_hub_id = %d m_is_init = %d this = %p\r\n",
	//   m_last_ip.c_str(), m_nick.c_str(), int(m_is_ditry), int(m_nick_id), int(m_hub_id), int(m_is_init), this);
	// dcassert(!m_nick.empty());
	//dcassert(m_hub_id && (m_nick_id || !m_nick.empty()));
	// dcassert(m_nick_id && !m_nick.empty()); ����� �������� ����� ��� - ��� ��� ����
	if (m_is_init && m_is_ditry && m_hub_id && m_nick_id && !m_nick.empty()
	        && CFlylinkDBManager::isValidInstance()) // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86337
	{
		FastLock l(g_cs); // ��� ������ m_upload_download_map
		dcassert(!m_last_ip.empty());
		if (!m_upload_download_map.empty())
		{
			if (!m_last_ip.empty() && m_last_ip_id == 0)
			{
				dcassert(!m_last_ip.empty() && m_last_ip_id == 0);
				m_last_ip_id = CFlylinkDBManager::getInstance()->get_ip_id(m_last_ip); // TODO - ������������ ������ ��� ����������...
			}
			CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_hub_id, m_nick, m_nick_id, m_upload_download_map, m_last_ip_id);
			setDitry(false);
		}
	}
}
void CFlyUserRatioInfo::initRatio(bool p_is_create)
{
	const bool l_is_full_params = m_hub_id && (m_nick_id != 0 || !m_nick.empty());
	if (m_is_first_item == false && p_is_create == true && m_is_init == true && l_is_full_params) // ���� ������, �� ��������� ���� ���������� ������� ���� ���
	{
		m_is_init = false;
		m_is_first_item = true;
	}
	if (m_is_init == false && l_is_full_params) // �� ������� ������ ������ �� ��������?
	{
		auto l_dbm = CFlylinkDBManager::getInstance();
		if (m_nick_id == 0)
			m_nick_id = l_dbm->get_dic_nick_id(m_nick, p_is_create);
		if (m_nick_id)
		{
			const CFlyRatioItem& l_item = l_dbm->LoadRatio(
			                                  m_hub_id,
			                                  m_nick_id,
			                                  *this);
			m_upload   = l_item.m_upload;
			m_download = l_item.m_download;
			if (m_last_ip.empty() && !l_item.m_last_ip.empty()) // [+] IRainman fix.
			{
				m_last_ip  = l_item.m_last_ip;
				m_last_ip_id = l_item.m_last_ip_id;
			}
		}
		else
		{
			dcassert(m_upload == 0 && m_download == 0);
#ifdef _DEBUG
			m_upload = 0;
			m_download = 0;
#endif
		}
		m_is_init  = true;
		m_is_ditry = false;
	}
}
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO

bool Identity::isTcpActive(const Client* client) const // [+] IRainman fix.
{
	if (ClientManager::isMe(user))
	{
		return client->isActive(); // userlist should display our real mode
	}
	else
	{
		return isTcpActive();
	}
}

bool Identity::isTcpActive() const
{
	// [!] IRainman fix.
	if (user->isNMDC())
	{
		return !user->isSet(User::NMDC_FILES_PASSIVE);
	}
	else
	{
		return !getIp().empty() && user->isSet(User::TCP4);
	}
	// [~] IRainman fix.
}
/*
bool Identity::isUdpActive(const Client* client) const // [+] IRainman fix.
{
    if (ClientManager::isMe(user))
    {
        return client->isActive(); // userlist should display our real mode
    }
    else
    {
        return isUdpActive();
    }
}
*/
bool Identity::isUdpActive() const
{
	// [!] IRainman fix.
	if (getIp().empty() || !getUdpPort())
	{
		return false;
	}
	else
	{
		return user->isSet(User::UDP4);
	}
	// [~] IRainman fix.
}

void Identity::getParams(StringMap& sm, const string& prefix, bool compatibility, bool dht) const
{
	PROFILE_THREAD_START();
	if (!dht)
	{
#define APPEND(cmd, val) sm[prefix + cmd] = val;
#define SKIP_EMPTY(cmd, val) { if (!val.empty()) APPEND(cmd, val); }
	
		APPEND("NI", getNick());
		SKIP_EMPTY("SID", getSIDString());
		const auto& l_cid = user->getCID().toBase32();
		APPEND("CID", l_cid);
		APPEND("SSshort", Util::formatBytes(getBytesShared()));
		SKIP_EMPTY("SU", getSupports());
// ���������� �������� ������� ����� ������� get �.�. � ���� �� ���
		SKIP_EMPTY("VE", getStringParam("VE"));
		SKIP_EMPTY("AP", getStringParam("AP"));
		if (compatibility)
		{
			if (prefix == "my")
			{
				sm["mynick"] = getNick();
				sm["mycid"] = l_cid;
			}
			else
			{
				sm["nick"] = getNick();
				sm["cid"] = l_cid;
				sm["ip"] = getIp();
				// sm["tag"] = getStringParam("TA"); deprecated
				sm["description"] = getDescription();
				sm["email"] = getEmail();
				sm["share"] = Util::toString(getBytesShared());
				sm["shareshort"] = Util::formatBytes(getBytesShared());
				sm["realshareformat"] = Util::formatBytes(getRealBytesShared());
			}
		}
#undef APPEND
#undef SKIP_EMPTY
	}
	FastSharedLock l(g_cs);
	for (auto i = m_stringInfo.cbegin(); i != m_stringInfo.cend(); ++i)
	{
		sm[prefix + string((char*)(&i->first), 2)] =  i->second;
	}
}
/* [-] IRainamn opt.
string Identity::getTag() const
{
    return getStringParam("TA");
}
   [-] IRainamn opt.*/
string Identity::getApplication() const
{
	const auto& application = getStringParam("AP");
	const auto& version = getStringParam("VE");
	
	if (version.empty())
	{
		return application;
	}
	
	if (application.empty())
	{
		// AP is an extension, so we can't guarantee that the other party supports it, so default to VE.
		return version;
	}
	
	return application + ' ' + version;
}

#ifdef _DEBUG

// #define PPA_INCLUDE_TEST
#ifdef PPA_INCLUDE_TEST
FastCriticalSection csTest;
#endif

#endif

#define ENABLE_CHECK_GET_SET_IN_IDENTITY
#ifdef ENABLE_CHECK_GET_SET_IN_IDENTITY
# define CHECK_GET_SET_COMMAND()\
	DISALLOW('T','A');\
	DISALLOW('C','L');\
	DISALLOW('N','I');\
	DISALLOW('A','W');\
	DISALLOW('B','O');\
	DISALLOW('S','U');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('I','4');\
	DISALLOW('I','6');\
	DISALLOW('S','S');\
	DISALLOW('C','T');\
	DISALLOW('S','I');\
	DISALLOW('U','S');\
	DISALLOW('S','L');\
	DISALLOW('H','N');\
	DISALLOW('H','R');\
	DISALLOW('H','O');\
	DISALLOW('F','C');\
	DISALLOW('S','T');\
	DISALLOW('R','S');\
	DISALLOW('S','S');\
	DISALLOW('F','D');\
	DISALLOW('F','S');\
	DISALLOW('S','F');\
	DISALLOW('R','S');\
	DISALLOW('C','M');\
	DISALLOW('D','S');\
	DISALLOW('I','D');\
	DISALLOW('L','O');\
	DISALLOW('P','K');\
	DISALLOW('O','P');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('C','O');\
	DISALLOW('W','O');


#else
# define CHECK_GET_SET_COMMAND()
#endif // ENABLE_CHECK_GET_SET_IN_IDENTITY

string Identity::getStringParam(const char* name) const // [!] IRainman fix.
{
	CHECK_GET_SET_COMMAND();
	
#ifdef PPA_INCLUDE_TEST
	static map<short, int> g_cnt;
	FastLock ll(csTest);
	auto& j = g_cnt[*(short*)name];
	j++;
	if (j % 100 == 0)
	{
		dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! get[%s] = %d \n", name, j);
	}
#endif
	
	switch (*(short*)name) // http://code.google.com/p/flylinkdc/issues/detail?id=1314
	{
		case TAG('E', 'M'):
		{
			if (!getEmptyStringParamBit(EM_IS_NOT_EMPTY))
			{
				//dcdebug(" =================== !getEmptyStringParamBit(EM_IS_NOT_EMPTY)\n");
				return Util::emptyString;
			}
		}
		case TAG('D', 'E'):
		{
			if (!getEmptyStringParamBit(DE_IS_NOT_EMPTY))
			{
				//dcdebug(" =================== !getEmptyStringParamBit(DE_IS_NOT_EMPTY)\n");
				return Util::emptyString;
			}
			break;
		}
	};
	
	FastSharedLock l(g_cs); // [4] https://www.box.net/shared/81bdfde50b7c189f8240  https://www.box.net/shared/a130f02bc6c8d99420d8
	
	switch (*(short*)name) // TODO: move to instantly method
	{
#define CHECK_STR_DIC(c1, c2, dmk)\
case TAG(c1,c2):\
{\
	const auto& dicId = getDic##dmk();\
	return dicId > 0 ? *g_infoDic##dmk[dicId - 1] : Util::emptyString;/*TODO: add instantly access*/\
}\
break

		CHECK_STRING_INFO_DIC_LIST();
#undef CHECK_STR_DIC
	}
	
	const auto i = m_stringInfo.find(*(short*)name);
	if (i != m_stringInfo.end())
	{
		return  i->second;
	}
	return Util::emptyString;
}

void Identity::setStringParam(const char* name, const string& val) // [!] IRainman fix.
{
	CHECK_GET_SET_COMMAND();
	
#ifdef PPA_INCLUDE_TEST
	{
		static map<short, int> g_cnt;
		FastLock ll(csTest);
//	auto& i = g_cnt[*(short*)name];
//	i++;
//	if (i % 100 == 0)
//	{
//		dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! set[%s] '%s' count = %d sizeof(*this) = %d\n", name, val.c_str(), i, sizeof(*this));
//	}
		static map<string, int> g_cnt_val;
		string l_key = string(name).substr(0, 2);
		auto& j = g_cnt_val[l_key + "~" + val];
		j++;
		if (l_key != "AP" && l_key != "EM" &&  l_key != "DE" &&  l_key != "VE")
		{
			dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! set[%s] '%s' count = %d sizeof(*this) = %d\n", name, val.c_str(), j, sizeof(*this));
		}
	}
#endif
	bool l_is_processing_stringInfo_map = true;
	if (val.empty()) //  http://code.google.com/p/flylinkdc/issues/detail?id=1314
	{
		switch (*(short*)name) // TODO: move to instantly method
		{
			case TAG('E', 'M'):
			{
				l_is_processing_stringInfo_map = getEmptyStringParamBit(EM_IS_NOT_EMPTY); // TODO ��� ���� ����� �������
				setEmptyStringParamBit(EM_IS_NOT_EMPTY, false);
				break;
			}
			case TAG('D', 'E'):
			{
				l_is_processing_stringInfo_map = getEmptyStringParamBit(DE_IS_NOT_EMPTY);  // TODO ��� ���� ����� �������
				setEmptyStringParamBit(DE_IS_NOT_EMPTY, false);
				break;
			}
		}
	}
	if (l_is_processing_stringInfo_map)
	{
		FastUniqueLock l(g_cs);
		
		switch (*(short*)name) // TODO: move to instantly method
		{
			case TAG('E', 'M'): //  http://code.google.com/p/flylinkdc/issues/detail?id=1314
			{
				setEmptyStringParamBit(EM_IS_NOT_EMPTY, !val.empty());
				break;
			}
			case TAG('D', 'E'): //  http://code.google.com/p/flylinkdc/issues/detail?id=1314
			{
				setEmptyStringParamBit(DE_IS_NOT_EMPTY, !val.empty());
				break;
			}
#define CHECK_STR_DIC(c1, c2, dmk)\
case TAG(c1,c2):\
	setDicId##dmk##L(val);\
	return
				
				CHECK_STRING_INFO_DIC_LIST();
#undef CHECK_STR_DIC
		}
		
		if (val.empty())
		{
			m_stringInfo.erase(*(short*)name);
		}
		else
		{
			m_stringInfo[*(short*)name] = val;
		}
	}
	else
	{
		// ���������� ����������� m_stringInfo ��� �����
		// �� ������ ���� �� ������ ?
	}
}

Identity::~Identity()
{
	// TODO fix me https://code.google.com/p/flylinkdc/issues/detail?id=1244
}

void FavoriteUser::update(const OnlineUser& info) // !SMT!-fix
{
	// [!] FlylinkDC Team: please let me know if the assertions fail. IRainman.
	dcassert(!info.getIdentity().getNick().empty() || info.getClient().getHubUrl().empty());
	
	setNick(info.getIdentity().getNick()); // [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=487
	setUrl(info.getClient().getHubUrl());
}

string Identity::setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient)
{
	if (!c.isOp() || isOp()) return Util::emptyString;
	
	PLAY_SOUND(SOUND_FAKERFILE);
	
	StringMap ucParams;
	getParams(ucParams, "user", true);
	string cheat = Util::formatParams(aCheatDescription, ucParams, false);
	
	setStringParam("CS", cheat);
	setFakeCardBit(BAD_CLIENT, aBadClient);
	
	string report = "*** " + STRING(USER) + ' ' + getNick() + " - " + cheat;
	return report;
}

tstring Identity::getHubs() const
{
	const auto l_hub_normal = getHubNormal();
	const auto l_hub_reg = getHubRegister();
	const auto l_hub_op = getHubOperator();
	if (l_hub_normal || l_hub_reg || l_hub_op)
	{
		tstring hubs;
		hubs.resize(64);
		hubs.resize(snwprintf(&hubs[0], hubs.size(), _T("%u (%u/%u/%u)"),
		                      l_hub_normal + l_hub_reg + l_hub_op,
		                      l_hub_normal,
		                      l_hub_reg,
		                      l_hub_op));
		return hubs;
	}
	else
	{
		return Util::emptyStringT;
	}
}

string Identity::formatShareBytes(uint64_t p_bytes) // [+] IRainman
{
	return p_bytes ? Util::formatBytes(p_bytes) + " (" + Text::fromT(Util::formatExactSize(p_bytes)) + ")" : Util::emptyString;
}

string Identity::formatIpString(const string& value) // [+] IRainman IP.
{
	if (!value.empty())
	{
		auto ret = value + " (";
		const auto l_dns = Socket::getRemoteHost(value);
		if (!l_dns.empty())
		{
			ret +=  l_dns + " - ";
		}
		const auto l_location = Util::getIpCountry(value);
		ret += Text::fromT(l_location.getDescription()) + ")";
		return ret;
	}
	else
	{
		return Util::emptyString;
	}
};

string Identity::formatSpeedLimit(const uint32_t limit) // [+] IRainman
{
	return limit ? Util::formatBytes(limit) + '/' + STRING(S) : Util::emptyString;
}

void Identity::getReport(string& p_report) const
{
	p_report = "\r\n *** FlylinkDC user info ***\r\n";// [+] FlylinkDC
	const string sid = getSIDString();
	{
		// [+] IRainman fix.
		auto appendBoolValue = [&](const string & name, const bool value, const string & iftrue, const string & iffalse)
		{
			p_report += "\t" + name + ": " + (value ? iftrue : iffalse) + "\r\n";
		};
		
		auto appendStringIfSetBool = [&](const string & str, const bool value)
		{
			if (value)
			{
				p_report += str + ' ';
			}
		};
		
		auto appendIfValueNotEmpty = [&](const string & name, const string & value)
		{
			if (!value.empty())
			{
				p_report += "\t" + name + ": " + value + "\r\n";
			}
		};
		
		auto appendIfValueSetInt = [&](const string & name, const unsigned int value)
		{
			if (value)
			{
				appendIfValueNotEmpty(name, Util::toString(value));
			}
		};
		
		auto appendIfValueSetSpeedLimit = [&](const string & name, const unsigned int value)
		{
			if (value)
			{
				appendIfValueNotEmpty(name, formatSpeedLimit(value));
			}
		};
		
		// TODO: translate
		const auto isNmdc = getUser()->isNMDC();
		
		appendIfValueNotEmpty(STRING(NICK), getNick());
		if (!isNmdc)
		{
			appendIfValueNotEmpty("Nicks", Util::toString(ClientManager::getInstance()->getNicks(user->getCID(), Util::emptyString)));
		}
		appendIfValueNotEmpty(STRING(HUBS), Text::fromT(getHubs()));
		if (!isNmdc)
		{
			appendIfValueNotEmpty("Hub names", Util::toString(ClientManager::getInstance()->getHubNames(user->getCID(), Util::emptyString)));
			appendIfValueNotEmpty("Hub addresses", Util::toString(ClientManager::getInstance()->getHubs(user->getCID(), Util::emptyString)));
		}
		
		p_report += "\t" "Client type" ": ";
		appendStringIfSetBool("Hub", isHub());
		appendStringIfSetBool("Bot", isBot());
		appendStringIfSetBool(STRING(OPERATOR), isOp());
		p_report += '(' + Util::toString(getClientType()) + ')';
		p_report += "\r\n";
		
		p_report += "\t";
		appendStringIfSetBool(STRING(AWAY), getUser()->isAway());
		appendStringIfSetBool(STRING(SERVER), getUser()->isServer());
		appendStringIfSetBool(STRING(FIREBALL), getUser()->isFireball());
		p_report += "\r\n";
		
		appendIfValueNotEmpty("Client ID", getUser()->getCID().toBase32());
		appendIfValueSetInt("Session ID", getSID());
		
		appendIfValueSetInt(STRING(SLOTS), getSlots());
		appendIfValueSetSpeedLimit(STRING(AVERAGE_UPLOAD), getLimit());
		appendIfValueSetSpeedLimit(isNmdc ? STRING(CONNECTION) : "Download speed", getDownloadSpeed());
		
		appendIfValueSetInt(STRING(SHARED_FILES), getSharedFiles());
		appendIfValueNotEmpty(STRING(SHARED) + " - reported", formatShareBytes(getBytesShared()));
		appendIfValueNotEmpty(STRING(SHARED) + " - real", formatShareBytes(getRealBytesShared()));
		
		appendIfValueSetInt("Fake check card", getFakeCard());
		appendIfValueSetInt("Connection Timeouts", getConnectionTimeouts());
		appendIfValueSetInt("Filelist disconnects", getFileListDisconnects());
		
		if (isNmdc)
		{
			appendIfValueNotEmpty("NMDC status", NmdcSupports::getStatus(*this));
			appendBoolValue("Files mode", getUser()->isSet(User::NMDC_FILES_PASSIVE), "Passive", "Active");
			appendBoolValue("Search mode", getUser()->isSet(User::NMDC_SEARCH_PASSIVE), "Passive", "Active");
		}
		appendIfValueNotEmpty("Known supports", getSupports());
		
		appendIfValueNotEmpty("IPv4 Address", formatIpString(getIp()));
		// appendIfValueNotEmpty("IPv6 Address", formatIpString(getIp())); TODO
		// [~] IRainman fix.
		
		// ���������� �������� ������� ����� ������� get �.�. � ���� �� ���
		appendIfValueNotEmpty("DC client", getStringParam("AP"));
		appendIfValueNotEmpty("Client version", getStringParam("VE"));
		
		FastSharedLock l(g_cs);
		for (auto i = m_stringInfo.cbegin(); i != m_stringInfo.cend(); ++i)
		{
			auto name = string((char*)(&i->first), 2);
			const auto& value = i->second;
			// TODO: translate known tags and format values to something more readable
			switch (i->first)
			{
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
				case TAG('C', 'L'):
					name = "Client name";
					break;
				case TAG('C', 'M'):
					name = "Comment";
					break;
				case TAG('G', 'E'):
					name = "Filelist generator";
					break;
# ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
				case TAG('L', 'O'):
					name = "NMDC Lock";
					break;
				case TAG('P', 'K'):
					name = "NMDC Pk";
					break;
# endif
#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER
				case TAG('C', 'S'): // ok
					name = "Cheat description";
					break;
				case TAG('D', 'E'): // ok
					name = STRING(DESCRIPTION);
					break;
				case TAG('E', 'M'): // ok
					name = "E-mail";
					break;
				case TAG('K', 'P'): // ok
					name = "KeyPrint";
					break;
				default:
					name += " (unknown)";
			}
			
			appendIfValueNotEmpty(name, value);
		}
	}
}

string Identity::getSupports() const // [+] IRainman fix.
{
	string tmp = UcSupports::getSupports(*this);
	/*
	if (getUser()->isNMDC())
	{
	    tmp += NmdcSupports::getSupports(*this);
	}
	else
	*/
	{
		tmp += AdcSupports::getSupports(*this);
	}
	return tmp;
}

/*
string Identity::updateClientType(const OnlineUser& ou)
{
    if (getUser()->isSet(User::DCPLUSPLUS))
    {
        if (get("LL") == "11" && getBytesShared() > 0)
        {
            string report = setCheat(ou.getClient(), "Fake file list - ListLen = 11" , true);
            set("CL", "DC++ Stealth");
            set_fake_card_bit(BAD_CLIENT | BAD_LIST, true);

            ClientManager::getInstance()->sendRawCommand(ou, SETTING(LISTLEN_MISMATCH));
            return report;
        }
        else if (strncmp(getTag().c_str(), "<++ V:0.69", 10) == 0 && get("LL") != "42")
        {
            string report = setCheat(ou.getClient(), "Listlen mismatched" , true);
            set("CL", "Faked DC++");
            set("CM", "Supports corrupted files...");
            set_fake_card_bit(BAD_CLIENT, true);

            ClientManager::getInstance()->sendRawCommand(ou, SETTING(LISTLEN_MISMATCH));
            return report;
        }
    }
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
    uint64_t tick = GET_TICK();

    StringMap params;
    getDetectionParams(params); // get identity fields and escape them, then get the rest and leave as-is
    const DetectionManager::DetectionItems& profiles = DetectionManager::getInstance()->getProfiles(params);

    for (auto i = profiles.cbegin(); i != profiles.cend(); ++i)
    {
        const DetectionEntry& entry = *i;
        if (!entry.isEnabled)
            continue;
        DetectionEntry::INFMap INFList;
        if (!entry.defaultMap.empty())
        {
            // fields to check for both, adc and nmdc
            INFList = entry.defaultMap;
        }

        if (getUser()->isSet(User::NMDC) && !entry.nmdcMap.empty())
        {
            INFList.insert(INFList.end(), entry.nmdcMap.begin(), entry.nmdcMap.end());
        }
        else if (!entry.adcMap.empty())
        {
            INFList.insert(INFList.end(), entry.adcMap.begin(), entry.adcMap.end());
        }

        if (INFList.empty())
            continue;

        bool _continue = false;

        DETECTION_DEBUG("\tChecking profile: " + entry.name);

        for (auto j = INFList.cbegin(); j != INFList.cend(); ++j)
        {

            // TestSUR not supported anymore, so ignore it to be compatible with older profiles
            if (j->first == "TS")
                continue;

            string aPattern = Util::formatRegExp(j->second, params);
            string aField = getDetectionField(j->first);
            DETECTION_DEBUG("Pattern: " + aPattern + " Field: " + aField);
            if (!Identity::matchProfile(aField, aPattern))
            {
                _continue = true;
                break;
            }
        }
        if (_continue)
            continue;

        DETECTION_DEBUG("Client found: " + entry.name + " time taken: " + Util::toString(GET_TICK() - tick) + " milliseconds");

        set("CL", entry.name);
        set("CM", entry.comment);

        set_fake_card_bit(BAD_CLIENT, !entry.cheat.empty());

        if (entry.checkMismatch && getUser()->isSet(User::NMDC) && (params["VE"] != params["PKVE"]))
        {
            set("CL", entry.name + " Version mis-match");
            return setCheat(ou.getClient(), entry.cheat + " Version mis-match", true);
        }

        string report;
        if (!entry.cheat.empty())
        {
            report = setCheat(ou.getClient(), entry.cheat, true);
        }

        ClientManager::getInstance()->sendRawCommand(ou, entry.rawToSend);// ���������� ������ getUser() ������� �� ou.getUser()
        return report;
    }

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

    set("CL", "Unknown");
    set("CS", Util::emptyString);
    set_fake_card_bit(BAD_CLIENT, false);
    return Util::emptyString;
}
*/
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
string Identity::updateClientType(const OnlineUser& ou)
{
	StringMap params;
	getDetectionParams(params); // get identity fields and escape them, then get the rest and leave as-is
	const DetectionManager::DetectionItems& profiles = DetectionManager::getInstance()->getProfiles(params);
	
	const uint64_t tick = GET_TICK();
	
	for (auto i = profiles.cbegin(); i != profiles.cend(); ++i)
	{
		const DetectionEntry& entry = *i;
		if (!entry.isEnabled)
			continue;
		DetectionEntry::INFMap INFList;
		if (!entry.defaultMap.empty())
		{
			// fields to check for both, adc and nmdc
			INFList = entry.defaultMap;
		}
		
		if (getUser()->isSet(User::NMDC) && !entry.nmdcMap.empty())
		{
			INFList.insert(INFList.end(), entry.nmdcMap.begin(), entry.nmdcMap.end());
		}
		else if (!entry.adcMap.empty())
		{
			INFList.insert(INFList.end(), entry.adcMap.begin(), entry.adcMap.end());
		}
		
		if (INFList.empty())
			continue;
			
		bool _continue = false;
		
		DETECTION_DEBUG("\tChecking profile: " + entry.name);
		
		for (auto j = INFList.cbegin(); j != INFList.cend(); ++j)
		{
		
			// TestSUR not supported anymore, so ignore it to be compatible with older profiles
			if (j->first == "TS")
				continue;
				
			string aPattern = Util::formatRegExp(j->second, params);
			string aField = getDetectionField(j->first);
			DETECTION_DEBUG("Pattern: " + aPattern + " Field: " + aField);
			if (!Identity::matchProfile(aField, aPattern))
			{
				_continue = true;
				break;
			}
		}
		if (_continue)
			continue;
			
		DETECTION_DEBUG("Client found: " + entry.name + " time taken: " + Util::toString(GET_TICK() - tick) + " milliseconds");
		
		setStringParam("CL", entry.name);
		setStringParam("CM", entry.comment);
		
		setFakeCardBit(BAD_CLIENT, !entry.cheat.empty());
		
		if (entry.checkMismatch && getUser()->isSet(User::NMDC)
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		        && (params["VE"] != params["PKVE"])
#endif
		   )
		{
			setStringParam("CL", entry.name + " Version mis-match");
			return setCheat(ou.getClient(), entry.cheat + " Version mis-match", true);
		}
		
		string report;
		if (!entry.cheat.empty())
		{
			report = setCheat(ou.getClient(), entry.cheat, true);
		}
		
		ClientManager::getInstance()->sendRawCommand(ou, entry.rawToSend);
		return report;
	}
	
	setStringParam("CL", "Unknown");
	setStringParam("CS", Util::emptyString);
	setFakeCardBit(BAD_CLIENT, false);
	return Util::emptyString;
}

string Identity::getDetectionField(const string& aName) const
{
	if (aName.length() == 2)
	{
		if (aName == "TA")
			return getTag();
		else if (aName == "CO")
			return getConnection();
		else
			return getStringParam(aName.c_str());
	}
	else
	{
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		if (aName == "PKVE")
		{
			return getPkVersion();
		}
#endif
		return Util::emptyString;
	}
}
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
string Identity::getPkVersion() const
{
	const string pk = getStringParam("PK");
	if (pk.length()) //[+]PPA
	{
		match_results<string::const_iterator> result;
		regex reg("[0-9]+\\.[0-9]+", regex_constants::icase);
		if (regex_search(pk.cbegin(), pk.cend(), result, reg, regex_constants::match_default))
			return result.str();
	}
	return Util::emptyString;
}
#endif // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
bool Identity::matchProfile(const string& aString, const string& aProfile) const
{
	DETECTION_DEBUG("\t\tMatching String: " + aString + " to Profile: " + aProfile);
	
	try
	{
		regex reg(aProfile);
		return regex_search(aString.begin(), aString.end(), reg);
	}
	catch (...)
	{
	}
	
	return false;
}

void Identity::getDetectionParams(StringMap& p)
{
	getParams(p, Util::emptyString, false);
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	p["PKVE"] = getPkVersion();
#endif
	//p["VEformat"] = getVersion();
	
	if (!user->isSet(User::NMDC))
	{
		string version = getStringParam("VE");
		string::size_type i = version.find(' ');
		if (i != string::npos)
			p["VEformat"] = version.substr(i + 1);
		else
			p["VEformat"] = version;
	}
	else
	{
		p["VEformat"] = getStringParam("VE");
	}
	
	// convert all special chars to make regex happy
	for (auto i = p.cbegin(); i != p.cend(); ++i)
	{
		// looks really bad... but do the job
		Util::replace("\\", "\\\\", i->second); // this one must be first
		Util::replace("[", "\\[", i->second);
		Util::replace("]", "\\]", i->second);
		Util::replace("^", "\\^", i->second);
		Util::replace("$", "\\$", i->second);
		Util::replace(".", "\\.", i->second);
		Util::replace("|", "\\|", i->second);
		Util::replace("?", "\\?", i->second);
		Util::replace("*", "\\*", i->second);
		Util::replace("+", "\\+", i->second);
		Util::replace("(", "\\(", i->second);
		Util::replace(")", "\\)", i->second);
		Util::replace("{", "\\{", i->second);
		Util::replace("}", "\\}", i->second);
	}
}

string Identity::getVersion(const string& aExp, string aTag)
{
	string::size_type i = aExp.find("%[version]");
	if (i == string::npos)
	{
		i = aExp.find("%[version2]");
		return splitVersion(aExp.substr(i + 11), splitVersion(aExp.substr(0, i), aTag, 1), 0);
	}
	return splitVersion(aExp.substr(i + 10), splitVersion(aExp.substr(0, i), aTag, 1), 0);
}

string Identity::splitVersion(const string& aExp, string aTag, size_t part)
{
	try
	{
		regex reg(aExp);
		
		StringList out;
		// old: boost::regex_split(std::back_inserter(out), aTag, reg, boost::regex_constants::match_default, 2);
		// new:
		sregex_token_iterator i(aTag.cbegin(), aTag.cend(), reg);
		sregex_token_iterator end;
		copy(i, end, out.begin());
		// ~new
		if (part >= out.size())
			return Util::emptyString;
			
		return out[part];
	}
	catch (...)
	{
	}
	
	return Util::emptyString;
}

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

#ifdef IRAINMAN_ENABLE_AUTO_BAN
User::DefinedAutoBanFlags User::hasAutoBan(Client *p_Client /*= NULL*/)
{
	// Check exclusion first
	
	bool bForceAllow = BOOLSETTING(PROT_FAVS) && FavoriteManager::getInstance()->isFavoriteUser(this);
	if (!bForceAllow && !UserManager::protectedUserListEmpty())
	{
		const string l_Nick = getLastNick();
		bForceAllow = !l_Nick.empty() && !UserManager::isInProtectedUserList(l_Nick);
	}
	int iBan = BAN_NONE;
	if (!bForceAllow)
	{
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		if (getHubID() != 0) // Value HubID is zero for himself, do not check your user.
#endif
		{
			const int l_limit = getLimit();
			const int l_slots = getSlots();
			
			const int iSettingBanSlotMax = SETTING(BAN_SLOTS_H);
			const int iSettingBanSlotMin = SETTING(BAN_SLOTS);
			const int iSettingLimit = SETTING(BAN_LIMIT);
			const int iSettingShare = SETTING(BAN_SHARE);
			
			// [+] IRainman
			if (m_support_slots == FLY_SUPPORT_SLOTS_FIRST)//[+]PPA optimize autoban
			{
				bool HubDoenstSupportSlot = isNMDC();
				if (HubDoenstSupportSlot)
				{
					if (p_Client)
					{
						HubDoenstSupportSlot = p_Client->hubIsNotSupportSlot();
					}
				}
				m_support_slots = HubDoenstSupportSlot ? FLY_NSUPPORT_SLOTS : FLY_SUPPORT_SLOTS;
			}
			// [~] IRainman
			
			if ((m_support_slots == FLY_SUPPORT_SLOTS || l_slots) && iSettingBanSlotMin && l_slots < iSettingBanSlotMin)
				iBan |= BAN_BY_MIN_SLOT;
				
			if (iSettingBanSlotMax && l_slots > iSettingBanSlotMax)
				iBan |= BAN_BY_MAX_SLOT;
				
			if (iSettingShare && static_cast<int>(getBytesShared() / uint64_t(1024 * 1024 * 1024)) < iSettingShare)
				iBan |= BAN_BY_SHARE;
				
			// Skip users with limitation turned off
			if (iSettingLimit && l_limit && l_limit < iSettingLimit)
				iBan |= BAN_BY_LIMIT;
		}
	}
	return static_cast<DefinedAutoBanFlags>(iBan);
}
#endif // IRAINMAN_ENABLE_AUTO_BAN
//[~]FlylinkDC
/**
 * @file
 * $Id: User.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
