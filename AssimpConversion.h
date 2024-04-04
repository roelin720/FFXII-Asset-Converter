#pragma once
#include "Scene.h"
#include <assimp/scene.h>

namespace AssimpConversion
{
	bool ConvertToAssimpScene(aiScene& aScene, const Scene& scene);
	bool ConvertFromAssimpScene(const aiScene& aScene, Scene& scene);

    void ClearUnusedBones(aiScene& aScene);

    //assimp math conversion functions:

    template<typename DstT, typename SrcT>
    inline DstT Convert(const SrcT& src) { DstT dst = src; return dst; }

    static_assert(sizeof(ai_real) == 4,         "sizeof(ai_real) != 4");
    static_assert(sizeof(aiMatrix4x4) == 64,    "sizeof(aiMatrix4x4) != 64");
    static_assert(sizeof(aiVector3D) == 12,     "sizeof(aiVector3D) != 12");
    static_assert(sizeof(aiVector2D) == 8,      "sizeof(aiVector2D) != 8");
    static_assert(sizeof(aiColor4D) == 16,      "sizeof(aiColor4D) != 16");

    template<> inline aiVector2D Convert<aiVector2D>(const glm::vec2& src) { return { src[0], src[1] }; };
    template<> inline aiVector3D Convert<aiVector3D>(const glm::vec3& src) { return { src[0], src[1], src[2] }; };
    template<> inline aiColor4D Convert<aiColor4D>(const glm::vec4& src) { return { src[0], src[1], src[2], src[3]}; };
    template<> inline aiMatrix4x4 Convert<aiMatrix4x4>(const glm::mat4& src) { return (aiMatrix4x4&)glm::transpose(src); };

    template<> inline glm::vec2 Convert<glm::vec2>(const aiVector2D& src) { return { src[0], src[1] }; };
    template<> inline glm::vec3 Convert<glm::vec3>(const aiVector3D& src) { return { src[0], src[1], src[2] }; };
    template<> inline glm::vec4 Convert<glm::vec4>(const aiColor4D& src) { return { src[0], src[1], src[2], src[3] }; };
    template<> inline glm::mat4 Convert<glm::mat4>(const aiMatrix4x4& src) { return glm::transpose((glm::mat4&)src); };
}

//assimp math printing functions:

#define fmtf(f) std::fixed << std::setprecision(4) << std::showpoint << f

inline std::ostream& operator<<(std::ostream& out, const aiVector2D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]);
}

inline std::ostream& operator<<(std::ostream& out, const aiVector3D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]);
}

inline std::ostream& operator<<(std::ostream& out, const aiColor4D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]) << "\t" << fmtf(val[3]);
}

inline std::ostream& operator<<(std::ostream& out, const aiMatrix4x4& val)
{
    for (uint32_t x = 0; x < 4; ++x)
    {
        for (uint32_t y = 0; y < 4; ++y)
        {
            out << fmtf(val[x][y]) << "\t";
        }
        out << std::endl;
    }
    return out << std::endl;
}

inline std::ostream& operator<<(std::ostream& out, const aiString& val)
{
    return out << val.C_Str();
}

#undef fmtf