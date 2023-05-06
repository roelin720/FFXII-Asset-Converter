#pragma once
#include "PhyreDAEUnpack.h"

namespace Phyre
{
	namespace DAE 
	{
		bool Pack(const std::string& original_path, const std::string& replacement_path, const std::string& output_path);
		bool Export(aiScene* orig_scene, const aiScene* rep_scene, const std::string& original_path, const std::string& output_path);
	}
}
