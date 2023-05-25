#include "PhyreMetadata.h"
#include "PhyreIOUtils.h"
#include "assimp/../../contrib/rapidjson/include/rapidjson/istreamwrapper.h"
#include <fstream>
#include <filesystem>

Phyre::Metadata::Metadata(const std::string& asset_dir)
{
	std::filesystem::path metadata_path = "";
	try
	{
		metadata_path = std::filesystem::path(asset_dir).parent_path().string() + PhyreIO::FileName(asset_dir) + "_metadata.json";
		if (!std::filesystem::exists(metadata_path))
		{
			metadata_path = "";
		}
	}
	catch (const std::exception& e)
	{
		return;
	}
	if (metadata_path.empty())
	{
		return;
	}

	try
	{
		std::ifstream ifs(metadata_path);
		rapidjson::IStreamWrapper isw(ifs);
		doc.ParseStream(isw);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load metadata - " << metadata_path << std::endl;
		return;
	}
}

bool Phyre::Metadata::enabled()
{
	try
	{
		return doc["enabled"].GetBool();
	}
	catch (const std::exception& e) 
	{
		return true;
	}
}

bool Phyre::Metadata::include_mesh(int mesh_index)
{
	try
	{
		rapidjson::Value& include = doc["include"];
		int item_count = doc["include"].Size();
		for (int i = 0; i < item_count; ++i)
		{
			if (include[i].GetString() == "Mesh" + std::to_string(mesh_index)) 
			{
				return true;
			}
		}
		return false;
	}
	catch (const std::exception& e)
	{
		return true;
	}
}

bool Phyre::Metadata::exclude_mesh(int mesh_index)
{
	try
	{
		rapidjson::Value& include = doc["exclude"];
		int item_count = doc["exclude"].Size();
		for (int i = 0; i < item_count; ++i)
		{
			if (include[i].GetString() == "Mesh" + std::to_string(mesh_index))
			{
				return true;
			}
		}
		return false;
	}
	catch (const std::exception& e)
	{
		return false;
	}
}
