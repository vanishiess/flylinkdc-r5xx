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

#ifdef _WIN32

#include "w.h"
#include "shlobj.h"
#include "CompatibilityManager.h" // [+] IRainman

#endif

#include "CID.h"
#include "File.h"
#include "SettingsManager.h"
#include "SettingsManager.h"
#include "UploadManager.h"
#include "ShareManager.h"
#include "SimpleXML.h"
#include "OnlineUser.h"
#include "Socket.h"
#include <fstream>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <ctype.h>
#endif
#include <locale.h>

#include <boost/algorithm/string.hpp>
#include "LogManager.h"
#include "../FlyFeatures/AutoUpdate.h"
#include "../windows/resource.h" // TODO - ����� ��� ��� ���������� �����? �� ����� �� �������, ��� �� ������ ��������, ������� ������ �������� ��� userlocation :)

#include "idna/idna.h" // [+] SSA
#include "MD5Calc.h" // [+] SSA
#include "../FlyFeatures/flyServer.h"

const string g_tth = "TTH:"; // [+] IRainman opt.
const time_t Util::g_startTime = time(NULL);
const string Util::emptyString;
const wstring Util::emptyStringW;
const tstring Util::emptyStringT;

const vector<uint8_t> Util::emptyByteVector; // [+] IRainman opt.

const string Util::m_dot = ".";
const string Util::m_dot_dot = "..";
const tstring Util::m_dotT = _T(".");
const tstring Util::m_dot_dotT = _T("..");

bool Util::g_away = false;
string Util::g_awayMsg;
time_t Util::g_awayTime;

string Util::g_paths[Util::PATH_LAST];
string Util::g_sysPaths[Util::SYS_PATH_LAST];
// [+] IRainman opt.
NUMBERFMT Util::g_nf = { 0 };
// [~] IRainman opt.
bool Util::g_localMode = true;

static void sgenrand(unsigned long seed);

extern "C" void bz_internal_error(int errcode)
{
	dcdebug("bzip2 internal error: %d\n", errcode);
}

#if (_MSC_VER >= 1400 )
void WINAPI invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	//do nothing, this exist because vs2k5 crt needs it not to crash on errors.
}
#endif
void Util::initialize()
{
	Text::initialize();
	
	sgenrand((unsigned long)time(NULL));
	
#if (_MSC_VER >= 1400)
	_set_invalid_parameter_handler(reinterpret_cast<_invalid_parameter_handler>(invalidParameterHandler));
#endif
	// [+] IRainman opt.
	static TCHAR g_sep[2] = _T(",");
	static wchar_t g_Dummy[16] = { 0 };
	g_nf.lpDecimalSep = g_sep;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, g_Dummy, 16);
	g_nf.Grouping = _wtoi(g_Dummy);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, g_Dummy, 16);
	g_nf.lpThousandSep = g_Dummy;
	// [~] IRainman opt.
#ifdef _WIN32
	LocalArray<TCHAR, MAX_PATH> buf;
	::GetModuleFileName(NULL, buf.data(), MAX_PATH);
	const string exePath = Util::getFilePath(Text::fromT(buf.data()));
	// [+] IRainman: FlylinkDC system path init.
#define SYS_WIN_PATH_INIT(path) \
	if(::SHGetFolderPath(NULL, CSIDL_##path, NULL, SHGFP_TYPE_CURRENT, buf.data()) == S_OK) \
		g_sysPaths[path] = Text::fromT(buf.data()) + PATH_SEPARATOR
	
	SYS_WIN_PATH_INIT(WINDOWS);
	SYS_WIN_PATH_INIT(PROGRAM_FILESX86);
	SYS_WIN_PATH_INIT(PROGRAM_FILES);
	if (CompatibilityManager::runningIsWow64())
	{
		// [!] Correct PF path on 64 bit system with run 32 bit programm.
		const char* l_PFW6432 = getenv("ProgramW6432");
		if (l_PFW6432)
			g_sysPaths[PROGRAM_FILES] = string(l_PFW6432) + PATH_SEPARATOR;
	}
	SYS_WIN_PATH_INIT(APPDATA);
	SYS_WIN_PATH_INIT(LOCAL_APPDATA);
	SYS_WIN_PATH_INIT(COMMON_APPDATA);
	SYS_WIN_PATH_INIT(PERSONAL);
	
#undef SYS_WIN_PATH_INIT
	// [~] IRainman: FlylinkDC system path init.
	
	// Global config path is FlylinkDC++ executable path...
	g_paths[PATH_EXE] = exePath;
	g_paths[PATH_GLOBAL_CONFIG] = exePath;
#ifdef USE_APPDATA //[+] NightOrion
	if ((File::isExist(exePath + "Settings" PATH_SEPARATOR_STR "DCPlusPlus.xml")) || !(locatedInSysPath(PROGRAM_FILES, exePath) || locatedInSysPath(PROGRAM_FILESX86, exePath)))
	{
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		g_paths[PATH_ALL_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
# endif
	}
	else
	{
		g_paths[PATH_USER_CONFIG] = getSysPath(APPDATA) + "FlylinkDC++" PATH_SEPARATOR_STR;
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		g_paths[PATH_ALL_USER_CONFIG] = getSysPath(COMMON_APPDATA) + "FlylinkDC++" PATH_SEPARATOR_STR;
# endif
	}
#else // USE_APPDATA
	g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
#endif //USE_APPDATA    
	g_paths[PATH_LANGUAGES] = g_paths[PATH_GLOBAL_CONFIG] + "Lang" PATH_SEPARATOR_STR;
	
	g_paths[PATH_EXTERNAL_ICO] = g_paths[PATH_GLOBAL_CONFIG] + "FlylinkDC.ico";//[+] IRainman
	
	g_paths[PATH_THEMES] = g_paths[PATH_GLOBAL_CONFIG] + "Themes" PATH_SEPARATOR_STR;
	
	g_paths[PATH_SOUNDS] = g_paths[PATH_GLOBAL_CONFIG] + "Sounds" PATH_SEPARATOR_STR;
	// [~] TODO This is crossplatform paths ;)
	
	loadBootConfig();
	
	if (!File::isAbsolute(g_paths[PATH_USER_CONFIG]))
	{
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_USER_CONFIG] = validateFileName(g_paths[PATH_USER_CONFIG]);
	
	if (g_localMode)
	{
		g_paths[PATH_USER_LOCAL] = g_paths[PATH_USER_CONFIG];
	}
	else
	{
		if (!getSysPath(PERSONAL).empty())
		{
			g_paths[PATH_USER_CONFIG] = getSysPath(PERSONAL) + "FlylinkDC++" PATH_SEPARATOR_STR;
		}
		
		g_paths[PATH_USER_LOCAL] = !getSysPath(PERSONAL).empty() ? getSysPath(PERSONAL) + "FlylinkDC++" PATH_SEPARATOR_STR : g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_DOWNLOADS] = getDownloadPath(CompatibilityManager::getDefaultPath());
//	g_paths[PATH_RESOURCES] = exePath;
	g_paths[PATH_WEB_SERVER] = exePath + "WEBserver" PATH_SEPARATOR_STR;
	
#else // _WIN32
	g_paths[PATH_GLOBAL_CONFIG] = "/etc/";
	const char* home_ = getenv("HOME");
	string home = home_ ? Text::toUtf8(home_) : "/tmp/";
	
	g_paths[PATH_USER_CONFIG] = home + "/.flylinkdc++/";
	
	loadBootConfig();
	
	if (!File::isAbsolute(g_paths[PATH_USER_CONFIG]))
	{
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_USER_CONFIG] = validateFileName(g_paths[PATH_USER_CONFIG]);
	
	if (g_localMode)
	{
		// @todo implement...
	}
	
	g_paths[PATH_USER_LOCAL] = g_paths[PATH_USER_CONFIG];
	g_paths[PATH_RESOURCES] = "/usr/share/";
	g_paths[PATH_LOCALE] = g_paths[PATH_RESOURCES] + "locale/";
	g_paths[PATH_DOWNLOADS] = home + "/Downloads/";
	
#endif // _WIN32
	
	g_paths[PATH_FILE_LISTS] = g_paths[PATH_USER_LOCAL] + "FileLists" PATH_SEPARATOR_STR;
	g_paths[PATH_HUB_LISTS] = g_paths[PATH_USER_LOCAL] + "HubLists" PATH_SEPARATOR_STR;
	g_paths[PATH_NOTEPAD] = g_paths[PATH_USER_CONFIG] + "Notepad.txt";
	g_paths[PATH_EMOPACKS] = g_paths[PATH_GLOBAL_CONFIG] + "EmoPacks" PATH_SEPARATOR_STR;
	
	// [+] IRainman opt
	shrink_to_fit(&g_paths[0], &g_paths[PATH_LAST]);
	shrink_to_fit(&g_sysPaths[0], &g_sysPaths[SYS_PATH_LAST]);
	// [~] IRainman opt.
	
	File::ensureDirectory(g_paths[PATH_USER_CONFIG]);
	File::ensureDirectory(g_paths[PATH_USER_LOCAL]);
}
//==========================================================================
static const char* g_countryCodes[] = // TODO: needs update this table! http://en.wikipedia.org/wiki/ISO_3166-1
{
	"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AQ", "AR", "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB",
	"BD", "BE", "BF", "BG", "BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA", "CC",
	"CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR", "CS", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ",
	"DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "EU", "FI", "FJ", "FK", "FM", "FO", "FR", "GA",
	"GB", "GD", "GE", "GF", "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW", "GY", "HK",
	"HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO", "JP",
	"KE", "KG", "KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK", "LR", "LS", "LT",
	"LU", "LV", "LY", "MA", "MC", "MD", "ME", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ", "MR", "MS", "MT",
	"MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU", "NZ", "OM",
	"PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS", "RU",
	"RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SY",
	"SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG",
	"UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT", "YU", "ZA", "ZM", "ZW"
};
//==========================================================================
static int getFlagIndexByCode(uint16_t p_countryCode) // [!] IRainman: countryCode is uint16_t.
{
	// country codes are sorted, use binary search for better performance
	int begin = 0;
	int end = _countof(g_countryCodes) - 1;
	
	while (begin <= end)
	{
		const int mid = (begin + end) / 2;
		const int cmp = memcmp(&p_countryCode, g_countryCodes[mid], 2);
		
		if (cmp > 0)
			begin = mid + 1;
		else if (cmp < 0)
			end = mid - 1;
		else
			return mid + 1;
	}
	return 0;
}
//==========================================================================
void Util::loadGeoIp()
{
	{
		CFlyLog l_log("[GeoIp]");
		// This product includes GeoIP data created by MaxMind, available from http://maxmind.com/
		// Updates at http://www.maxmind.com/app/geoip_country
		const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + "GeoIpCountryWhois.csv";
		                        
		try
		{
			const uint64_t l_timeStampFile  = File::getSafeTimeStamp(fileName);
			const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampGeoIP);
			if (l_timeStampFile != l_timeStampDb)
			{
				const string data = File(fileName, File::READ, File::OPEN).read();
				l_log.step("read:" + fileName);
				const char* start = data.c_str();
				size_t linestart = 0;
				size_t lineend = 0;
				uint32_t startIP = 0, stopIP = 0;
				uint16_t flagIndex = 0;
				// [+] IRainman opt: http://en.wikipedia.org/wiki/ISO_3166-1 : 2013.08.12: Currently 249 countries, territories, or areas of geographical interest are assigned official codes in ISO 3166-1,
				// http://www.assembla.com/spaces/customlocations-greylink 20130812-r1281, providers count - 1422
				string countryFullNamePrev;
				CFlyLocationIPArray l_sqlite_array;
				l_sqlite_array.reserve(100000);
				while (true)
				{
					auto pos = data.find(',', linestart);
					if (pos == string::npos) break;
					pos = data.find(',', pos + 6); // ��� ����� ���������� �� 1 � 6 �.�. ����������� ����� IP � ���� ������ ����� 7 �������� "1.1.1.1"
					if (pos == string::npos) break;
					startIP = toUInt32(start + pos + 2);
					
					pos = data.find(',', pos + 7); // ��� ����� ���������� �� 1 � 7 �.�. ����������� ����� IP � ���� ����� ����� 8 �������� 1.0.0.0 = 16777216
					if (pos == string::npos) break;
					stopIP = toUInt32(start + pos + 2);
					
					pos = data.find(',', pos + 7); // ��� ����� ���������� �� 1 � 7 �.�. ����������� ����� IP � ���� ����� ����� 8 �������� 1.0.0.0 = 16777216
					if (pos == string::npos) break;
					flagIndex = getFlagIndexByCode(*reinterpret_cast<const uint16_t*>(start + pos + 2));
					pos = data.find(',', pos + 1);
					if (pos == string::npos) break;
					lineend = data.find('\n', pos);
					if (lineend == string::npos) break;
					pos += 2;
					countryFullNamePrev = data.substr(pos, lineend - 1 - pos);
					CFlyLocationIP l_item;
					l_item.m_start_ip = startIP;
					l_item.m_stop_ip  = stopIP;
					l_item.m_dic_country_location_ip = CFlylinkDBManager::getInstance()->get_dic_country_id(countryFullNamePrev);
					l_item.m_flag_index = flagIndex;
					l_sqlite_array.push_back(l_item);
					linestart = lineend + 1;
				}
				{
					CFlyLog l_geo_log_sqlite("[GeoIp-sqlite]");
					CFlylinkDBManager::getInstance()->save_geoip(l_sqlite_array);
				}
				CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampGeoIP, l_timeStampFile);
			}
		}
		catch (const FileException&)
		{
			LogManager::getInstance()->message("Error open " + fileName);
		}
	}
}

void customLocationLog(const string& p_line, const string& p_error) // [+] IRainman
{
	if (BOOLSETTING(LOG_CUSTOM_LOCATION))
	{
		StringMap params;
		params["line"] = p_line;
		params["error"] = p_error;
		LOG(CUSTOM_LOCATION, params);
	}
}

void Util::loadCustomlocations()// [!] IRainman: this function workings fine. Please don't merge from other project!
{
	const tstring l_fileName = Text::toT(getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	                                         true
#endif
	                                     )) + _T("CustomLocations.ini");
	const uint64_t l_timeStampFile = File::getSafeTimeStamp(Text::fromT(l_fileName)); // TOOD - fix fromT
	const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampCustomLocation);
	if (l_timeStampFile != l_timeStampDb)
	{
		std::ifstream l_file(l_fileName.c_str());
		string l_currentLine;
		if (l_file.is_open())
		{
			CFlyLog l_log("[CustomLocations.ini]");
			CFlyLocationIPArray l_sqliteArray;
			l_sqliteArray.reserve(6000);
			
			auto parseValidLine = [](CFlyLocationIPArray & p_sqliteArray, const string & p_line, uint32_t p_startIp, uint32_t p_endIp) -> void
			{
				const string::size_type l_space = p_line.find(' ');
				if (l_space != string::npos)
				{
					string l_fullNetStr = p_line.substr(l_space + 1); //TODO Crash
					boost::trim(l_fullNetStr);
					const auto l_comma = l_fullNetStr.find(',');
					if (l_comma != string::npos)
					{
						const string l_networkName = l_fullNetStr.substr(l_comma + 1);
						//const auto l_index_offset = _countof(g_countryCodes) + 1;
						CFlyLocationIP l_item;
						l_item.m_start_ip = p_startIp;
						l_item.m_stop_ip  = p_endIp;
						l_item.m_dic_country_location_ip = CFlylinkDBManager::getInstance()->get_dic_location_id(l_networkName);
						l_item.m_flag_index = Util::toInt(l_fullNetStr.c_str());
						p_sqliteArray.push_back(l_item);
					}
					else
					{
						customLocationLog(p_line, STRING(COMMA_NOT_FOUND));
					}
				}
				else
				{
					customLocationLog(p_line, STRING(SPACE_NOT_FOUND));
				}
			};
			
			try
			{
				uint32_t a = 0, b = 0, c = 0, d = 0, a2 = 0, b2 = 0, c2 = 0, d2 = 0, n = 0;
				bool l_end_file;
				do
				{
					l_end_file = getline(l_file, l_currentLine).eof();
					
					if (!l_currentLine.empty() && isdigit((unsigned char)l_currentLine[0]))
					{
						if (l_currentLine.find('-') != string::npos && count(l_currentLine.begin(), l_currentLine.end(), '.') >= 6)
						{
							const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u-%u.%u.%u.%u", &a, &b, &c, &d, &a2, &b2, &c2, &d2);
							if (l_Items == 8)
							{
								const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
								const uint32_t l_endIP = (a2 << 24) + (b2 << 16) + (c2 << 8) + d2 + 1;
								if (l_startIP >= l_endIP)
								{
									customLocationLog(l_currentLine, STRING(INVALID_RANGE));
								}
								else
								{
									parseValidLine(l_sqliteArray, l_currentLine, l_startIP, l_endIP);
								}
							}
							else
							{
								customLocationLog(l_currentLine, STRING(MASK_NOT_FOUND) + " d.d.d.d-d.d.d.d");
							}
						}
						else if (l_currentLine.find('+') != string::npos && count(l_currentLine.begin(), l_currentLine.end(), '.') >= 3)
						{
							const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u+%u", &a, &b, &c, &d, &n);
							if (l_Items == 5)
							{
								const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
								parseValidLine(l_sqliteArray, l_currentLine, l_startIP, l_startIP + n);
							}
							else
							{
								customLocationLog(l_currentLine, STRING(MASK_NOT_FOUND) + " d.d.d.d+d");
							}
						}
					}
				}
				while (!l_end_file);
			}
			catch (const Exception& e)
			{
				customLocationLog(l_currentLine, "Parser fatal error:" + e.getError());
			}
			CFlyLog l_logSqlite("[CustomLocation-sqlite]");
			CFlylinkDBManager::getInstance()->save_location(l_sqliteArray);
			CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampCustomLocation, l_timeStampFile);
		}
		else
		{
			LogManager::getInstance()->message("Error open " + Text::fromT(l_fileName));
		}
	}
}

void Util::migrate(const string& file)
{
	if (g_localMode)
		return;
	if (File::getSize(file) != -1)
		return;
	string fname = getFileName(file);
	string old = g_paths[PATH_GLOBAL_CONFIG] + "Settings\\" + fname;
	if (File::getSize(old) == -1)
		return;
	File::renameFile(old, file);
}

void Util::loadBootConfig()
{
	// Load boot settings
	try
	{
		SimpleXML boot;
		boot.fromXML(File(getPath(PATH_GLOBAL_CONFIG) + "dcppboot.xml", File::READ, File::OPEN).read());
		boot.stepIn();
		
		if (boot.findChild("LocalMode"))
		{
			g_localMode = boot.getChildData() != "0";
		}
		boot.resetCurrentChild();
		// [!] FlylinkDC Dont merge this code from another projects!!!!!
		StringMap params;
#ifdef _WIN32
		// @todo load environment variables instead? would make it more useful on *nix
		
		params["APPDATA"] = RemovePathSeparator(getSysPath(APPDATA));
		params["LOCAL_APPDATA"] = RemovePathSeparator(getSysPath(LOCAL_APPDATA));
		params["COMMON_APPDATA"] = RemovePathSeparator(getSysPath(COMMON_APPDATA));
		params["PERSONAL"] = RemovePathSeparator(getSysPath(PERSONAL));
		params["PROGRAM_FILESX86"] = RemovePathSeparator(getSysPath(PROGRAM_FILESX86));
		params["PROGRAM_FILES"] = RemovePathSeparator(getSysPath(PROGRAM_FILES));
#endif
		
		if (boot.findChild("ConfigPath"))
		{
		
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA //[+] NightOrion
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
#endif
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#ifdef USE_APPDATA //[+] NightOrion
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		boot.resetCurrentChild();
		
		if (boot.findChild("SharedConfigPath"))
		{
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
		}
# endif
		boot.resetCurrentChild();
		
		if (boot.findChild("UserConfigPath"))
		{
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#endif
		// [~] FlylinkDC Dont merge this code from another projects!!!!!
	}
	catch (const Exception&)
	{
		//-V565
		// Unable to load boot settings...
	}
}

#ifdef _WIN32
static const char badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '/', '"', '|', '?', '*', 0
};
#else

static const char badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '\\', '"', '|', '?', '*', 0
};
#endif

/**
 * Replaces all strange characters in a file with '_'
 * @todo Check for invalid names such as nul and aux...
 */
string Util::validateFileName(string tmp)
{
	string::size_type i = 0;
	
	// First, eliminate forbidden chars
	while ((i = tmp.find_first_of(badChars, i)) != string::npos)
	{
		tmp[i] = '_';
		i++;
	}
	
	// Then, eliminate all ':' that are not the second letter ("c:\...")
	i = 0;
	while ((i = tmp.find(':', i)) != string::npos)
	{
		if (i == 1)
		{
			i++;
			continue;
		}
		tmp[i] = '_';
		i++;
	}
	
	// Remove the .\ that doesn't serve any purpose
	i = 0;
	while ((i = tmp.find("\\.\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	i = 0;
	while ((i = tmp.find("/./", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	
	// Remove any double \\ that are not at the beginning of the path...
	i = 1;
	while ((i = tmp.find("\\\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	i = 1;
	while ((i = tmp.find("//", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	
	// And last, but not least, the infamous ..\! ...
	i = 0;
	while (((i = tmp.find("\\..\\", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	i = 0;
	while (((i = tmp.find("/../", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	
	// Dots at the end of path names aren't popular
	i = 0;
	while (((i = tmp.find(".\\", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	i = 0;
	while (((i = tmp.find("./", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	
	
	return tmp;
}

string Util::cleanPathChars(string aNick)
{
	string::size_type i = 0;
	
	while ((i = aNick.find_first_of("/.\\", i)) != string::npos)
	{
		aNick[i] = '_';
	}
	return aNick;
}

string Util::getShortTimeString(time_t t)
{
	/*
	#ifdef _DEBUG
	    static int l_count = 0;
	    if (t == time(NULL))
	        dcdebug("\n\n\nUtil::getShortTimeString called with curent time. Count = %d\n\n\n", ++l_count);
	#endif
	*/
	tm* _tm = localtime(&t);
	if (_tm == NULL)
	{
		return "xx:xx";
	}
	else
	{
		string l_buf;
		l_buf.resize(255);
		l_buf.resize(strftime(&l_buf[0], l_buf.size(), SETTING(TIME_STAMPS_FORMAT).c_str(), _tm));
#ifdef _WIN32
		if (!Text::validateUtf8(l_buf))
			return Text::toUtf8(l_buf);
		else
			return l_buf;
#else
		return Text::toUtf8(l_buf);
#endif
	}
}

/**
 * Decodes a URL the best it can...
 * Default ports:
 * http:// -> port 80
 * https:// -> port 443
 * dchub:// -> port 411
 */
void Util::decodeUrl(const string& url, string& protocol, string& host, uint16_t& port, string& path, bool& isSecure, string& query, string& fragment)
{
	auto fragmentEnd = url.size();
	auto fragmentStart = url.rfind('#');
	
	size_t queryEnd;
	if (fragmentStart == string::npos)
	{
		queryEnd = fragmentStart = fragmentEnd;
	}
	else
	{
		dcdebug("f");
		queryEnd = fragmentStart;
		fragmentStart++;
	}
	
	auto queryStart = url.rfind('?', queryEnd);
	size_t fileEnd;
	
	if (queryStart == string::npos)
	{
		fileEnd = queryStart = queryEnd;
	}
	else
	{
		dcdebug("q");
		fileEnd = queryStart;
		queryStart++;
	}
	
	size_t protoStart = 0;
	auto protoEnd = url.find("://", protoStart);
	
	auto authorityStart = protoEnd == string::npos ? protoStart : protoEnd + 3;
	auto authorityEnd = url.find_first_of("/#?", authorityStart);
	
	size_t fileStart;
	if (authorityEnd == string::npos)
	{
		authorityEnd = fileStart = fileEnd;
	}
	else
	{
		dcdebug("a");
		fileStart = authorityEnd;
	}
	
	protocol = (protoEnd == string::npos ? Util::emptyString : Text::toLower(url.substr(protoStart, protoEnd - protoStart))); // [!] IRainman rfc fix lower string to proto and servername
	if (protocol.empty())
		protocol = "dchub";
		
	if (authorityEnd > authorityStart)
	{
		dcdebug("x");
		size_t portStart = string::npos;
		if (url[authorityStart] == '[')
		{
			// IPv6?
			auto hostEnd = url.find(']');
			if (hostEnd == string::npos)
			{
				return;
			}
			
			host = url.substr(authorityStart + 1, hostEnd - authorityStart - 1);
			if (hostEnd + 1 < url.size() && url[hostEnd + 1] == ':')
			{
				portStart = hostEnd + 2;
			}
		}
		else
		{
			size_t hostEnd;
			portStart = url.find(':', authorityStart);
			if (portStart != string::npos && portStart > authorityEnd)
			{
				portStart = string::npos;
			}
			
			if (portStart == string::npos)
			{
				hostEnd = authorityEnd;
			}
			else
			{
				hostEnd = portStart;
				portStart++;
			}
			
			dcdebug("h");
			host = Text::toLower(url.substr(authorityStart, hostEnd - authorityStart)); // [!] IRainman rfc fix lower string to proto and servername
		}
		
		if (portStart == string::npos)
		{
			if (protocol == "dchub")
			{
				port = 411;
			}
			else if (protocol == "http" || protocol == "steam")
			{
				port = 80;
			}
			else if (protocol == "https")
			{
				port = 443;
				isSecure = true;
			}
			else if (protocol == "mtasa")
			{
				port = 22004;
			}
			else if (protocol == "samp")
			{
				port = 7790;
			}
		}
		else
		{
			dcdebug("p");
			port = static_cast<uint16_t>(Util::toInt(url.substr(portStart, authorityEnd - portStart)));
		}
	}
	
	dcdebug("\n");
	path = url.substr(fileStart, fileEnd - fileStart);
	query = url.substr(queryStart, queryEnd - queryStart);
	fragment = url.substr(fragmentStart, fragmentEnd - fragmentStart);  //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2606
	if (!Text::isAscii(host))
	{
		static const BOOL success = IDNA_init(0);// [!] IRainman opt: no needs to reinit (+static const).
		if (success)
		{
			// http://www.rfc-editor.org/rfc/rfc3490.txt
			// [!] IRainman fix: no needs! string hostUTF8 = Text::utf8ToAcp(host);
			LocalArray<char, MAX_HOST_LEN> buff;
			size_t size = MAX_HOST_LEN;
			memzero(buff.data(), buff.size());
			memcpy(buff.data(), host.c_str(), min(buff.size(), host.size()));// [!] IRainman opt. add size
			if (IDNA_convert_to_ACE(buff.data(), &size))
			{
				host = buff.data();
			}
		}
	}
}

map<string, string> Util::decodeQuery(const string& query)
{
	map<string, string> ret;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
		{
			break;
		}
		
		auto param = eq + 1;
		auto end = query.find('&', param);
		
		if (end == string::npos)
		{
			end = query.size();
		}
		
		if (eq > start && end > param)
		{
			ret[query.substr(start, eq - start)] = query.substr(param, end - param);
		}
		
		start = end + 1;
	}
	
	return ret;
}


void Util::setAway(bool aAway, bool notUpdateInfo /*= false*/)
{
	g_away = aAway;
	
	SET_SETTING(AWAY, aAway);
	
	if (g_away)
		g_awayTime = time(NULL);
		
	if (!notUpdateInfo)
		ClientManager::getInstance()->infoUpdated(); // [+] IRainman fix.
}
// [~] InfinitySky. ������ � ��������������.
string Util::getAwayMessage(StringMap& params)
{
	time_t currentTime;
	time(&currentTime);
	int currentHour = localtime(&currentTime)->tm_hour;
	
	params["idleTI"] = formatSeconds(time(NULL) - g_awayTime);
	
	if (BOOLSETTING(AWAY_TIME_THROTTLE) && ((SETTING(AWAY_START) < SETTING(AWAY_END) &&
	                                         currentHour >= SETTING(AWAY_START) && currentHour < SETTING(AWAY_END)) ||
	                                        (SETTING(AWAY_START) > SETTING(AWAY_END) &&
	                                         (currentHour >= SETTING(AWAY_START) || currentHour < SETTING(AWAY_END)))))
	{
		return formatParams((g_awayMsg.empty() ? SETTING(SECONDARY_AWAY_MESSAGE) : g_awayMsg), params, false, g_awayTime);
	}
	else
	{
		return formatParams((g_awayMsg.empty() ? SETTING(DEFAULT_AWAY_MESSAGE) : g_awayMsg), params, false, g_awayTime);
	}
}

string Util::formatBytes(int64_t aBytes) // TODO fix copy-paste
{
	char buf[64];
	buf[0] = 0;
	if (aBytes < 1024)
	{
		snprintf(buf, _countof(buf), "%d %s", (int)aBytes & 0xffffffff, CSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1024.0), CSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1048576.0), CSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1073741824.0), CSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1099511627776.0), CSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1125899906842624.0), CSTRING(PB));
	}
	else
	{
		snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1152921504606846976.0), CSTRING(EB));
	}
	return buf;
}
string Util::formatBytes(double aBytes) // TODO fix copy-paste
{
	char buf[64];
	buf[0] = 0;
	if (aBytes < 1024)
	{
		snprintf(buf, _countof(buf), "%d %s", (int)aBytes & 0xffffffff, CSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1024.0), CSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1048576.0), CSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1073741824.0), CSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1099511627776.0), CSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1125899906842624.0), CSTRING(PB));
	}
	else
	{
		snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1152921504606846976.0), CSTRING(EB));
	}
	return buf;
}

wstring Util::formatBytesW(int64_t aBytes)
{
	wchar_t buf[64];
	if (aBytes < 1024)
	{
		snwprintf(buf, _countof(buf), L"%d %s", (int)(aBytes & 0xffffffff), CWSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1024.0), CWSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1048576.0), CWSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1073741824.0), CWSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1099511627776.0), CWSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1125899906842624.0), CWSTRING(PB));
	}
	else
	{
		snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1152921504606846976.0), CWSTRING(EB)); //TODO Crash beta-16
	}
	
	return buf;
}

wstring Util::formatExactSize(int64_t aBytes)
{
#ifdef _WIN32
	wchar_t l_number[64];
	l_number[0] = 0;
	snwprintf(l_number, _countof(l_number), _T(I64_FMT), aBytes);
	wchar_t l_buf_nf[64];
	l_buf_nf[0] = 0;
	GetNumberFormat(LOCALE_USER_DEFAULT, 0, l_number, &g_nf, l_buf_nf, _countof(l_buf_nf));
	snwprintf(l_buf_nf, _countof(l_buf_nf), _T("%s %s"), l_buf_nf, CWSTRING(B));
	return l_buf_nf;
#else
	wchar_t buf[64];
	snwprintf(buf, _countof(buf), _T(I64_FMT), (long long int)aBytes);
	return tstring(buf) + TSTRING(B);
#endif
}

string Util::getLocalIp()
{
	string tmp;
	char buf[256];
	if (!gethostname(buf, 255))
	{
		const hostent* he = gethostbyname(buf);
		if (he == nullptr || he->h_addr_list[0] == 0)
			return Util::emptyString;
		sockaddr_in dest;
		int i = 0;
		
		// We take the first ip as default, but if we can find a better one, use it instead...
		memcpy(&(dest.sin_addr), he->h_addr_list[i++], he->h_length);
		tmp = inet_ntoa(dest.sin_addr);
		if (Util::isPrivateIp(tmp) || strncmp(tmp.c_str(), "169", 3) == 0)
		{
			while (he->h_addr_list[i])
			{
				memcpy(&(dest.sin_addr), he->h_addr_list[i], he->h_length);
				string tmp2 = inet_ntoa(dest.sin_addr);
				if (!Util::isPrivateIp(tmp2) && strncmp(tmp2.c_str(), "169", 3) != 0)
				{
					tmp = tmp2;
				}
				i++;
			}
		}
	}
	return tmp;
}

bool Util::isPrivateIp(const string& ip)
{
	struct in_addr addr;
	
	addr.s_addr = inet_addr(ip.c_str());
	
	if (addr.s_addr  != INADDR_NONE)
	{
		unsigned long haddr = ntohl(addr.s_addr);
		return ((haddr & 0xff000000) == 0x0a000000 || // 10.0.0.0/8
		        (haddr & 0xff000000) == 0x7f000000 || // 127.0.0.0/8
		        (haddr & 0xffff0000) == 0xa9fe0000 || // 169.254.0.0/16
		        (haddr & 0xfff00000) == 0xac100000 || // 172.16.0.0/12
		        (haddr & 0xffff0000) == 0xc0a80000);  // 192.168.0.0/16
	}
	return false;
}

typedef const uint8_t* ccp;
static wchar_t utf8ToLC(ccp& str)
{
	wchar_t c = 0;
	if (str[0] & 0x80)
	{
		if (str[0] & 0x40)
		{
			if (str[0] & 0x20)
			{
				if (str[1] == 0 || str[2] == 0 ||
				        !((((unsigned char)str[1]) & ~0x3f) == 0x80) ||
				        !((((unsigned char)str[2]) & ~0x3f) == 0x80))
				{
					str++;
					return 0;
				}
				c = ((wchar_t)(unsigned char)str[0] & 0xf) << 12 |
				    ((wchar_t)(unsigned char)str[1] & 0x3f) << 6 |
				    ((wchar_t)(unsigned char)str[2] & 0x3f);
				str += 3;
			}
			else
			{
				if (str[1] == 0 ||
				        !((((unsigned char)str[1]) & ~0x3f) == 0x80))
				{
					str++;
					return 0;
				}
				c = ((wchar_t)(unsigned char)str[0] & 0x1f) << 6 |
				    ((wchar_t)(unsigned char)str[1] & 0x3f);
				str += 2;
			}
		}
		else
		{
			str++;
			return 0;
		}
	}
	else
	{
		c = Text::asciiToLower((char)str[0]);
		str++;
		return c;
	}
	
	return Text::toLower(c);
}

string Util::toString(const string& p_sep, const StringList& p_lst)
{
	string ret;
	for (auto i = p_lst.cbegin(), iend = p_lst.cend(); i != iend; ++i)
	{
		ret += *i;
		if (i + 1 != iend)
			ret += p_sep;
	}
	return ret;
}

string Util::toString(const StringList& lst)
{
	if (lst.size() == 1)
		return lst[0];
	string tmp("[");
	for (auto i = lst.cbegin(), iend = lst.cend(); i != iend; ++i)
	{
		tmp += *i + ',';
	}
	if (tmp.length() == 1)
		tmp.push_back(']');
	else
		tmp[tmp.length() - 1] = ']';
	return tmp;
}

string::size_type Util::findSubString(const string& aString, const string& aSubString, string::size_type start) noexcept
{
	if (aString.length() < start)
		return (string::size_type)string::npos;
		
	if (aString.length() - start < aSubString.length())
		return (string::size_type)string::npos;
		
	if (aSubString.empty())
		return 0;
		
	// Hm, should start measure in characters or in bytes? bytes for now...
	const uint8_t* tx = (const uint8_t*)aString.c_str() + start;
	const uint8_t* px = (const uint8_t*)aSubString.c_str();
	
	const uint8_t* end = tx + aString.length() - start - aSubString.length() + 1;
	
	wchar_t wp = utf8ToLC(px);
	
	while (tx < end)
	{
		const uint8_t* otx = tx;
		if (wp == utf8ToLC(tx))
		{
			const uint8_t* px2 = px;
			const uint8_t* tx2 = tx;
			
			for (;;)
			{
				if (*px2 == 0)
					return otx - (uint8_t*)aString.c_str();
					
				if (utf8ToLC(px2) != utf8ToLC(tx2))
					break;
			}
		}
	}
	return (string::size_type)string::npos;
}

wstring::size_type Util::findSubString(const wstring& aString, const wstring& aSubString, wstring::size_type pos) noexcept
{
	if (aString.length() < pos)
		return static_cast<wstring::size_type>(wstring::npos);
		
	if (aString.length() - pos < aSubString.length())
		return static_cast<wstring::size_type>(wstring::npos);
		
	if (aSubString.empty())
		return 0;
		
	wstring::size_type j = 0;
	wstring::size_type end = aString.length() - aSubString.length() + 1;
	
	for (; pos < end; ++pos)
	{
		if (Text::toLower(aString[pos]) == Text::toLower(aSubString[j]))
		{
			wstring::size_type tmp = pos + 1;
			bool found = true;
			for (++j; j < aSubString.length(); ++j, ++tmp)
			{
				if (Text::toLower(aString[tmp]) != Text::toLower(aSubString[j]))
				{
					j = 0;
					found = false;
					break;
				}
			}
			
			if (found)
				return pos;
		}
	}
	return static_cast<wstring::size_type>(wstring::npos);
}

string Util::encodeURI(const string& aString, bool reverse)
{
	// reference: rfc2396
	string tmp = aString;
	if (reverse)
	{
		// TODO idna: convert host name from punycode
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp.length() > idx + 2 && tmp[idx] == '%' && isxdigit(tmp[idx + 1]) && isxdigit(tmp[idx + 2]))
			{
				tmp[idx] = fromHexEscape(tmp.substr(idx + 1, 2));
				tmp.erase(idx + 1, 2);
			}
			else   // reference: rfc1630, magnet-uri draft
			{
				if (tmp[idx] == '+')
					tmp[idx] = ' ';
			}
		}
	}
	else
	{
		static const string disallowed = ";/?:@&=+$," // reserved
		                                 "<>#%\" "    // delimiters
		                                 "{}|\\^[]`"; // unwise
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp[idx] == ' ')
			{
				tmp[idx] = '+';
			}
			else
			{
				if (tmp[idx] <= 0x1F || tmp[idx] >= 0x7f || (disallowed.find_first_of(tmp[idx])) != string::npos)
				{
					tmp.replace(idx, 1, toHexEscape(tmp[idx]));
					idx += 2;
				}
			}
		}
	}
	return tmp;
}

/**
 * This function takes a string and a set of parameters and transforms them according to
 * a simple formatting rule, similar to strftime. In the message, every parameter should be
 * represented by %[name]. It will then be replaced by the corresponding item in
 * the params stringmap. After that, the string is passed through strftime with the current
 * date/time and then finally written to the log file. If the parameter is not present at all,
 * it is removed from the string completely...
 */
string Util::formatParams(const string& msg, const StringMap& params, bool filter, const time_t t)
{
	string result = msg;
	
	string::size_type c = 0;
	static const string g_goodchars = "aAbBcdHIjmMpSUwWxXyYzZ%";
	bool l_find_alcohol = false;
	while ((c = result.find('%', c)) != string::npos)
	{
		l_find_alcohol = true;
		if (c < result.length() - 1)
		{
			if (g_goodchars.find(result[c + 1], 0) == string::npos) // [6] https://www.box.net/shared/68bcb4f96c1b5c39f12d
			{
				result.replace(c, 1, "%%");
				c++;
			}
			c++;
		}
		else
		{
			result.replace(c, 1, "%%");
			break;
		}
	}
	if (l_find_alcohol) // �� �������� ������ %[ �.�. �� ����� %
	{
		result = formatTime(result, t); // http://code.google.com/p/flylinkdc/issues/detail?id=1015
		string::size_type i, j, k;
		i = 0;
		while ((j = result.find("%[", i)) != string::npos)
		{
			// [!] IRainman fix.
			if (result.size() < j + 2)
				break;
				
			if ((k = result.find(']', j + 2)) == string::npos)
			{
				result.replace(j, 2, ""); // [+] IRainman: invalid shablon fix - auto correction.
				break;
			}
			// [~] IRainman fix.
			string name = result.substr(j + 2, k - j - 2);
			const auto& smi = params.find(name);
			if (smi == params.end())
			{
				result.erase(j, k - j + 1);
				i = j;
			}
			else
			{
				if (smi->second.find_first_of("\\./") != string::npos)
				{
					string tmp = smi->second;
					
					if (filter)
					{
						// Filter chars that produce bad effects on file systems
						c = 0;
#ifdef _WIN32 // !SMT!-f add windows special chars
						static const char badchars[] = "\\./:*?|<>";
#else // unix is more tolerant
						static const char badchars[] = "\\./";
#endif
						while ((c = tmp.find_first_of(badchars, c)) != string::npos)
						{
							tmp[c] = '_';
						}
					}
					
					result.replace(j, k - j + 1, tmp);
					i = j + tmp.size();
				}
				else
				{
					result.replace(j, k - j + 1, smi->second);
					i = j + smi->second.size();
				}
			}
		}
	}
	return result;
}

string Util::formatRegExp(const string& msg, const StringMap& params)
{
	string result = msg;
	string::size_type i, j, k;
	i = 0;
	while ((j = result.find("%[", i)) != string::npos)
	{
		if ((result.size() < j + 2) || ((k = result.find(']', j + 2)) == string::npos))
		{
			break;
		}
		const string name = result.substr(j + 2, k - j - 2);
		const auto& smi = params.find(name);
		if (smi != params.end())
		{
			result.replace(j, k - j + 1, smi->second);
			i = j + smi->second.size();
		}
		else
		{
			i = k + 1;
		}
	}
	return result;
}

uint64_t Util::getDirSize(const string &sFullPath)
{
	uint64_t total = 0;
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFile(Text::toT(sFullPath + "\\*").c_str(), &fData);
	
	if (hFind != INVALID_HANDLE_VALUE)
	{
		const string l_tmp_path = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		do
		{
			if ((fData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && !BOOLSETTING(SHARE_HIDDEN))
				continue;
			const string name = Text::fromT(fData.cFileName);
			if (name == Util::m_dot || name == Util::m_dot_dot)
				continue;
			if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				const string newName = sFullPath + PATH_SEPARATOR + name;
				// TODO TEMP_DOWNLOAD_DIRECTORY ����� ��������� ������ "[targetdrive]" ���������� � ��� �� ������ �����
				if (stricmp(newName + PATH_SEPARATOR, l_tmp_path) != 0)
				{
					total += getDirSize(newName);
				}
			}
			else
			{
				total += (uint64_t)fData.nFileSizeLow | ((uint64_t)fData.nFileSizeHigh) << 32;
			}
		}
		while (FindNextFile(hFind, &fData));
		FindClose(hFind);
	}
	return total;
}

bool Util::validatePath(const string &sPath)
{
	if (sPath.empty())
		return false;
		
	if ((sPath.substr(1, 2) == ":\\") || (sPath.substr(0, 2) == "\\\\"))
	{
		if (GetFileAttributes(Text::toT(sPath).c_str()) & FILE_ATTRIBUTE_DIRECTORY)
			return true;
	}
	
	return false;
}
/* [!] IRainman fix: see File::isExist for details
bool Util::fileExists(const tstring &aFile)
{
    DWORD attr = GetFileAttributes(aFile.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);// [!] IRainman fixs magical number
}
*/
// [+] SSA
string Util::getFilenameForRenaming(const string& p_filename)
{
	string outFilename;
	const string ext = getFileExt(p_filename);
	const string fname = getFileName(p_filename);
	int i = 0;
	do
	{
		i++;
		outFilename = p_filename.substr(0, p_filename.length() - fname.length());
		outFilename += fname.substr(0, fname.length() - ext.length());
		outFilename += '(' + Util::toString(i) + ')';
		outFilename += ext;
	}
	while (File::isExist(outFilename));
	
	return outFilename;
}
//[+]FlylinkDC++ Team
string Util::formatDigitalClock(const string &p_msg, const time_t p_t)
{
	/*
	#ifdef _DEBUG
	    static int l_count = 0;
	    if (p_t == time(NULL))
	        dcdebug("\n\n\nUtil::formatDigitalClock called with curent time. Count = %d\n\n\n", ++l_count);
	#endif
	*/
	tm* l_loc = localtime(&p_t);
	if (!l_loc)
	{
		return Util::emptyString;
	}
	const size_t l_bufsize = p_msg.size() + 15;
	string l_buf;
	l_buf.resize(l_bufsize + 1);
	const size_t l_len = strftime(&l_buf[0], l_bufsize, p_msg.c_str(), l_loc);
	if (!l_len)
		return p_msg;
	else
	{
		l_buf.resize(l_len);
		return l_buf;
	}
}
//[~]FlylinkDC++ Team
string Util::formatTime(const string &p_msg, const time_t p_t)
{
	/*
	#ifdef _DEBUG
	    static int l_count = 0;
	    if (t == time(NULL))
	        dcdebug("\n\n\nUtil::formatTime(1) called with curent time. Count = %d\n\n\n", ++l_count);
	#endif
	*/
	if (!p_msg.empty())
	{
		tm* l_loc = localtime(&p_t);
		if (!l_loc)
		{
			return Util::emptyString;
		}
		// [!] IRainman fix.
		const string l_msgAnsi = Text::fromUtf8(p_msg);
		size_t bufsize = l_msgAnsi.size() + 256;
		string buf;
		buf.resize(bufsize + 1);
		while (true)
		{
			const size_t l_len = strftime(&buf[0], bufsize, l_msgAnsi.c_str(), l_loc);
			if (l_len)
			{
				buf.resize(l_len);
#ifdef _WIN32
				if (!Text::validateUtf8(buf))
#endif
				{
					buf = Text::toUtf8(buf);
				}
				return buf;
			}
			
			if (errno == EINVAL
			        || bufsize > l_msgAnsi.size() + 1024) // [+] IRainman fix.
				return Util::emptyString;
				
			bufsize += 64;
			buf.resize(bufsize);
		}
		// [~] IRainman fix.
	}
	return Util::emptyString;
}

string Util::formatTime(uint64_t rest, const bool withSecond /*= true*/)
{
#define formatTimeformatInterval(n) snprintf(buf, _countof(buf), first ? "%I64u " : " %I64u ", n);\
	/*[+] PVS Studio V576 Incorrect format. Consider checking the fourth actual argument of the '_snprintf' function. The argument is expected to be not greater than 32-bit.*/\
	formatedTime += (string)buf;\
	first = false

	/*
	#ifdef _DEBUG
	    static int l_count = 0;
	    if (rest == time(NULL))
	        dcdebug("\n\n\nUtil::formatTime(2) called with curent time. Count = %d\n\n\n", ++l_count);
	#endif
	*/
	char buf[32];
	string formatedTime;
	uint64_t n;
	uint8_t i = 0;
	bool first = true;
	n = rest / (24 * 3600 * 7);
	rest %= (24 * 3600 * 7);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(WEEKS) : STRING(WEEK);
		i++;
	}
	n = rest / (24 * 3600);
	rest %= (24 * 3600);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(DAYS) : STRING(DAY);
		i++;
	}
	n = rest / (3600);
	rest %= (3600);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(HOURS) : STRING(HOUR);
		i++;
	}
	n = rest / (60);
	rest %= (60);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += STRING(MIN);
		i++;
	}
	if (withSecond && i <= 2)
	{
		if (rest)
		{
			formatTimeformatInterval(rest);
			formatedTime += STRING(SEC);
		}
	}
	return formatedTime;
	
#undef formatTimeformatInterval
}

/* Below is a high-speed random number generator with much
   better granularity than the CRT one in msvc...(no, I didn't
   write it...see copyright) */
/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
   Any feedback is very welcome. For any question, comments,
   see http://www.math.keio.ac.jp/matumoto/emt.html or email
   matumoto@math.keio.ac.jp */
/* Period parameters */

// TODO ������ ���������� �����!!!
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

static std::vector<unsigned long> g_mt(N); /* the array for the state vector  */
static int g_mti = N + 1; /* mti==N+1 means mt[N] is not initialized */

/* initializing the array with a NONZERO seed */
static void sgenrand(unsigned long seed)
{
	/* setting initial seeds to mt[N] using         */
	/* the generator Line 25 of Table 1 in          */
	/* [KNUTH 1981, The Art of Computer Programming */
	/*    Vol. 2 (2nd Ed.), pp102]                  */
	g_mt[0] = seed & ULONG_MAX;
	for (g_mti = 1; g_mti < N; g_mti++)
		g_mt[g_mti] = (69069 * g_mt[g_mti - 1]) & ULONG_MAX;
}

uint32_t Util::rand()
{
	unsigned long y;
	/* mag01[x] = x * MATRIX_A  for x=0,1 */
	
	if (g_mti >= N)   /* generate N words at one time */
	{
		static unsigned long mag01[2] = {0x0, MATRIX_A};
		int kk;
		
		if (g_mti == N + 1) /* if sgenrand() has not been called, */
			sgenrand(4357); /* a default initial seed is used   */
			
		for (kk = 0; kk < N - M; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + M] ^(y >> 1) ^ mag01[y & 0x1];
		}
		for (; kk < N - 1; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + (M - N)] ^(y >> 1) ^ mag01[y & 0x1];
		}
		y = (g_mt[N - 1] & UPPER_MASK) | (g_mt[0] & LOWER_MASK);
		g_mt[N - 1] = g_mt[M - 1] ^(y >> 1) ^ mag01[y & 0x1];
		
		g_mti = 0;
	}
	
	y = g_mt[g_mti++];
	y ^= TEMPERING_SHIFT_U(y);
	y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
	y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
	y ^= TEMPERING_SHIFT_L(y);
	
	return y;
}
//[+]FlylinkDC++ Team
string Util::getRandomNick()
{
	// Create RND nick
	static const size_t iNickLength = 20;
	static const char samples[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static const char samples2[] = "_-";
	static const string samples3[] = { "User", "Sun", "Moon", "Monkey", "Boys", "Girls", "True", "Head", "Hulk", "Hawk", "Fire", "Rabbit", "Water", "Smile", "Bear", "Cow", "Troll", "Cool", "Man", "Men", "Women", "Onotole", "Hedgehog", };
	
	string name = samples3[Util::rand(_countof(samples3) - 1)];
	name += samples2[Util::rand(_countof(samples2) - 1)];
	
	for (size_t i = Util::rand(3, 7); i; --i)
		name += samples[Util::rand(_countof(samples) - 1)];
		
	if (name.size() > iNickLength)
		name.resize(iNickLength);
		
	return name;
}
//======================================================================================================================================
tstring Util::CustomNetworkIndex::getDescription() const
{
	if (m_location_cache_index)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_location_from_cache(m_location_cache_index);
		return l_res.m_description;
	}
	else if (m_country_cache_index)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_country_from_cache(m_country_cache_index);
		return l_res.m_description;
	}
	else
		return Util::emptyStringT;
}
//======================================================================================================================================
int16_t Util::CustomNetworkIndex::getFlagIndex() const
{
	if (m_location_cache_index)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_location_from_cache(m_location_cache_index);
		return l_res.m_flag_index;
	}
	else
	{
		return 0;
	}
}
//======================================================================================================================================
uint8_t Util::CustomNetworkIndex::getCountryIndex() const
{
	if (m_country_cache_index)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_country_from_cache(m_country_cache_index);
		return l_res.m_flag_index;
	}
	else
	{
		return 0;
	}
}
//======================================================================================================================================
/*  getIpCountry
    This function returns the full country name of an ip and abbreviation
    for exemple: it returns { "PT", "Portugal" or ImageIndex, "Provider name" }.
    more info: http://www.maxmind.com/app/csv
*/
// [!] IRainman: this function workings fine. Please don't merge from other project!
Util::CustomNetworkIndex Util::getIpCountry(const string& p_ip)
{
	const uint32_t l_ipNum = Socket::convertIP4(p_ip);
	if (l_ipNum)
	{
		uint8_t l_country_index = 0;
		CFlylinkDBManager::getInstance()->get_country(l_ipNum, l_country_index);
		uint32_t  l_location_index = 0;
		CFlylinkDBManager::getInstance()->get_location(l_ipNum, l_location_index);
		if (l_location_index > 0)
		{
			const CustomNetworkIndex l_index(l_location_index, l_country_index);
			return l_index;
		}
		else
		{
			if (g_fly_server_config.isCollectLostLocation() && BOOLSETTING(AUTOUPDATE_CUSTOMLOCATION) && AutoUpdate::getInstance()->isUpdate())
			{
				CFlylinkDBManager::getInstance()->save_lost_location(p_ip);
			}
		}
		if (l_country_index)
		{
			const CustomNetworkIndex l_index(l_location_index, l_country_index);
			return l_index;
		}
	}
	else
	{
		dcdebug(string("Invalid IP on Util::getIpCountry: " + p_ip + '\n').c_str());
		dcassert(!l_ipNum);
	}
	static const CustomNetworkIndex g_unknownLocationIndex(0, 0);
	return g_unknownLocationIndex;
}
//======================================================================================================================================
string Util::toAdcFile(const string& file)
{
	if (file == "files.xml.bz2" || file == "files.xml")
		return file;
		
	string ret;
	ret.reserve(file.length() + 1);
	ret += '/';
	ret += file;
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '\\')
		{
			ret[i] = '/';
		}
	}
	return ret;
}
string Util::toNmdcFile(const string& file)
{
	if (file.empty())
		return Util::emptyString;
		
	string ret(file.substr(1));
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '/')
		{
			ret[i] = '\\';
		}
	}
	return ret;
}

string Util::getIETFLang()
{
	string l_lang = SETTING(LANGUAGE_FILE);
	boost::replace_last(l_lang, ".xml", "");
	
	return l_lang;
}

string Util::translateError(int aError)
{
#ifdef _WIN32
#ifdef NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
	switch (aError)
	{
		case WSAEADDRNOTAVAIL   :
			return STRING(SOCKET_ERROR_WSAEADDRNOTAVAIL);
		case WSAENETDOWN        :
			return STRING(SOCKET_ERROR_WSAENETDOWN);
		case WSAENETUNREACH     :
			return STRING(SOCKET_ERROR_WSAENETUNREACH);
		case WSAENETRESET       :
			return STRING(SOCKET_ERROR_WSAENETRESET);
		case WSAECONNABORTED    :
			return STRING(SOCKET_ERROR_WSAECONNABORTED);
		case WSAECONNRESET      :
			return STRING(SOCKET_ERROR_WSAECONNRESET);
		case WSAENOBUFS         :
			return STRING(SOCKET_ERROR_WSAENOBUFS);
		case WSAEISCONN         :
			return STRING(SOCKET_ERROR_WSAEISCONN);
		case WSAETIMEDOUT       :
			return STRING(SOCKET_ERROR_WSAETIMEDOUT);
		case WSAECONNREFUSED    :
			return STRING(SOCKET_ERROR_WSAECONNREFUSED);
		case WSAELOOP           :
			return STRING(SOCKET_ERROR_WSAELOOP);
		case WSAENAMETOOLONG    :
			return STRING(SOCKET_ERROR_WSAENAMETOOLONG);
		case WSAEHOSTDOWN       :
			return STRING(SOCKET_ERROR_WSAEHOSTDOWN);
		default:
#endif //NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
			DWORD l_formatMessageFlag =
			    FORMAT_MESSAGE_ALLOCATE_BUFFER |
			    FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_IGNORE_INSERTS;
			    
			LPCVOID lpSource;
			// http://code.google.com/p/flylinkdc/issues/detail?id=1077
			// http://stackoverflow.com/questions/2159458/why-is-formatmessage-failing-to-find-a-message-for-wininet-errors/2159488#2159488
			if (aError >= INTERNET_ERROR_BASE && aError < INTERNET_ERROR_LAST)
			{
				l_formatMessageFlag |= FORMAT_MESSAGE_FROM_HMODULE;
				lpSource = GetModuleHandle(_T("wininet.dll"));
			}
			/*
			else if (aError >= XXX && aError < YYY)
			{
			TODO: Load text for errors from other libraries?
			}
			*/
			else
			{
				lpSource = NULL;
			}
			
			LPTSTR lpMsgBuf = 0;
			DWORD chars = FormatMessage(
			                  l_formatMessageFlag,
			                  lpSource,
			                  aError,
#if defined (_CONSOLE) || defined (_DEBUG)
			                  MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_US), // US
#else
			                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
#endif
			                  (LPTSTR) &lpMsgBuf,
			                  0,
			                  NULL
			              );
			string tmp;
			if (chars != 0)
			{
				tmp = Text::fromT(lpMsgBuf);
				// Free the buffer.
				LocalFree(lpMsgBuf);
				string::size_type i = 0;
				
				while ((i = tmp.find_first_of("\r\n", i)) != string::npos)
				{
					tmp.erase(i, 1);
				}
			}
			tmp += "[error: " + toString(aError) + "]";
			return tmp;
#else // _WIN32
	return Text::toUtf8(strerror(aError));
#endif // _WIN32
#ifdef NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
	}
#endif //NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
}

TCHAR* Util::strstr(const TCHAR *str1, const TCHAR *str2, int *pnIdxFound)
{
	TCHAR *s1, *s2;
	TCHAR *cp = const_cast<TCHAR*>(str1);
	if (!*str2)
		return const_cast<TCHAR*>(str1);
	int nIdx = 0;
	while (*cp)
	{
		s1 = cp;
		s2 = (TCHAR *) str2;
		while (*s1 && *s2 && !(*s1 - *s2))
			s1++, s2++;
		if (!*s2)
		{
			if (pnIdxFound != NULL)
				*pnIdxFound = nIdx;
			return cp;
		}
		cp++;
		nIdx++;
	}
	if (pnIdxFound != NULL)
		*pnIdxFound = -1;
	return nullptr;
}

/* natural sorting */

int Util::DefaultSort(const wchar_t *a, const wchar_t *b, bool noCase /*=  true*/)
{
	/*
	if(BOOLSETTING(NAT_SORT))
	    //[-]PPA TODO
	{
	
	    int v1, v2;
	    while (*a != 0 && *b != 0)
	    {
	        v1 = 0;
	        v2 = 0;
	        bool t1 = isNumeric(*a);
	        bool t2 = isNumeric(*b);
	        if (t1 != t2) return (t1) ? -1 : 1;
	
	        if (!t1 && noCase)
	        {
	            if (Text::toLower(*a) != Text::toLower(*b))
	                return ((int)Text::toLower(*a)) - ((int)Text::toLower(*b));
	            a++;
	            b++;
	        }
	        else if (!t1)
	        {
	            if (*a != *b)
	                return ((int)*a) - ((int)*b);
	            a++;
	            b++;
	        }
	        else
	        {
	            while (isNumeric(*a))
	            {
	                v1 *= 10;
	                v1 += *a - '0';
	                a++;
	            }
	
	            while (isNumeric(*b))
	            {
	                v2 *= 10;
	                v2 += *b - '0';
	                b++;
	            }
	
	            if (v1 != v2)
	                return (v1 < v2) ? -1 : 1;
	        }
	    }
	
	    return noCase ? (((int)Text::toLower(*a)) - ((int)Text::toLower(*b))) : (((int)*a) - ((int)*b));
	
	
	    // [+] brain-ripper
	    // TODO:
	    // implement dynamic call to StrCmpLogicalW (this function not exist on Win2000.
	    // Note that this function is case insensitive
	    // return StrCmpLogicalW(a, b);
	
	}
	else*/
	{
		return noCase ? lstrcmpi(a, b) : lstrcmp(a, b);
	}
}
/* [-] IRainman fix
string Util::formatMessage(const string& message)
{
    string tmp = message;
    // Check all '<' and '[' after newlines as they're probably pasts...
    size_t i = 0;
    while ((i = tmp.find('\n', i)) != string::npos)
    {
        if (i + 1 < tmp.length())
        {
            if (tmp[i + 1] == '[' || tmp[i + 1] == '<')
            {
                tmp.insert(i + 1, "- ");
                i += 2;
            }
        }
        i++;
    }
    return Text::toDOS(tmp);
}
*/
void Util::setLimiter(bool aLimiter)
{
	SET_SETTING(THROTTLE_ENABLE, aLimiter);
	ClientManager::getInstance()->infoUpdated(); // [!] IRainman fix
}

std::string Util::getRegistryCommaSubkey(const tstring& p_key)
{
	std::string l_result;
	std::string l_sep;
	HKEY l_hk = nullptr;
	TCHAR l_buf[512];
	l_buf[0] = 0;
	tstring l_key =  FLYLINKDC_REGISTRY_PATH _T("\\");
	l_key += p_key;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_key.c_str(), 0, KEY_READ, &l_hk) == ERROR_SUCCESS)
	{
		DWORD l_dwIndex = 0;
		DWORD l_len = _countof(l_buf);
		while (RegEnumValue(l_hk, l_dwIndex++, l_buf, &l_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			l_result += l_sep + Text::fromT(l_buf);
			l_len = _countof(l_buf);
			if (l_sep.empty())
				l_sep = ',';
		}
		::RegCloseKey(l_hk);
	}
	return l_result;
}

string Util::getRegistryValueString(const tstring& p_key, bool p_is_path)
{
	HKEY hk = nullptr;
	TCHAR l_buf[512];
	l_buf[0] = 0;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD l_bufLen = sizeof(l_buf);
		::RegQueryValueEx(hk, p_key.c_str(), NULL, NULL, (LPBYTE)l_buf, &l_bufLen);
		::RegCloseKey(hk);
		if (l_bufLen)
		{
			string l_result = Text::fromT(l_buf);
			if (p_is_path)
				AppendPathSeparator(l_result); //[+]PPA
			return l_result;
		}
	}
	return emptyString;
}

bool Util::deleteRegistryValue(const tstring& p_value)
{
	HKEY hk = nullptr;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL) != ERROR_SUCCESS)
	{
		return false;
	}
	const LSTATUS status = ::RegDeleteValue(hk, p_value.c_str());
	::RegCloseKey(hk);
	dcassert(status == ERROR_SUCCESS);
	return status == ERROR_SUCCESS;
}
// [+] SSA
bool Util::setRegistryValueString(const tstring& p_key, const tstring& p_value)
{
	HKEY hk = nullptr;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL) != ERROR_SUCCESS)
	{
		return false;
	}
	const LSTATUS status = ::RegSetValueEx(hk, p_key.c_str(), NULL, REG_SZ, (LPBYTE)p_value.c_str(), sizeof(TCHAR) * (p_value.length() + 1));
	::RegCloseKey(hk);
	dcassert(status == ERROR_SUCCESS);
	return status == ERROR_SUCCESS;
}

string Util::getExternalIP(const string& p_url, LONG p_timeOut /* = 500 */)
{
	CFlyLog l_log("[GetIP]");
	string l_downBuf;
	getDataFromInet(_T("GetIP"), 256, p_url, l_downBuf, p_timeOut);
	if (!l_downBuf.empty())
	{
		SimpleXML xml;
		try
		{
			xml.fromXML(l_downBuf);
			if (xml.findChild("html"))
			{
				xml.stepIn();
				if (xml.findChild("body"))
				{
					const string l_IP = xml.getChildData().substr(20);
					l_log.step("Download : " + p_url + " IP = " + l_IP);
					if(isValidIP(l_IP))
					{
					return l_IP;
				}
					else
					{
						dcassert(0);
					}
				}
			}
		}
		catch (SimpleXMLException & e)
		{
			l_log.step(string("Error parse XML: ") + e.what());
		}
	}
	else
		l_log.step("Error download : " + Util::translateError(GetLastError()));
	return Util::emptyString;
}
//[+] SSA
size_t Util::getDataFromInet(LPCWSTR agent, const DWORD frameBufferSize, const string& url, string& data, LONG timeOut /*=0*/, IDateReceiveReporter* reporter /* = NULL */)
{
	// FlylinkDC++ Team TODO: http://code.google.com/p/flylinkdc/issues/detail?id=632
	dcassert(!url.empty());
	if (url.empty())
		return 0;
	size_t totalBytesRead = 0;
	DWORD numberOfBytesRead = 0;
	DWORD numberOfBytesToRead = frameBufferSize;
	
	std::vector<char> pTempData(frameBufferSize + 1);
	pTempData[0] = 0;
	CInternetHandle hInternet(InternetOpen(agent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	if (!hInternet)
		return 0;
	if (timeOut)
		InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeOut, sizeof(timeOut));
	CInternetHandle hURL(InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 1)); // https://www.box.net/shared/096fe84f3e7e68666a3e
	// 2012-04-29_13-38-26_QSBM3KCUOSEPDSQX3BQ6IG7MTJ3QRPGV62TS46I_F9A54D9E_crash-stack-r501-build-9869.dmp
	if (!hURL)
	{
		LogManager::getInstance()->message("InternetOpenUrl error = " + Util::translateError(GetLastError()));
		return 0;
	}
	bool isUserCancel = false;
	while (InternetReadFile(hURL, pTempData.data(), numberOfBytesToRead, &numberOfBytesRead))
	{
		if (numberOfBytesRead == 0)
		{
			// EOF detected
			break;
		}
		else
		{
			// Copy partial data
			pTempData[numberOfBytesRead] = '\0';
			data += pTempData.data();
			totalBytesRead += numberOfBytesRead;
			if (reporter)
			{
				if (!reporter->ReportResultReceive(numberOfBytesRead))
				{
					isUserCancel = true;
					break;
				}
			}
		}
	}
	if (isUserCancel)
	{
		data = emptyString;
		totalBytesRead = 0;
	}
	return totalBytesRead;
}

#if 0
uint64_t Util::getDataFromInet(LPCWSTR agent, const DWORD frameBufferSize, const string& url, File& fileout, LONG timeOut/* = 0*/, IDateReceiveReporter* reporter/* = NULL*/)
{
	// FlylinkDC++ Team TODO: http://code.google.com/p/flylinkdc/issues/detail?id=632
	dcassert(!url.empty());
	if (url.empty())
		return 0;
	uint64_t totalBytesRead = 0;
	DWORD numberOfBytesRead = 0;
	DWORD numberOfBytesToRead = frameBufferSize;
	// vector<byte*> vecOut(numberOfBytesToRead);
	
	AutoArray<uint8_t> pTempData(frameBufferSize + 1);
	CInternetHandle  hInternet(InternetOpen(agent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	if (!hInternet)
		return 0;
		
	if (timeOut)
		InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeOut, sizeof(timeOut));
		
	CInternetHandle hURL(InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 1));
	if (!hURL)
	{
		LogManager::getInstance()->message("InternetOpenUrl error = " + Util::translateError(GetLastError()));
		return 0;
	}
	
	while (InternetReadFile(hURL, pTempData.data(), numberOfBytesToRead, &numberOfBytesRead))
	{
		if (numberOfBytesRead == 0)
		{
			// EOF detected
			break;
		}
		else
		{
			// Copy partial data
			// pTempData[numberOfBytesRead] = '\0';
			// data += pTempData.get();
			fileout.write(pTempData.data(), numberOfBytesRead);
			totalBytesRead += numberOfBytesRead;
			if (reporter)
			{
				if (!reporter->ReportResultReceive(numberOfBytesRead))
				{
					break;
				}
			}
		}
	}
	return totalBytesRead;
}

#endif
//[+] SSA
uint64_t Util::getDataFromInet(LPCWSTR agent, const DWORD frameBufferSize, const string& url, unique_ptr<byte[]>& dataOut, LONG timeOut /*=0*/, IDateReceiveReporter* reporter /* = NULL */)
{
	dcassert(!url.empty());
	// FlylinkDC++ Team TODO: http://code.google.com/p/flylinkdc/issues/detail?id=632
	// TODO: please refactoring this code: The current implementation is redundant and is not optimal:
	// now used three different storage container of the same data :( . Please use only one container.
	if (url.empty())
		return 0;
	uint64_t totalBytesRead = 0;
	DWORD numberOfBytesRead = 0;
	DWORD numberOfBytesToRead = frameBufferSize;
	vector<byte*> vecOut(numberOfBytesToRead); // The first container.
	
	AutoArray<uint8_t> pTempData(frameBufferSize + 1); // Second container.
	CInternetHandle hInternet(InternetOpen(agent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	if (!hInternet)
		return 0;
		
	if (timeOut)
		InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeOut, sizeof(timeOut));
		
	CInternetHandle hURL(InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 1));
	if (!hURL)
	{
		LogManager::getInstance()->message("InternetOpenUrl error = " + Util::translateError(GetLastError()));
		return 0;
	}
	bool isUserCancel = false;
	while (InternetReadFile(hURL, pTempData.data(), numberOfBytesToRead, &numberOfBytesRead))
	{
		if (numberOfBytesRead == 0)
		{
			// EOF detected
			break;
		}
		else
		{
			// Copy partial data
			// pTempData[numberOfBytesRead] = '\0';
			// data += pTempData.get();
			if (totalBytesRead + numberOfBytesRead > vecOut.size())
			{
				vecOut.resize(totalBytesRead + numberOfBytesRead);
			}
			memcpy(&vecOut[totalBytesRead], pTempData.data(), numberOfBytesRead);
			totalBytesRead += numberOfBytesRead;
			if (reporter)
			{
				if (!reporter->ReportResultReceive(numberOfBytesRead)) //  2012-04-29_06-52-32_ZCMA2HPLJNT2IO6XPN2TGEMMKDEC4FRWXU24CMY_56E6F820_crash-stack-r502-beta23-build-9860.dmp
				{
					isUserCancel = true;
					break;
				}
			}
		}
	}
	if (isUserCancel)
	{
		dataOut.reset();
		totalBytesRead = 0;
	}
	else
	{
		dataOut = unique_ptr<byte[]>(new byte[ totalBytesRead ]); // Third container.
		for (size_t i = 0; i < totalBytesRead; i += frameBufferSize)
		{
			size_t lenCopy = frameBufferSize;
			if (totalBytesRead - i < frameBufferSize)
				lenCopy = totalBytesRead - i;
				
			byte* ptrCopy = dataOut.get() + i;
			memcpy(ptrCopy, &vecOut[i], lenCopy);
		}
	}
	return totalBytesRead;
}

// [-] IRainman
//bool Util::IsXPSP3AndHigher()
//{
//	OSVERSIONINFOEX ver;
//	if (!getVersionInfo(ver))
//		return false;
//
//	if (ver.dwMajorVersion >= 6)
//		return true;
//	if (ver.dwMajorVersion == 5 && ver.dwMinorVersion > 1)
//		return true;
//	if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1 && ver.wServicePackMajor >= 3)
//		return true;
//
//	return false;
//}
// [-] IRainman
//bool Util::getVersionInfo(OSVERSIONINFOEX& ver)
//{
//	// version can't change during process lifetime
//	if (osvi.dwOSVersionInfoSize != 0)
//	{
//		ver = osvi;
//		return true;
//	}
//
//	memzero(&ver, sizeof(OSVERSIONINFOEX));
//	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
//
//	if (!GetVersionEx((OSVERSIONINFO*)&ver))
//	{
//		ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
//		if (!GetVersionEx((OSVERSIONINFO*)&ver))
//		{
//			return false;
//		}
//	}
//
//	if (&ver != &osvi) osvi = ver;
//	return true;
//}

bool Util::getTTH_MD5(const string& p_filename, size_t p_buffSize, unique_ptr<TigerTree>* p_tth, unique_ptr<MD5Calc>* p_md5 /* = 0 */, bool p_isAbsPath/* = true*/)
{
	dcassert(p_tth != nullptr);
	if (p_tth == nullptr)
		return false;
	AutoArray<uint8_t> buf(p_buffSize);
	try
	{
		File f(p_filename, File::READ, File::OPEN, p_isAbsPath);
		
		*p_tth = unique_ptr<TigerTree>(new TigerTree(TigerTree::calcBlockSize(f.getSize(), 1)));
		if (p_md5)
		{
			*p_md5 = unique_ptr<MD5Calc>(new MD5Calc());
			p_md5->get()->MD5Init();
			// *****************************************************
			if (f.getSize() > 0)
			{
				size_t n = p_buffSize;
				while ((n = f.read(buf.data(), n)) > 0)
				{
					p_tth->get()->update(buf.data(), n);
					// ****************
					p_md5->get()->MD5Update(buf.data(), n);
					// ****************
					n = p_buffSize;
				}
			}
			else
			{
				p_tth->get()->update("", 0);
			}
			// *****************************************************
		}
		else
		{
			// *****************************************************
			if (f.getSize() > 0)
			{
				size_t n = p_buffSize;
				while ((n = f.read(buf.data(), n)) > 0)
				{
					p_tth->get()->update(buf.data(), n);
					n = p_buffSize;
				}
			}
			else
			{
				p_tth->get()->update("", 0);
			}
			// *****************************************************
		}
		f.close();
		p_tth->get()->finalize();
		return true;
	}
	catch (const FileException&)
	{
		//-V565
		// No File
	}
	return false;
}
// [+] NightOrion
void Util::BackupSettings()
{
	static const char* FileList[] =
	{
		"ADLSearch.xml",
		"DCPlusPlus.xml",
		"Favorites.xml",
		"IPTrust.ini",
#ifdef SSA_IPGRANT_FEATURE
		"IPGrant.ini",
#endif
#ifdef PPA_INCLUDE_IPGUARD
		"IPGuard.ini",
#endif
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		"Profiles.xml"
#endif
	};
	
	const string bkpath = formatTime(getConfigPath() + "BackUp\\%Y-%m-%d\\", time(NULL));
	const string& sourcepath = getConfigPath();
	File::ensureDirectory(bkpath);
	for (size_t i = 0; i < _countof(FileList); ++i)
	{
		if (!File::isExist(bkpath + *FileList[i]) && File::isExist(sourcepath + FileList[i]))
		{
			try
			{
				File::copyFile(sourcepath + FileList[i], bkpath + FileList[i]); // Exception 2012-05-03_22-05-14_LZE57W5HZ7NI3VC773UG4DNJ4QIKP7Q7AEBLWOA_AA236F48_crash-stack-r502-beta24-x64-build-9900.dmp
			}
			catch (FileException &)
			{
			}
		}
	}
}

string Util::formatDchubUrl(const string& DchubUrl)
{
#ifdef _DEBUG
	static unsigned int l_call_count = 0;
	dcdebug("Util::formatDchubUrl DchubUrl =\"%s\", call count %d\n", DchubUrl.c_str(), ++l_call_count);
#endif
	//[-] PVS-Studio V808 string path;
	uint16_t port;
	string proto, host, file, query, fragment;
	
	decodeUrl(DchubUrl, proto, host, port, file, query, fragment);
	const string l_url = proto + "://" + host + ((port == 411 && proto == "dchub") ? "" : ":" + Util::toString(port)); // [!] IRainman opt
	dcassert(l_url == Text::toLower(l_url));
	return l_url;
}
// [~] NightOrion

string Util::getMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	return "magnet:?xt=urn:tree:tiger:" + aHash.toBase32() + "&xl=" + toString(aSize) + "&dn=" + encodeURI(aFile);
}

// [+] necros
string Util::getWebMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	StringMap params;
	params["magnet"] = getMagnet(aHash, aFile, aSize);
	params["size"] = formatBytes(aSize);
	params["TTH"] = aHash.toBase32();
	params["name"] = aFile;
	return formatParams(SETTING(COPY_WMLINK), params, false);
}

string Util::getMagnetByPath(const string& aFile) // [+] SSA - returns empty string or magnet
{
	// [-] IRainman fix. try {
	string outFilename;
	TTHValue outTTH;
	int64_t outSize = 0;
	if (ShareManager::getInstance()->findByRealPathName(aFile, &outTTH, &outFilename,  &outSize))
		return getMagnet(outTTH, outFilename, outSize);
		
	// [-] IRainman fix. } catch (ShareException& /*shEx*/) {}
	return emptyString;
}
string Util::getDownloadPath(const string& def)
{
	typedef HRESULT(WINAPI * _SHGetKnownFolderPath)(GUID & rfid, DWORD dwFlags, HANDLE hToken, PWSTR * ppszPath);
	
	// Try Vista downloads path
	static HINSTANCE shell32 = nullptr;
	if (!shell32)
	{
		shell32 = ::LoadLibrary(_T("Shell32.dll"));
		if (shell32)
		{
			_SHGetKnownFolderPath getKnownFolderPath = (_SHGetKnownFolderPath)::GetProcAddress(shell32, "SHGetKnownFolderPath");
			
			if (getKnownFolderPath)
			{
				PWSTR path = nullptr;
				// Defined in KnownFolders.h.
				static GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
				if (getKnownFolderPath(downloads, 0, NULL, &path) == S_OK)
				{
					const string ret = Text::fromT(path) + "\\";
					::CoTaskMemFree(path);
					return ret;
				}
			}
			::FreeLibrary(shell32); // [+] IRainman fix.
		}
	}
	
	return def + "Downloads\\";
}

// From RSSManager.h
string Util::ConvertFromHTMLSymbol(const string &htmlString, const string& findString, const string& changeString)
{
	string strRet;
	if (htmlString.empty())
		return htmlString;
		
	string::size_type findFirst = htmlString.find(findString);
	string::size_type prevPos = 0;
	while (findFirst != string::npos)
	{
		strRet.append(htmlString.substr(prevPos, findFirst - prevPos));
		//strRet.append(changeString);
		strRet.append(Text::acpToUtf8(changeString));
		prevPos = findFirst + findString.length();
		findFirst = htmlString.find(findString, prevPos);
	}
	strRet.append(htmlString.substr(prevPos));
	return strRet;
}
string Util::ConvertFromHTML(const string &htmlString)
{
	// Replace &quot; with \"
	string strRet;
	if (htmlString.empty())
		return htmlString;
	strRet = ConvertFromHTMLSymbol(htmlString, "&nbsp;", " ");
	strRet = ConvertFromHTMLSymbol(strRet, "<br>", " \r\n");
	strRet = ConvertFromHTMLSymbol(strRet, "&quot;", "\"");
	strRet = ConvertFromHTMLSymbol(strRet, "&mdash;", "�");
	strRet = ConvertFromHTMLSymbol(strRet, "&ndash;", "�");
	strRet = ConvertFromHTMLSymbol(strRet, "&hellip;", "�");
	strRet = ConvertFromHTMLSymbol(strRet, "&laquo;", "�");
	strRet = ConvertFromHTMLSymbol(strRet, "&raquo;", "�");
//	strRet = ConvertFromHTMLSymbol(strRet, "&lt;", "<");
//	strRet = ConvertFromHTMLSymbol(strRet, "&gt;", ">");
	return strRet;
}
// From RSSManager.h

tstring Util::eraseHtmlTags(tstring && p_desc)
{
	static const std::wregex g_tagsRegex(L"<[^>]+>|<(.*)>");
	p_desc = std::regex_replace(p_desc, g_tagsRegex, (tstring) L" ");
//	p_desc = Text::toT(ConvertFromHTML( Text::fromT(p_desc) ));
	/*
	static const tstring g_htmlTags[] = // Tags: from long to short !
	{
	    _T("<br"), // always first.
	    _T("<strong"), _T("</strong"),
	    _T("<table"), _T("</table"),
	    _T("<font"), _T("</font"),
	    _T("<img"),
	    _T("<div"), _T("</div"),
	    _T("<tr"), _T("</tr"),
	    _T("<td"), _T("</td"),
	    _T("<u"), _T("</u"),
	    _T("<i"), _T("</i"),
	    _T("<b"), _T("</b")
	};
	for (size_t i = 0; i < _countof(g_htmlTags); ++i)
	{
	    const tstring& l_find = g_htmlTags[i];
	    size_t l_start = 0;
	    while (true)
	    {
	        l_start = p_desc.find(l_find, 0);
	        if (l_start == string::npos)
	            break;
	
	        const auto l_end = p_desc.find(_T('>'), l_start);
	        if (l_end == tstring::npos)
	        {
	            p_desc.erase(l_start, p_desc.size() - l_start);
	            return p_desc; // Did not find the closing tag? - Stop processing
	        }
	        if (i == 0)  // space instead of <br>
	        {
	            p_desc.replace(l_start, l_end - l_start + 1, _T(" "));
	            ++l_start;
	        }
	        else
	        {
	            p_desc.erase(l_start, l_end - l_start + 1);
	        }
	    }
	}
	*/
	return p_desc;
}

void Util::playSound(const string& p_sound, const bool p_beep /* = false */)
{
//#ifdef _DEBUG
//	LogManager::getInstance()->message(p_sound + (p_beep ? string(" p_beep = true") : string(" p_beep = false")));
//#endif
	if (!p_sound.empty())
	{
		PlaySound(Text::toT(p_sound).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	}
	else if (p_beep)
	{
		MessageBeep(MB_OK);
	}
}
// [+] IRainman: settings split and parse.
StringList Util::splitSettingAndReplaceSpace(string patternList)
{
	boost::replace_all(patternList, " ", "");
	return splitSetting(patternList);
}

string Util::toSettingString(const StringList& patternList)
{
	string ret;
	for (auto i = patternList.cbegin(), iend = patternList.cend(); i != iend; ++i)
	{
		ret += *i + ';';
	}
	if (!ret.empty())
	{
		ret.resize(ret.size() - 1);
	}
	return ret;
}
// [~] IRainman: settings split and parse.
string Util::getLang()
{
	const string& l_lang = SETTING(LANGUAGE_FILE);
	dcassert(l_lang.length() == 9);
	return l_lang.substr(0, 2);
}

string Util::getWikiLink()
{
	return HELPPAGE + getLang() + ':';
}

#ifdef _WIN32
DWORD Util::GetTextResource(const int p_res, LPCSTR& p_data)
{
	HRSRC hResInfo = FindResource(nullptr, MAKEINTRESOURCE(p_res), RT_RCDATA);
	if (hResInfo)
	{
		HGLOBAL hResGlobal = LoadResource(nullptr, hResInfo);
		if (hResGlobal)
		{
			p_data = (LPCSTR)LockResource(hResGlobal);
			if (p_data)
			{
				return SizeofResource(nullptr, hResInfo);
			}
		}
	}
	dcassert(0);
	return 0;
}

void Util::WriteTextResourceToFile(const int p_res, const tstring& p_file)
{
	LPCSTR l_data;
	if (const DWORD l_size = GetTextResource(p_res, l_data))
	{
		std::ofstream l_file_out(p_file.c_str());
		l_file_out.write(l_data, l_size);
		return;
	}
	dcassert(0);
}
#endif // _WIN32

/**
 * @file
 * $Id: Util.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */