#pragma once
#include <string>
#include <stdexcept>

class rapidjson_exception : public std::runtime_error
{
public:
	rapidjson_exception() : std::runtime_error("json schema invalid") {}
};
#define RAPIDJSON_ASSERT(x)  if(x); else throw rapidjson_exception();

#include "assimp/../../contrib/rapidjson/include/rapidjson/document.h"

namespace Phyre
{
	struct Metadata
	{
	private:
		rapidjson::Document doc;

	public:
		Metadata(const std::string& asset_dir);

		bool enabled();
		bool include_mesh(int mesh_index);
		bool exclude_mesh(int mesh_index);
	};
}