#pragma once
#include <string>
#include <iomanip>
#include <assimp/mesh.h>
#include <array>

#define fmtf(f) std::fixed << std::setprecision(4) << std::showpoint << f

//Creates an enum class used to index into a and string array of chunk names
#define DEFINE_REQUIRED_CHUNKS(file_type, ...) enum class Requred##file_type##Chunks { __VA_ARGS__ };\
std::vector<std::string> Requred##file_type##ChunkNames = ([](){{\
    constexpr char names[] = #__VA_ARGS__;\
    std::vector<std::string> list;\
    std::string name;\
    list.reserve(8);\
    name.reserve(sizeof(names));\
    for (int i = 0; i < sizeof(names) - 1; ++i)\
    {\
        if      (names[i] == ',') { list.push_back(name); name.clear(); }\
        else if (names[i] != ' ') { name += names[i]; }\
    }\
    list.push_back(name);\
    return list;\
}})();

namespace Phyre
{
    struct Chunk
    {
        int64_t offset = 0;
        std::string name;
        int64_t elem_count = 0;
        int64_t data_offset = 0;
    };

    struct Mesh : public aiMesh
    {
        struct VertexComponentMetadata
        {
            int64_t offset = 0;
            int64_t stride = 0;
            int64_t data_count = 0;
            int64_t data_offset = 0;
        };
        struct MeshMetadata
        {
            int64_t offset = 0;
            int64_t face_offset = 0;
            int64_t component_count = 0;
        };

        MeshMetadata            meta;
        VertexComponentMetadata pos_meta;
        VertexComponentMetadata norm_meta;
        VertexComponentMetadata uv_meta;
        VertexComponentMetadata tang_meta;
        VertexComponentMetadata bitang_meta;
        VertexComponentMetadata color_meta;
        VertexComponentMetadata weight_meta;
        VertexComponentMetadata boneID_meta;

        VertexComponentMetadata& component(size_t index);
        const VertexComponentMetadata& component(size_t index) const;

        Mesh();

    private:
        VertexComponentMetadata* _components[8];
    };

    struct BoneWeight
    {
        uint8_t boneID = 0;
        float   weight = 0.0f;
    };

    using byte = int8_t;
}

namespace Assimp 
{
    using aiVector4D = std::array<ai_real, 4>;
    using aiVector4B = std::array<uint8_t, 4>;

    static_assert(sizeof(ai_real)       == 4,   "sizeof(ai_real) != 4");
    static_assert(sizeof(aiMatrix4x4)   == 64,  "sizeof(aiMatrix4x4) != 64");
    static_assert(sizeof(aiVector3D)    == 12,  "sizeof(aiVector3D) != 12");
    static_assert(sizeof(aiVector2D)    == 8,   "sizeof(aiVector2D) != 8");
    static_assert(sizeof(aiColor4D)     == 16,  "sizeof(aiColor4D) != 16");
    static_assert(sizeof(aiVector4D)    == 16,  "sizeof(aiVector4D) != 16");
    static_assert(sizeof(aiVector4B)    == 4,   "sizeof(aiVector4B) != 4");
}

inline std::ostream& operator<<(std::ostream& out, const aiVector2D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]);
}

inline std::ostream& operator<<(std::ostream& out, const aiVector3D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]);
}

inline std::ostream& operator<<(std::ostream& out, const Assimp::aiVector4D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]) << "\t" << fmtf(val[3]);
}

inline std::ostream& operator<<(std::ostream& out, const aiColor4D& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]) << "\t" << fmtf(val[3]);
}

inline std::ostream& operator<<(std::ostream& out, const Assimp::aiVector4B& val)
{
    return out << (int)val[0] << "\t" << (int)val[1] << "\t" << (int)val[2] << "\t" << (int)val[3];
}

inline std::ostream& operator<<(std::ostream& out, const aiMatrix4x4& val)
{
    for (int x = 0; x < 4; x++)
    {
        for (int y = 0; y < 4; y++)
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

inline std::ostream& operator<< (std::ostream& out, const Phyre::Chunk& val)
{
    out << "name: " << val.name << std::endl;
    out << "offset: " << val.offset << std::endl;
    out << "elem_count: " << val.elem_count << std::endl;
    out << "data_offset: " << val.data_offset << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const Phyre::Mesh::VertexComponentMetadata& val)
{
    out << "offset: " << val.offset << std::endl;
    out << "stride: " << val.stride << std::endl;
    out << "data_count: " << val.data_count << std::endl;
    out << "data_offset: " << val.data_offset << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const Phyre::Mesh::MeshMetadata& val)
{
    out << "offset: " << val.offset << std::endl;
    out << "face_offset: " << val.face_offset << std::endl;
    out << "component_count: " << val.component_count << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const Phyre::BoneWeight& val)
{
    return out << (int)val.boneID << " " << val.weight << std::endl;
}

inline std::ostream& operator<< (std::ostream& out, const aiVertexWeight& val)
{
    return out << val.mVertexId << " " << val.mWeight << std::endl;
}

#undef fmtf