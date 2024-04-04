#include "GUIPathHistory.h"
#include "ConverterInterface.h"
#include "FileIOUtils.h"
#include <filesystem>

namespace
{
	inline constexpr int64_t mod(int64_t a, int64_t b)
	{
		return (a % b + b) % b;
	}
}

PathHistory::PathHistory()
{
	std::fill_n(scroll_pos, _PathID_COUNT, -1);

	for (int i = 0; i < _PathID_COUNT; ++i)
	{
		current[i] = new char[max_path_size];
		std::fill_n(current[i], max_path_size, 0);
	}
}

PathHistory::~PathHistory()
{
	for (int i = 0; i < _PathID_COUNT; ++i)
	{
		delete[] current[i];
	}
}

void PathHistory::save()
{
	using namespace IO;
	try
	{
		std::ofstream stream("input_history.bin", std::ios::binary);
		for (int i = 0; i < data.size(); ++i)
		{
			write<int32_t>(stream, data[i].size());
			for (int j = 0; j < data[i].size(); ++j)
			{
				write<int32_t>(stream, data[i][j].size());
				write(stream, data[i][j].data(), data[i][j].size());
			}
		}
	}
	catch (std::exception& e)
	{
		LOG(ERR) << "Failed to save input history " << std::string(e.what());
	}
}

int PathHistory::find(const std::string& path, PathID pathID)
{
	for (int i = 0; i < data[pathID].size(); ++i)
	{
		if (path == data[pathID][i])
		{
			return i;
		}
	}
	return -1;
}

void PathHistory::push(const std::string& path, PathID pathID)
{
	int prev_index = find(path, pathID);
	if (prev_index >= 0)
	{
		auto it = data[pathID].begin() + prev_index;
		std::rotate(it, it + 1, data[pathID].end());
	}
	else
	{
		data[pathID].push_back(path);
		if (data[pathID].size() >= history_max)
		{
			data[pathID].erase(data[pathID].begin());
		}
	}
	scroll_pos[pathID] = data.size() - 1;
}

std::string PathHistory::last(PathID pathID)
{

	for (size_t i = 0; i < data.size(); ++i)
	{
		size_t cur_path_type = mod(int64_t(pathID) - int64_t(i), int64_t(data.size()));
		if (!data[cur_path_type].empty())
		{
			return data[cur_path_type].back();
		}
	}
	return "";
}

void PathHistory::load()
{
	using namespace IO;
	try
	{
		if (!std::filesystem::exists("input_history.bin"))
		{
			return;
		}
		std::ifstream stream("input_history.bin", std::ios::binary);
		for (int i = 0; i < data.size(); ++i)
		{
			data[i].resize(read<int32_t>(stream));

			for (int j = 0; j < data[i].size(); ++j)
			{
				data[i][j].resize(read<int32_t>(stream), '_');
				read(stream, data[i][j].data(), data[i][j].size());
			}
			if (data[i].empty() == false)
			{
				strcpy_s(current[i], max_path_size, data[i].back().data());
			}
		}
	}
	catch (std::exception& e)
	{
		LOG(ERR) << "Failed to load input data " << std::string(e.what()) << std::endl;
	}
}
int PathHistory::scroll_callback(ImGuiInputTextCallbackData* _data, PathID pathID)
{
	if (data[pathID].size())
	{
		if (_data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
		{
			if (_data->EventKey == ImGuiKey_UpArrow)
			{
				scroll_pos[pathID] = mod(int64_t(scroll_pos[pathID]) + 1, data[pathID].size());
				_data->DeleteChars(0, _data->BufTextLen);
				_data->InsertChars(0, data[pathID][scroll_pos[pathID]].c_str());
				_data->SelectAll();
			}
			else if (_data->EventKey == ImGuiKey_DownArrow)
			{
				scroll_pos[pathID] = mod(int64_t(scroll_pos[pathID]) - 1, data[pathID].size());
				_data->DeleteChars(0, _data->BufTextLen);
				_data->InsertChars(0, data[pathID][scroll_pos[pathID]].c_str());
				_data->SelectAll();
			}
		}
	}
	return 0;
}
