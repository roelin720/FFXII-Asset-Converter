#pragma once
#include <list>
#include <string>

namespace Phyre
{
	enum LogType
	{
		Log_Info = 1,
		Log_ConvInfo = 1 | 2,
		Log_StartInfo = 1 | 4,
		Log_Error = 8,
		Log_ConvError = 8 | 16
	};

	struct Log : public std::string
	{
		LogType type;

		std::string datetime();

		Log(LogType type, const std::string& msg);
	};

	struct Logs
	{
		static constexpr uint32_t log_buffer_max = 2048;
		static std::list<Log> data;
		static bool scroll_log;

		static void push(const Log& log);
	};

}