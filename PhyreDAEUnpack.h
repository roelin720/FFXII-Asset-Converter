#pragma once
#include <string>
#include <assimp/scene.h>

namespace Phyre
{
    namespace DAE 
    {
        bool Unpack(const std::string& orig_file, const std::string& out_file);
        void Import(const std::string& pFile, aiScene* scene);

        void FlipNormals(aiScene* scene);
    }
}
