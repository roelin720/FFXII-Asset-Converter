#pragma once
#include <string>

namespace Phyre
{
    namespace DDS
    {
        bool Pack(const std::string& original_path, const std::string& replacement_path, const std::string& output_path);
    }
}
