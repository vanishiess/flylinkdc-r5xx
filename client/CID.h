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

#ifndef DCPLUSPLUS_DCPP_CID_H
#define DCPLUSPLUS_DCPP_CID_H

#include "Util.h"
#ifdef IRAINMAN_USE_BOOST_HASHER_FOR_CID
#include <boost/functional/hash.hpp>
#endif

class CID
{
	public:
		enum { SIZE = 192 / 8 };
		
		CID()
		{
			memzero(cid, sizeof(cid));
		}
		explicit CID(const uint8_t* data)
		{
			memcpy(cid, data, sizeof(cid));
		}
		explicit CID(const string& base32)
		{
			Encoder::fromBase32(base32.c_str(), cid, sizeof(cid));
		}
		
		bool operator==(const CID& rhs) const
		{
			return memcmp(cid, rhs.cid, sizeof(cid)) == 0;
		}
		bool operator<(const CID& rhs) const
		{
			return memcmp(cid, rhs.cid, sizeof(cid)) < 0;
		}
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
		operator const CID*() const // [+] IRainman fix.
		{
			return this;
		}
#endif
		
		string toBase32() const
		{
			return Encoder::toBase32(cid, sizeof(cid));
		}
		string& toBase32(string& tmp) const
		{
			return Encoder::toBase32(cid, sizeof(cid), tmp);
		}
		
		size_t toHash() const
		{
#ifdef IRAINMAN_USE_BOOST_HASHER_FOR_CID
			return boost::hash<uint8_t[SIZE]>()(cid);
#else
			// RVO should handle this as efficiently as reinterpret_cast version
			size_t cidHash = 0; // [+] IRainman fix: V512 http://www.viva64.com/en/V512 A call of the 'memcpy' function will lead to underflow of the buffer 'cid'.
			memcpy(&cidHash, cid, sizeof(size_t)); //-V512
			return cidHash;
#endif // IRAINMAN_USE_BOOST_HASHER_FOR_CID
		}
		const uint8_t* data() const
		{
			return cid;
		}
		uint8_t* get_data_for_write() //[+]PPA
		{
			return cid;
		}
		
		bool isZero() const
		{
			return std::find_if(cid, cid + SIZE, [](uint8_t c)
			{
				return c != 0;
			}) == (cid + SIZE);
		}
		
		static CID generate();
		
	private:
		uint8_t cid[SIZE];
};

namespace std
{
template<>
struct hash<CID>
{
	size_t operator()(const CID& rhs) const
	{
		return rhs.toHash(); // [!] IRainman fix.
	}
};
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
template<>
struct hash<const CID*> // [!] IRainman fix.
{
	size_t operator()(const CID* rhs) const
	{
		return rhs->toHash(); // [!] IRainman fix.
	}
};

template<>
struct equal_to<const CID*> // [!] IRainman fix.
{
	bool operator()(const CID* lhs, const CID* rhs) const
	{
		return (*lhs) == (*rhs);
	}
};
#endif // IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
}

#endif // !defined(CID_H)

/**
* @file
* $Id: CID.h 568 2011-07-24 18:28:43Z bigmuscle $
*/