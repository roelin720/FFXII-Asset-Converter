#pragma once
#include <array>
#include <vector>
#include <string>
#include "imgui.h"

namespace Phyre
{
	enum PathID
	{
		PathID_INVALID = -1,
		PathID_ORIG,
		PathID_REP,
		PathID_MOD,
		_PathID_COUNT
	};

	struct PathHistory
	{
		static constexpr uint32_t history_max = 128;
		static constexpr int max_path_size = 2048;

		std::array<std::vector<std::string>, _PathID_COUNT> data;
		char* current[_PathID_COUNT] = {};
		int64_t scroll_pos[_PathID_COUNT];

		int find(const std::string& path, PathID pathID);
		void push(const std::string& path, PathID pathID);
		std::string last(PathID pathID);
		void load();
		void save();

		int scroll_callback(ImGuiInputTextCallbackData* data, PathID pathID);

		PathHistory();
		~PathHistory();
	};
}