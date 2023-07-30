#include "GUILogs.h"
#include "ConverterInterface.h"
#include <sstream>
#include <iomanip>
#include <ctime>

std::list<::Log> Logs::data;
bool Logs::scroll_log = false;

std::string Log::datetime()
{
	std::stringstream sstream;
	tm _tm = {};
	time_t time = std::time(nullptr);
	localtime_s(&_tm, &time);
	sstream << std::put_time(&_tm, "%d-%m-%Y %H-%M-%S");
	return sstream.str();
}

Log::Log(LogType type, const std::string& msg) : type(type)
{
	std::stringstream ss(msg);
	std::string s;

	if (!msg.empty())
	{
		for (std::string line; std::getline(ss, line, '\n');)
		{
			s += (s.empty() ? datetime() + ": " : "                   : ") + line + "\n";
		}
	}
	assign(s);
}

void Logs::push(const Log& log)
{
	data.push_back(log);
	if (data.size() >= log_buffer_max)
	{
		data.pop_front();
	}
	scroll_log = true;
}

