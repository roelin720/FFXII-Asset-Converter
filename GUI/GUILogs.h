#pragma once
#include <list>
#include <string>
#include "imgui.h"

enum GUILogType
{
	GUILog_KeyInfo,
	GUILog_ConvInfo,
	GUILog_Info,
	GUILog_ConvKeyInfo,
	GUILog_Warn,
	GUILog_ConvWarn,
	GUILog_Error,
	GUILog_ConvError,
	GUILog_ConvSuccess,
	GUILog_COUNT
};

constexpr ImVec4 LogColours[] =
{
	ImVec4(0.7f, 0.7f, 1.0f, 1.0f),
	ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
	ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
	ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
	ImVec4(1.0f, 1.0f, 0.7f, 1.0f),
	ImVec4(1.0f, 0.9f, 0.6f, 1.0f),
	ImVec4(1.0f, 0.7f, 0.7f, 1.0f),
	ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
	ImVec4(0.6f, 1.0f, 0.6f, 1.0f),
};

struct GUILog : public std::string
{
	GUILogType type;

	std::string datetime();

	GUILog(GUILogType type, const std::string& msg);
};

struct GUILogs
{
	static constexpr uint32_t log_buffer_max = 2048;
	static std::list<GUILog> data;
	static bool scroll_log;

	static void push(const GUILog& log);
};