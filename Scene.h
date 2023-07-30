#pragma once
#include "FileStructs.h"
#include "FileObjectBlock.h"
#include "FileIOUtils.h"
#include "glm/glm.hpp"
#include <set>
#include <map>

struct VertexComponent
{
    VertexComponentType comp_type;
    uint32_t ID = 0;
    PrimType prim_type = PrimType();

    int64_t elem_size = 0;
    int64_t elem_count = 0;
    std::vector<char> data;
};

struct Material
{
    std::string texture_ref;
    uint32_t ID = 0;

    std::string diffuse_texture_path;
    std::string normal_texture_path;
    std::string specular_texture_path;
};

struct BBox
{
    glm::vec3 min;
    glm::vec3 max;
    uint32_t ID = 0;
};

struct Model;
struct ModelBBox : public BBox
{
    Model* model = nullptr;
};

struct Bone;
struct BoneBBox : public BBox
{
    Bone* bone = nullptr;
};

struct Node;
struct GlobalTransform
{
    glm::mat4 transform;
    uint32_t ID = 0;
    Node* node = nullptr;
};

struct Node
{
    std::string name;
    uint32_t ID = 0;

    Node* parent = nullptr;
    std::vector<Node*> children;

    glm::mat4 transform;
    GlobalTransform* global_transform = nullptr;
};

struct Bone
{
    Node* node = nullptr;
    Bone* parent = nullptr;
    BoneBBox* bounds;
    glm::mat4 transform;
    glm::mat4 pose;
    uint32_t ID;
};

struct Mesh;
struct SubMesh
{
    std::string name;
    uint32_t ID = 0;

    Mesh* mesh = nullptr;
    Material* material = nullptr;
    std::vector<Bone*> bones; //recover lost information

    std::map<VertexComponentType, std::vector<VertexComponent*>> components;
    std::vector<uint16_t> indices;

    int64_t file_material_ID = 0;

    void clear();
};

struct Mesh
{
    std::string name;
    uint32_t ID = 0;

    std::vector<Model*> models;
    //bone indices index into skin_bone_remap
    //first = index into hierarchy matrices, fixed
    //second = index into pose array, flexible
    std::map<uint16_t, uint16_t> global_to_local_bone_ID;
    std::vector<Bone> skeleton;

    std::vector<SubMesh*> submeshes;
};

struct Model
{
    std::string name;
    uint32_t ID = 0;

    Node* node = nullptr;
    Mesh* mesh = nullptr;
    ModelBBox* bounds = nullptr;
};

struct Scene
{
    struct FileObjectBlocks : public std::vector<ObjectBlock>
    {
        ObjectBlock* mesh = nullptr;
        ObjectBlock* mesh_instance = nullptr;
        ObjectBlock* mesh_segment = nullptr;
        ObjectBlock* string = nullptr;
        ObjectBlock* node = nullptr;
        ObjectBlock* world_matrix = nullptr;
        ObjectBlock* matrix4 = nullptr;
        ObjectBlock* bone_remap = nullptr;
        ObjectBlock* asset_reference_import = nullptr;
        ObjectBlock* data_block = nullptr;
        ObjectBlock* vertex_stream = nullptr;
        ObjectBlock* material = nullptr;
        ObjectBlock* mesh_instance_bounds = nullptr;
        ObjectBlock* mesh_skel_bounds = nullptr;

        bool initialise(std::istream& stream);

        void unpack_node_element(std::istream& stream, uint32_t elem_ID, Node& node);
        void unpack_world_matrix_element(std::istream& stream, uint32_t elem_ID, GlobalTransform& global_transform);
        void unpack_mesh_segment_element(std::istream& stream, uint32_t elem_ID, const PhyreHeader& header, SubMesh& submesh);
        void unpack_data_block_element(std::istream& stream, uint32_t elem_ID, const PhyreHeader& header, VertexComponent& vcomp);
        void unpack_vertex_stream_element(std::istream& stream, uint32_t elem_ID, const std::vector<SharedData>& shared_data, VertexComponent& vcomp);
        void unpack_model_bounds(std::istream& stream, uint32_t elem_ID, ModelBBox& bounds);
        uint32_t unpack_bone_bounds(std::istream& stream, uint32_t elem_ID, BoneBBox& bounds);

        void pack_node_element(std::ostream& stream, uint32_t elem_ID, const Node& node);
        void pack_world_matrix_element(std::ostream& stream, uint32_t elem_ID, const GlobalTransform& global_transform);
        void pack_mesh_segment_element(std::ostream& stream, uint32_t elem_ID, const PhyreHeader& header, const SubMesh& submesh, uint32_t indices_offset);
        void pack_data_block_element(std::ostream& stream, uint32_t elem_ID, const PhyreHeader& header, const VertexComponent& vcomp, uint32_t vertices_offset);
        void pack_model_bounds(std::ostream& stream, uint32_t elem_ID, const ModelBBox& bounds);
    };

    PhyreHeader header = {};

    FileObjectBlocks obj_blocks;
    std::vector<SharedData> shared_data;

    std::vector<Node> nodes;
    std::vector<Model> models;
    std::vector<Mesh> meshes;
    std::vector<SubMesh> submeshes;
    std::vector<VertexComponent> vertex_components;
    std::vector<GlobalTransform> global_transforms;
    std::vector<ModelBBox> model_bounds;
    std::vector<BoneBBox> bone_bounds;
    std::vector<Material> materials;

    std::map<std::string, Node*> node_map;

    Node* root_node = nullptr;

    bool unpack(const std::string& orig_scene_path);
    bool pack(const std::string& mod_scene_path, const std::string& orig_scene_path);

    bool evaluate_local_material_textures(const std::string& orig_scene_path, const std::string& intr_scene_path);

    bool unpack_shared_data(std::istream& stream);
    bool unpack_nodes(std::istream& stream);
    bool unpack_global_transforms(std::istream& stream);
    bool unpack_models(std::istream& stream);
    bool unpack_meshes(std::istream& stream);
    bool unpack_submeshes(std::istream& stream);
    bool unpack_vertex_streams(std::istream& stream);
    bool unpack_materials(std::istream& stream);

    void join_identical_vertices();
    void recalculate_model_bounds();
    void recalculate_global_transforms();
};