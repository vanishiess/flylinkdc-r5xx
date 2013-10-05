#if !defined __DEBUGMANAGER_H
#define __DEBUGMANAGER_H

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "TimerManager.h"
#include "Util.h"

struct DebugTask // [+] IRainman.
{
	enum Type
	{
		HUB_IN,
		HUB_OUT,
		CLIENT_IN,
		CLIENT_OUT,
		DETECTION,
#ifdef _DEBUG
		LAST
#endif
	};
	DebugTask()
#ifdef _DEBUG
		: m_type(LAST)
#endif
	{
	}
	DebugTask(const string& message, Type type, const string& p_ip_and_port = Util::emptyString) :
		m_message(message), m_ip_and_port(p_ip_and_port), m_time(TimerManager::getTime()), m_type(type)
	{
	}
	static string format(const DebugTask& task)
	{
		string out = Util::getShortTimeString(task.m_time) + ' '; // [!] IRainman fix: use real time.
		switch (task.m_type)
		{
			case DebugTask::HUB_IN:
				out += "Hub:\t[Incoming][" + task.m_ip_and_port + "]\t \t";
				break;
			case DebugTask::HUB_OUT:
				out += "Hub:\t[Outgoing][" + task.m_ip_and_port + "]\t \t";
				break;
			case DebugTask::CLIENT_IN:
				out += "Client:\t[Incoming][" + task.m_ip_and_port + "]\t \t";
				break;
			case DebugTask::CLIENT_OUT:
				out += "Client:\t[Outgoing][" + task.m_ip_and_port + "]\t \t";
				break;
#ifdef _DEBUG
			case DebugTask::DETECTION:
				break;
			default:
				dcassert(0);
#endif
		}
		out += !Text::validateUtf8(task.m_message) ? Text::toUtf8(task.m_message) : task.m_message; // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=617
		out += "\r\n";
		return out;
	}
	string m_message;
	string m_ip_and_port;
	time_t m_time;
	Type m_type;
};

class DebugManagerListener
{
	public:
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> DebugEvent;
		
		virtual void on(DebugEvent, const DebugTask&) noexcept { } // [!] IRainman fix.
};

class DebugManager : public Singleton<DebugManager>, public Speaker<DebugManagerListener>
{
		friend class Singleton<DebugManager>;
		DebugManager() { }
		~DebugManager() { }
	public:
		void SendCommandMessage(const string& command, DebugTask::Type type, const string& adress) noexcept
		{
			fire(DebugManagerListener::DebugEvent(), DebugTask(command, type, adress)); // [!] IRainman: use real time.
		}
		void SendDetectionMessage(const string& mess) noexcept
		{
			fire(DebugManagerListener::DebugEvent(), DebugTask(mess, DebugTask::DETECTION)); // [!] IRainman: use real time.
		}
};
#define COMMAND_DEBUG(command, direction, ip) if (DebugManager::isValidInstance()) { DebugManager::getInstance()->SendCommandMessage(command, direction, ip); }
#define DETECTION_DEBUG(message) if (DebugManager::isValidInstance()) { DebugManager::getInstance()->SendDetectionMessage(message); }
#else
#define COMMAND_DEBUG(message, direction, ip)
#define DETECTION_DEBUG(message)
#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
#endif