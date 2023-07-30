#pragma once
#include "Enums.h"
#include "FileObjectBlock.h"
#include <string>
#include <iomanip>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <vector>
#include <array>
#include <map>

#define MAGIC_32(c1, c2, c3, c4) uint32_t(c1) | (uint32_t(c2) << 8) | (uint32_t(c3) << 16) | (uint32_t(c3) << 24)
#define PHYRE_MAGIC_BYTES MAGIC_32('P', 'H', 'Y', 'R')

struct PhyreHeader
{
    /* 0  */ uint32_t magic_bytes;				
    /* 4  */ uint32_t header_size;					
    /* 8  */ uint32_t namespace_size;		
    /* 12 */ uint32_t platform_ID;				    
    /* 16 */ uint32_t object_block_count;
    /* 20 */ uint32_t array_link_size;			
    /* 24 */ uint32_t array_link_count;			
    /* 28 */ uint32_t object_link_size;		
    /* 32 */ uint32_t object_link_count;		
    /* 36 */ uint32_t object_array_link_size;	
    /* 40 */ uint32_t object_array_link_count;	
    /* 44 */ uint32_t objects_in_arrays_count;	
    /* 48 */ uint32_t shared_data_count;			
    /* 52 */ uint32_t shared_data_size;		
    /* 56 */ uint32_t block_data_size;			      
    /* 60 */ uint32_t header_class_object_block_count;
    /* 64 */ uint32_t header_class_child_count;
    /* 68 */ uint32_t physics_engine_ID;
    /* 72 */ uint32_t indices_size;
    /* 76 */ uint32_t vertices_size;
    /* 80 */ uint32_t max_texture_size;
};

struct PrimType
{
    PrimID ID;
    uint32_t count;

    PrimType() : ID(PrimID::INVALID), count(0) {};
    PrimType(uint32_t prim_ID_with_elem_count) :
        ID(PrimElementType(prim_ID_with_elem_count)),
        count(PrimElementCount(prim_ID_with_elem_count))
    {}
};

struct SharedData 
{
    PrimType type;
    std::vector<uint8_t> data;
};

struct fBoneWeight
{
    uint8_t boneID = 0;
    float   weight = 0.0f;
};

struct fSubMesh : public aiMesh
{
    struct VertexComponentMetadata
    {
        VertexComponentType ID;
        PrimType data_type = PrimType();

        int64_t offset = 0;
        int64_t stride = 0;
        int64_t data_count = 0;
        int64_t data_offset = 0;
    };

    struct SubMeshMetadata
    {
        int64_t offset = 0;
        int64_t face_offset = 0;
        int64_t component_count = 0;
        int64_t bone_remap_offset = 0;
    };

    SubMeshMetadata meta;
    std::map<VertexComponentType, std::vector<VertexComponentMetadata>> comp_metas;
};

struct fMeshMetadata : public aiMesh
{
    std::vector<std::string> bones;
    std::vector<std::string> mesh_node_names;
};

struct fNode : public aiNode
{
    using aiNode::aiNode;

    struct NodeMetadata
    {
        int64_t meshID = -1;
        int64_t mesh_skinningID = -1;
        int64_t submesh_count = 0;
        int64_t total_bone_count = 0;
        int64_t total_pose_matrix_count = 0;
        int64_t local_matrix_offset = 0;
        int64_t global_matrix_offset = -1;
        int64_t bounds_offset = -1;
        int64_t skel_bounds_offset = -1;

        std::vector<std::string> bone_names;
    };

    NodeMetadata meta;
};

struct fSceneMetadata : public aiMetadata
{
    PhyreHeader header = {};
    std::vector<ObjectBlock> object_blocks;

    ObjectBlock* ob_mesh = nullptr;
    ObjectBlock* ob_mesh_instance = nullptr;
    ObjectBlock* ob_mesh_segment = nullptr;
    ObjectBlock* ob_string = nullptr;
    ObjectBlock* ob_node = nullptr;
    ObjectBlock* ob_world_matrix = nullptr;
    ObjectBlock* ob_matrix4 = nullptr;
    ObjectBlock* ob_bone_remap = nullptr;
    ObjectBlock* ob_asset_reference_import = nullptr;
    ObjectBlock* ob_data_block = nullptr;
    ObjectBlock* ob_vertex_stream = nullptr;
    ObjectBlock* ob_mesh_instance_bounds = nullptr;
    ObjectBlock* ob_mesh_skel_bounds = nullptr;

    std::vector<std::string> submesh_node_names;

    std::vector<fMeshMetadata> mesh_meta;
    std::vector<std::string> mesh_instance_node_names;

    std::vector<::SharedData> shared_data;

    int64_t total_pose_matrix_count = 0;
    int64_t total_bone_count = 0;
};

struct fUnpackingContext : public fSceneMetadata
{
    std::map<std::string, ::fNode*> node_map;
    std::vector<::fNode*> mesh_instance_nodes;
    std::map<uint32_t, ::fNode*> wmat4_node_map;

    std::vector<::fNode*> submesh_nodes;
};


inline std::ostream& operator<< (std::ostream& out, const ObjectBlock& val)
{
    out << "name: " << val.name << std::endl;
    out << "elem_count: " << val.elem_count << std::endl;
    out << "data_offset: " << val.data_offset << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const ::fSubMesh::VertexComponentMetadata& val)
{
    out << "offset: " << val.offset << std::endl;
    out << "stride: " << val.stride << std::endl;
    out << "data_count: " << val.data_count << std::endl;
    out << "data_offset: " << val.data_offset << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const ::fSubMesh::SubMeshMetadata& val)
{
    out << "offset: " << val.offset << std::endl;
    out << "face_offset: " << val.face_offset << std::endl;
    out << "component_count: " << val.component_count << std::endl;
    return out;
}

inline std::ostream& operator<< (std::ostream& out, const ::fBoneWeight& val)
{
    return out << (int)val.boneID << " " << val.weight << std::endl;
}

inline std::ostream& operator<< (std::ostream& out, const aiVertexWeight& val)
{
    return out << val.mVertexId << " " << val.mWeight << std::endl;
}