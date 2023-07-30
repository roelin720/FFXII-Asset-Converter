#pragma once
#include <list>
#include <string>
#include "imgui.h"

enum LogType
{
	Log_Info,
	Log_ConvInfo,
	Log_StartInfo,
	Log_Warn,
	Log_ConvWarn,
	Log_Error,
	Log_ConvError,
	Log_COUNT
};

constexpr ImVec4 LogColours[] =
{
	ImVec4(0.8f, 0.8f, 1.0f, 1.0f),
	ImVec4(0.6f, 1.0f, 0.6f, 1.0f),
	ImVec4(0.6f, 0.6f, 1.0f, 1.0f),
	ImVec4(1.0f, 1.0f, 0.7f, 1.0f),
	ImVec4(1.0f, 0.9f, 0.6f, 1.0f),
	ImVec4(1.0f, 0.7f, 0.7f, 1.0f),
	ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
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