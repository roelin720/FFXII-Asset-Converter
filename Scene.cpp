#include "Scene.h"
#include "ConverterInterface.h"
#include <iostream>
#include <numeric>
#include <execution>
#include <algorithm>
#include <filesystem>
#include <glm/ext.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace IO;

namespace
{
#ifdef GUI_BUILD
    constexpr auto exeq_mode = std::execution::seq;
#else
    constexpr auto exeq_mode = std::execution::par_unseq;
#endif
}

#define CHECKBLOCKREQUIRED(b) if (!b) { gbl_err << "Error: Object block '"#b"' not present" << std::endl; return false; }
#define ASSERTBLOCKREQUIRED(b) if (!b || b->elem_count == 0) { throw std::exception("Error: Object block '"#b"' not present"); }
#define GETVALIDSINGLE(x) [&](){ auto ptr_arr = x; if(ptr_arr.size() != 1) throw std::exception("Failed to get just one value from '"#x"'"); else return ptr_arr[0]; }()

bool Scene::FileObjectBlocks::initialise(std::istream& stream)
{
    int64_t stream_pos = tell(stream);

    if (!LoadObjectBlocks(stream, *this))
    {
        gbl_err << "Failed to load object blocks" << std::endl;
        return false;
    }

    mesh                    = FindObjectBlock(*this, "PMesh");
    mesh_instance           = FindObjectBlock(*this, "PMeshInstance");
    mesh_segment            = FindObjectBlock(*this, "PMeshSegment");
    string                  = FindObjectBlock(*this, "PString");
    node                    = FindObjectBlock(*this, "PNode");
    world_matrix            = FindObjectBlock(*this, "PWorldMatrix");
    matrix4                 = FindObjectBlock(*this, "PMatrix4");
    bone_remap              = FindObjectBlock(*this, "PSkinBoneRemap");
    asset_reference_import  = FindObjectBlock(*this, "PAssetReferenceImport");
    data_block              = FindObjectBlock(*this, "PDataBlock");
    vertex_stream           = FindObjectBlock(*this, "PVertexStream");
    material                = FindObjectBlock(*this, "PMaterial");
    mesh_instance_bounds    = FindObjectBlock(*this, "PMeshInstanceBounds");
    mesh_skel_bounds        = FindObjectBlock(*this, "PSkeletonJointBounds");

    seek(stream, stream_pos, std::ios::beg);

    return true;
}

bool Scene::unpack(const std::string& orig_path)
{
    try
    {
        std::ifstream stream(orig_path, std::ios::binary);
        if (!stream)
        {
            gbl_err << "Could not open " << orig_path << std::endl;
            return false;
        }

        if (!obj_blocks.initialise(stream))
        {
            return false;
        }

        seek(stream, 0, std::ios::beg);
        header = read<PhyreHeader>(stream);

        obj_blocks.bone_remap;
        std::vector<ObjectLink*> skin_bone_remap_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_segment, "PSkinBoneRemap");

        unpack_shared_data(stream);
        unpack_nodes(stream);
        unpack_global_transforms(stream);
        unpack_models(stream);
        unpack_meshes(stream);
        unpack_submeshes(stream);
        unpack_vertex_streams(stream);
        unpack_materials(stream);
    }
    catch (const std::exception& e)
    {
        gbl_err << "Failed to unpack " << orig_path << " - " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool Scene::pack(const std::string& mod_path, const std::string& orig_path)
{
    try
    {
        if (header.magic_bytes == 0)
        {
            gbl_err << orig_path << " has not been unpacked to be able to pack" << std::endl;
            return false;
        }

        std::ifstream istream(orig_path, std::ios::binary);
        if (!istream)
        {
            gbl_err << "Could not open " << orig_path << std::endl;
            return false;
        }

        std::ofstream ostream(mod_path, std::ios::binary);
        if (!ostream)
        {
            gbl_err << "Could not open " << mod_path << std::endl;
            return false;
        }

        //dummify empty submeshes

        std::vector<ObjectLink*> bone_remap_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_segment, "PSkinBoneRemap");
        
        for (int i = 0; i < submeshes.size(); ++i)
        {
            std::vector<ObjectLink*> local_bone_remap_links = FindParentsObjectLinks(bone_remap_links, submeshes[i].ID);
            CONVASSERT(!submeshes[i].bones.empty() || local_bone_remap_links.empty());

            if (!local_bone_remap_links.empty() && submeshes[i].bones.empty()) //originally had bones
            {
                CONVASSERT(!submeshes[i].mesh->global_to_local_bone_ID.empty());

                submeshes[i].bones.push_back(&submeshes[i].mesh->skeleton[submeshes[i].mesh->global_to_local_bone_ID.begin()->first]);
            }

            if (submeshes[i].indices.size() < 3)
            {
                submeshes[i].indices = { 0, 1, 2 };
            }
            for (auto& vcomps : submeshes[i].components)
            {
                for (auto& vcomp : vcomps.second)
                {
                    int32_t min_size = vcomp->elem_size * 3;
                    if (vcomp->data.size() < min_size)
                    {
                        vcomp->elem_count = 3;
                        vcomp->data.resize(min_size);
                        memset(vcomp->data.data(), 0, min_size);
                    }
                }
            }
        }

        std::vector<uint32_t> header_modified_block_IDs;
        std::vector<uint32_t> link_modified_block_IDs;
        std::vector<uint32_t> data_modified_block_IDs;
        std::vector<std::vector<char>> modified_data;

        if (obj_blocks.bone_remap)
        {
            size_t bone_remap_count = [this]() { size_t sum = 0; for (auto& sm : submeshes) sum += sm.bones.size(); return sum; }();

            obj_blocks.bone_remap->elem_count = bone_remap_count;
            obj_blocks.bone_remap->objects_size = bone_remap_count * 4;
            obj_blocks.bone_remap->data_size = obj_blocks.bone_remap->objects_size + obj_blocks.bone_remap->arrays_size;
            
            data_modified_block_IDs.push_back(ObjectBlockID(obj_blocks, "PSkinBoneRemap"));
            link_modified_block_IDs.push_back(ObjectBlockID(obj_blocks, "PMeshSegment"));
            header_modified_block_IDs.push_back(data_modified_block_IDs.back());
            modified_data.push_back({});
            modified_data.back().resize(obj_blocks.bone_remap->data_size);

            uint16_t* bone_remap_data = (uint16_t*)modified_data.back().data();
            uint32_t bone_remap_ID_offset = 0;

            for (size_t i = 0; i < submeshes.size(); ++i)
            {
                if (submeshes[i].bones.size() == 0)
                {
                    continue;
                }

                ObjectLink* local_bone_remap_link = GETVALIDSINGLE(FindParentsObjectLinks(bone_remap_links, submeshes[i].ID));
                CONVASSERT(local_bone_remap_link->obj_array_count < 128);

                local_bone_remap_link->obj_ID = bone_remap_ID_offset;
                local_bone_remap_link->obj_array_count = submeshes[i].bones.size();

                for (size_t j = 0; j < submeshes[i].bones.size(); ++j)
                {
                    uint32_t bone_global_ID = uint32_t(submeshes[i].bones[j] - &submeshes[i].mesh->skeleton[0]);
                    CONVASSERT(bone_global_ID < submeshes[i].mesh->skeleton.size());

                    auto mapping = submeshes[i].mesh->global_to_local_bone_ID.find(bone_global_ID);
                    CONVASSERT(mapping != submeshes[i].mesh->global_to_local_bone_ID.end());

                    bone_remap_data[(bone_remap_ID_offset + j) * 2 + 0] = mapping->first;
                    bone_remap_data[(bone_remap_ID_offset + j) * 2 + 1] = mapping->second;
                }

                bone_remap_ID_offset += submeshes[i].bones.size();
            }
        }

        std::vector<char> prelim_data;

        int64_t prelim_data_size = obj_blocks.back().data_offset + obj_blocks.back().data_size +
            int64_t(header.array_link_size) +
            int64_t(header.object_link_size) +
            int64_t(header.object_array_link_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.shared_data_size) +
            int64_t(header.shared_data_count) * 12;

        prelim_data.resize(prelim_data_size);
        seek(istream, 0, std::ios::beg);
        read(istream, prelim_data.data(), prelim_data.size());
        seek(ostream, 0, std::ios::beg);
        write(ostream, prelim_data.data(), prelim_data.size());
        std::vector<char>().swap(prelim_data);

        ModifyBlockData(ostream, istream, header, obj_blocks, data_modified_block_IDs, modified_data);
        ModifyBlockLinks(ostream, istream, header, obj_blocks, link_modified_block_IDs);
        ModifyBlockHeaders(ostream, header, obj_blocks, header_modified_block_IDs);
        
        //these modifications don't follow the pipeline above because they don't have to
        if (!model_bounds.empty())
        {
            for (const ModelBBox& bounds : model_bounds)
            {
                obj_blocks.pack_model_bounds(ostream, bounds.ID, bounds);
            }
        }
        for (const Node& node: nodes)
        {
            obj_blocks.pack_node_element(ostream, node.ID, node);
        }
        for (const GlobalTransform& gtform : global_transforms)
        {
            obj_blocks.pack_world_matrix_element(ostream, gtform.ID, gtform);
        }

        int64_t index_data_offset = obj_blocks.back().data_offset + obj_blocks.back().data_size +
            int64_t(header.array_link_size) +
            int64_t(header.object_link_size) +
            int64_t(header.object_array_link_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.shared_data_size) +
            int64_t(header.shared_data_count) * 12;

        header.indices_size = 0;
        for (int64_t i = 0; i < submeshes.size(); ++i)
        {
            obj_blocks.pack_mesh_segment_element(ostream, i, header, submeshes[i], index_data_offset + header.indices_size);
            header.indices_size += submeshes[i].indices.size() * 2;
        }
        
        int64_t vertex_data_offset = index_data_offset + header.indices_size;
        header.vertices_size = 0;
        for (int64_t i = 0; i < vertex_components.size(); ++i)
        {
            obj_blocks.pack_data_block_element(ostream, i, header, vertex_components[i], vertex_data_offset + header.vertices_size);
            header.vertices_size += vertex_components[i].elem_size * vertex_components[i].elem_count;
        }
        
        seek(ostream, 0, std::ios::beg);
        write<PhyreHeader>(ostream, header);
    }
    catch (const std::exception& e)
    {
        gbl_err << "Failed to pack " << mod_path << " - " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool Scene::unpack_shared_data(std::istream& stream)
{
    int64_t shared_data_offset = obj_blocks.back().data_offset + obj_blocks.back().data_size;

    shared_data.reserve(header.shared_data_count);

    for (uint32_t i = 0; i < header.shared_data_count; ++i)
    {
        seek(stream, shared_data_offset + header.shared_data_size + i * 12, std::ios::beg);
        shared_data.push_back(SharedData());
        shared_data.back().type = PrimType(read<uint32_t>(stream));
        shared_data.back().data.resize(read<uint32_t>(stream));

        seek(stream, shared_data_offset + read<uint32_t>(stream), std::ios::beg);
        read(stream, shared_data.back().data.data(), shared_data.back().data.size());
    }

    return true;
}

bool Scene::unpack_nodes(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.node);
    std::vector<ObjectLink*> ref_node_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.node, "PNode");

    nodes.resize(obj_blocks.node->elem_count);
    root_node = &nodes[0];

    std::set<uint32_t> node_field_ids;
    for (size_t i = 0; i < ref_node_links.size(); ++i)
    {
        node_field_ids.insert(ref_node_links[i]->parent_obj_offset);
    }
    CONVASSERT(node_field_ids.size() == 3);
    CONVASSERT(*std::next(node_field_ids.begin(), 0) + 1 == *std::next(node_field_ids.begin(), 1));
    CONVASSERT(*std::next(node_field_ids.begin(), 1) + 1 == *std::next(node_field_ids.begin(), 2));

    uint32_t node_parent_field_id = *node_field_ids.begin();

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].ID = i;

        obj_blocks.unpack_node_element(stream, nodes[i].ID, nodes[i]);

        if (i != 0)
        {
            seek(stream, obj_blocks.node->data_offset + obj_blocks.node->objects_size + obj_blocks.node->array_links[i - 1].offset, std::ios::beg);
            nodes[i].name = read<std::string>(stream);

            node_map[nodes[i].name] = &nodes[i];
        }
    }

    for (size_t i = 0; i < ref_node_links.size(); ++i)
    {
        CONVASSERT(ref_node_links[i]->parent_obj_ID < nodes.size());

        Node& node = nodes[ref_node_links[i]->parent_obj_ID];
        if (ref_node_links[i]->parent_obj_offset == node_parent_field_id)
        {
            CONVASSERT(ref_node_links[i]->obj_ID < nodes.size());

            node.parent = &nodes[ref_node_links[i]->obj_ID];
            node.parent->children.push_back(&node);
        }
    }

    CONVASSERT(root_node->parent == nullptr);

    return true;
}

bool Scene::unpack_global_transforms(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.node);
    CHECKBLOCKREQUIRED(obj_blocks.world_matrix);

    std::vector<ObjectLink*> node_world_matrix_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.node, "PWorldMatrix"); 
    CONVASSERT(!node_world_matrix_links.empty());
    CONVASSERT(!nodes.empty());

    global_transforms.resize(obj_blocks.world_matrix->elem_count);

    for (size_t i = 0; i < node_world_matrix_links.size(); ++i)
    {
        int64_t wmat_ID = node_world_matrix_links[i]->obj_ID;
        int64_t node_ID = node_world_matrix_links[i]->parent_obj_ID;
        CONVASSERT(wmat_ID < global_transforms.size());
        CONVASSERT(node_ID < nodes.size());

        global_transforms[wmat_ID].ID = wmat_ID;
        global_transforms[wmat_ID].node = &nodes[node_ID];
        
        nodes[node_ID].global_transform = &global_transforms[wmat_ID];

        obj_blocks.unpack_world_matrix_element(stream, wmat_ID, global_transforms[wmat_ID]);     
    }
    
    return true;
}

bool Scene::unpack_models(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.mesh_instance);
    CONVASSERT(!global_transforms.empty());
    CONVASSERT(!nodes.empty());

    std::vector<ObjectLink*> global_transform_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_instance, "PWorldMatrix");

    models.resize(obj_blocks.mesh_instance->elem_count);

    for (int64_t i = 0; i < obj_blocks.mesh_instance->elem_count; ++i)
    {
        models[i].name = std::string("model") + std::to_string(i);
        models[i].ID = i;
        
        models[i].node = global_transforms[global_transform_links[i]->obj_ID].node;
    }

    if (obj_blocks.mesh_instance_bounds)
    {
        model_bounds.resize(obj_blocks.mesh_instance_bounds->elem_count);

        std::vector<ObjectLink*> bounds_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_instance, "PMeshInstanceBounds");

        for (int64_t i = 0; i < bounds_links.size(); ++i)
        {
            uint32_t model_ID = bounds_links[i]->parent_obj_ID;
            uint32_t bounds_ID = bounds_links[i]->obj_ID;
            model_bounds[i].ID = bounds_ID;
            model_bounds[i].model = &models[model_ID];
            model_bounds[i].model->bounds = &model_bounds[i];

            obj_blocks.unpack_model_bounds(stream, bounds_ID, model_bounds[i]);
        }
    }

    return true;
}

bool Scene::unpack_meshes(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.mesh_instance);
    CHECKBLOCKREQUIRED(obj_blocks.mesh);

    std::vector<ObjectLink*> mesh_instance_mesh_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_instance, "PMesh");
    std::vector<ObjectLink*> bone_transform_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PMatrix4");
    std::vector<ObjectLink*> mesh_segment_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PMeshSegment");
    std::vector<ObjectLink*> skin_bone_remap_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_segment, "PSkinBoneRemap");
    std::vector<ObjectLink*> mesh_bone_name_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PString");
    std::vector<ObjectLink*> bone_bounds_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PSkeletonJointBounds");

    CONVASSERT(!models.empty());

    meshes.resize(obj_blocks.mesh->elem_count);

    for (int64_t i = 0; i < mesh_instance_mesh_links.size(); ++i)
    {
        int64_t model_ID = mesh_instance_mesh_links[i]->parent_obj_ID;
        int64_t mesh_ID = mesh_instance_mesh_links[i]->obj_ID;
        //I'm not sure why it's even possible for more than once reference to exist
        if (std::find_if(meshes[mesh_ID].models.begin(), meshes[mesh_ID].models.end(), [model_ID](const Model* model) {return model->ID == model_ID; }) == meshes[mesh_ID].models.end())
        {
            models[model_ID].mesh = &meshes[mesh_ID];
            meshes[mesh_ID].models.push_back(&models[model_ID]);
        }     
    }

    for (int64_t i = 0; i < obj_blocks.mesh->elem_count; ++i)
    {
        meshes[i].name = "mesh" + std::to_string(i);
        meshes[i].ID = i;

        if (obj_blocks.mesh_skel_bounds)
        {
            bone_bounds.resize(obj_blocks.mesh_skel_bounds->elem_count);
        }

        std::vector<ObjectLink*> local_bone_transform_links = FindParentsObjectLinks(bone_transform_links, meshes[i].ID);     
        if (!local_bone_transform_links.empty()) //has bones
        {
            CHECKBLOCKREQUIRED(obj_blocks.matrix4);
            CHECKBLOCKREQUIRED(obj_blocks.bone_remap);
            CHECKBLOCKREQUIRED(obj_blocks.string);

            //build skeleton
            ObjectLink* local_bone_name_link = GETVALIDSINGLE(FindParentsObjectLinks(mesh_bone_name_links, meshes[i].ID));
            int64_t bone_name_begin = local_bone_name_link->obj_ID;
            int64_t bone_name_count = local_bone_name_link->obj_array_count;

            CONVASSERT(local_bone_transform_links[0]->obj_array_count <= local_bone_transform_links[1]->obj_array_count);
            CONVASSERT(local_bone_transform_links[1]->obj_array_count == bone_name_count);

            int64_t non_influencing_bone_count = local_bone_transform_links[1]->obj_array_count - local_bone_transform_links[0]->obj_array_count;

            meshes[i].skeleton.resize(bone_name_count);

            for (int64_t j = 0; j < bone_name_count; ++j)
            {
                seek(stream, obj_blocks.string->data_offset + obj_blocks.string->objects_size + obj_blocks.string->array_links[bone_name_begin + j].offset, std::ios::beg);
                std::string bone_name = read<std::string>(stream);

                CONVASSERT(node_map.find(bone_name) != node_map.end());

                meshes[i].skeleton[j].node = node_map[bone_name];
                meshes[i].skeleton[j].ID = j;

                seek(stream, obj_blocks.matrix4->data_offset + (local_bone_transform_links[1]->obj_ID + j) * 64, std::ios::beg);
                meshes[i].skeleton[j].transform = read<glm::mat4>(stream);

                if (j >= non_influencing_bone_count)
                {
                    seek(stream, obj_blocks.matrix4->data_offset + (local_bone_transform_links[0]->obj_ID + j - non_influencing_bone_count) * 64, std::ios::beg);
                    meshes[i].skeleton[j].pose = read<glm::mat4>(stream);
                }
            }

            //load bone hierarchy
            ArrayLink* bone_index_array_link = FindParentsArrayLinks(obj_blocks.mesh->array_links, meshes[i].ID)[0];
            CONVASSERT(bone_index_array_link->count == meshes[i].skeleton.size());

            for (int64_t j = 0; j < meshes[i].skeleton.size(); ++j)
            {
                seek(stream, obj_blocks.mesh->data_offset + obj_blocks.mesh->objects_size + bone_index_array_link->offset + j * 4, std::ios::beg);
                int32_t index = read<int32_t>(stream);
                if (index >= 0) meshes[i].skeleton[j].parent = &meshes[i].skeleton[index];
            }

            //gather skin_bone_remap indices
            ObjectLink* local_mesh_segment_link = GETVALIDSINGLE(FindParentsObjectLinks(mesh_segment_links, meshes[i].ID));
            int64_t seg_begin = local_mesh_segment_link->obj_ID;
            int64_t seg_end = seg_begin + local_mesh_segment_link->obj_array_count;

            for (int64_t j = seg_begin; j < seg_end; ++j)
            {
                ObjectLink* skin_bone_remap_link = GETVALIDSINGLE(FindParentsObjectLinks(skin_bone_remap_links, j));

                int64_t br_begin = skin_bone_remap_link->obj_ID;
                int64_t br_end = br_begin + skin_bone_remap_link->obj_array_count;

                for (int64_t k = br_begin; k < br_end; ++k)
                {
                    CONVASSERT(obj_blocks.bone_remap->elem_size == 4);
                    seek(stream, obj_blocks.bone_remap->data_offset + 4 * k, std::ios::beg);
                    int16_t bone_global_ID = read<uint16_t>(stream);
                    int16_t bone_local_ID = read<uint16_t>(stream);

                    meshes[i].global_to_local_bone_ID[bone_global_ID] = bone_local_ID;
                }
            }

            //gather bone bounds
            ObjectLink* local_bone_bounds_link = GETVALIDSINGLE(FindParentsObjectLinks(bone_bounds_links, meshes[i].ID));
            int64_t bbounds_begin = local_bone_bounds_link->obj_ID;
            int64_t bbounds_end = bbounds_begin + local_bone_bounds_link->obj_array_count;
            CONVASSERT(local_bone_bounds_link->obj_array_count == meshes[i].global_to_local_bone_ID.size());

            for (int64_t j = bbounds_begin; j < bbounds_end; ++j)
            {
                bone_bounds[j].ID = j;
                uint32_t bone_ID = obj_blocks.unpack_bone_bounds(stream, j, bone_bounds[j]);
                bone_bounds[j].bone = &meshes[i].skeleton[bone_ID];
                bone_bounds[j].bone->bounds = &bone_bounds[j];
            }
        }
    }

    return true;
}

bool Scene::unpack_submeshes(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.mesh_segment);

    CONVASSERT(!meshes.empty());

    submeshes.resize(obj_blocks.mesh_segment->elem_count);

    std::vector<ObjectLink*> mesh_seg_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PMeshSegment");

    for (int64_t i = 0; i < mesh_seg_links.size(); ++i)
    {
        int64_t mesh_ID = mesh_seg_links[i]->parent_obj_ID;
        int64_t submesh_begin = mesh_seg_links[i]->obj_ID;
        int64_t submesh_count = mesh_seg_links[i]->obj_array_count;
        int64_t submesh_end = submesh_begin + submesh_count;

        meshes[mesh_ID].submeshes.reserve(submesh_count);

        for (int64_t j = submesh_begin; j < submesh_end; ++j)
        {
            meshes[mesh_ID].submeshes.push_back(&submeshes[j]);
            submeshes[j].mesh = &meshes[mesh_ID];
        }
    }

    for (int64_t i = 0; i < obj_blocks.mesh_segment->elem_count; ++i)
    {
        submeshes[i].name = "submesh" + std::to_string(i);
        submeshes[i].ID = i;

        obj_blocks.unpack_mesh_segment_element(stream, submeshes[i].ID, header, submeshes[i]);
    }

    if (obj_blocks.bone_remap)
    {
        std::vector<ObjectLink*> bone_remap_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_segment, "PSkinBoneRemap");

        for (int64_t i = 0; i < bone_remap_links.size(); ++i)
        {
            int64_t submesh_ID = bone_remap_links[i]->parent_obj_ID;
            int64_t br_begin = bone_remap_links[i]->obj_ID;
            int64_t br_count = bone_remap_links[i]->obj_array_count;
            int64_t br_end = br_begin + br_count;

            submeshes[submesh_ID].bones.reserve(br_count);

            for (int64_t j = br_begin; j < br_end; ++j)
            {
                seek(stream, obj_blocks.bone_remap->data_offset + 4 * j, std::ios::beg);
                int16_t bone_global_ID = read<uint16_t>(stream);

                Bone* bone = &submeshes[submesh_ID].mesh->skeleton[bone_global_ID];

                submeshes[submesh_ID].bones.push_back(bone);
            }
        }
    }

    return true;
}

bool Scene::unpack_vertex_streams(std::istream& stream)
{
    CHECKBLOCKREQUIRED(obj_blocks.mesh_segment);
    CHECKBLOCKREQUIRED(obj_blocks.data_block);

    CONVASSERT(!submeshes.empty());

    //a data block may have multiple vertex streams, but it's unclear why. If there are multiple, the rest are ignored.

    vertex_components.resize(obj_blocks.data_block->elem_count);

    for (size_t i = 0; i < vertex_components.size(); ++i)
    {
        vertex_components[i].ID = uint32_t(i);
    }

    std::vector<ObjectLink*> data_block_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh_segment, "PDataBlock");
    std::vector<ObjectLink*> vertex_stream_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.data_block, "PVertexStream");

    for (int64_t i = 0; i < data_block_links.size(); ++i)
    {
        int64_t submesh_ID = data_block_links[i]->parent_obj_ID;
        int64_t data_block_begin = data_block_links[i]->obj_ID;
        int64_t data_block_count = data_block_links[i]->obj_array_count;
        int64_t data_block_end = data_block_begin + data_block_count;

        for (int64_t j = data_block_begin; j < data_block_end; ++j)
        {
            int64_t v_stream_ID = vertex_stream_links[j]->obj_ID;

            //APPLY VSTREAM GROUPING
            obj_blocks.unpack_data_block_element(stream, j, header, vertex_components[j]);
            obj_blocks.unpack_vertex_stream_element(stream, v_stream_ID, shared_data, vertex_components[j]);

            submeshes[submesh_ID].components[vertex_components[j].comp_type].push_back(&vertex_components[j]);
        }
    }

    return true;
}

bool Scene::unpack_materials(std::istream& stream)
{
    ASSERTBLOCKREQUIRED(obj_blocks.mesh);
    ASSERTBLOCKREQUIRED(obj_blocks.asset_reference_import);

    CONVASSERT(!meshes.empty());
    CONVASSERT(!submeshes.empty());

    std::vector<std::string> imported_assets;
    imported_assets.resize(obj_blocks.asset_reference_import->elem_count);

    for (uint32_t i = 0; i < imported_assets.size(); ++i)
    {
        seek(stream, obj_blocks.asset_reference_import->data_offset + obj_blocks.asset_reference_import->objects_size + obj_blocks.asset_reference_import->array_links[i].offset, std::ios::beg);
        imported_assets[i] = read<std::string>(stream);
    }

    std::vector<ObjectLink*> material_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.mesh, "PMaterial");
    std::vector<ObjectLink*> param_buff_links = FindMemberObjectLinks(obj_blocks, *obj_blocks.material, "PParameterBuffer");

    std::map<SubMesh*, std::string> submesh_texture_ref_map;
    std::map<std::string, Material*> texture_ref_material_map;

    for (int64_t i = 0; i < meshes.size(); ++i)
    {
        //this usage of links is very unusual
        std::vector<ObjectLink*> local_material_links = FindParentsObjectLinks(material_links, i);

        std::sort(local_material_links.begin(), local_material_links.end(), [](const ObjectLink* a, const ObjectLink* b) { return a->obj_array_count < b->obj_array_count; });

        for (int64_t j = 0; j < meshes[i].submeshes.size(); ++j)
        {
            CONVASSERT(meshes[i].submeshes[j]->file_material_ID < local_material_links.size());
            int64_t material_ID = local_material_links[meshes[i].submeshes[j]->file_material_ID]->obj_ID;

            ObjectLink* param_buff_link = GETVALIDSINGLE(FindParentsObjectLinks(param_buff_links, material_ID));
            ObjectBlock& param_buff = obj_blocks[param_buff_link->obj_block_ID];

            std::vector<ObjectLink*> asset_ref_links = FindMemberObjectLinks(obj_blocks, param_buff, "PAssetReference");

            std::string texture_ref;
            for (size_t k = 0; k < asset_ref_links.size(); ++k)
            {
                std::string asset_ref = imported_assets[shared_data[asset_ref_links[k]->shared_data_ID].data[1]];
                if (asset_ref.ends_with(".dds"))
                {
                    texture_ref = asset_ref;
                    break;
                }
            }

            if (texture_ref.empty())
            {
                gbl_warn << "Material on submesh '" << meshes[i].submeshes[j]->name << "' has no identifiable texures" << std::endl;
                continue;
            }

            submesh_texture_ref_map[meshes[i].submeshes[j]] = texture_ref;
            texture_ref_material_map.insert({ texture_ref, nullptr });
        }
    }

    materials.reserve(texture_ref_material_map.size());

    for (auto& tm : texture_ref_material_map)
    {
        materials.push_back(Material());
        tm.second = &materials.back();
        tm.second->ID = materials.size() - 1;
        tm.second->texture_ref = tm.first;
    }
    for (auto& st : submesh_texture_ref_map)
    {
        st.first->material = texture_ref_material_map[st.second];
    }

    return true;
}

void Scene::join_identical_vertices()
{
    std::vector<VertexComponent> new_vertex_components;
    new_vertex_components.resize(vertex_components.size());

//#define PRINT_JOIN_RESULTS
#ifdef PRINT_JOIN_RESULTS
    std::atomic<size_t> old_vertex_total = 0;
    std::atomic<size_t> new_vertex_total = 0;
    gbl_log << "Joining identical vertices..." << std::endl;
#define TOTALS , &old_vertex_total, &new_vertex_total
#else
#define TOTALS
#endif

    std::for_each(exeq_mode, submeshes.begin(), submeshes.end(), [this, &new_vertex_components TOTALS](SubMesh& submesh)
    {
        size_t old_vertex_count = submesh.components.begin()->second[0]->elem_count;
        size_t new_vertex_count = 0;

        std::vector<int32_t> new_to_old;
        std::vector<int32_t> old_to_new;
        
        new_to_old.reserve(old_vertex_count);
        old_to_new.resize(old_vertex_count);
        memset(old_to_new.data(), -1, old_to_new.size() * sizeof(old_to_new[0]));
        
        for (size_t i = 0; i < old_vertex_count; ++i)
        {
            if (old_to_new[i] == -1)
            {
                new_to_old.push_back(int32_t(i));
                old_to_new[i] = int32_t(new_vertex_count);
                ++new_vertex_count;
            }

            for (size_t j = i + 1; j < old_vertex_count; ++j)
            {
                if (old_to_new[j] != -1)
                {
                    continue;
                }

                for (const auto& tc : submesh.components)
                {
                    for (const VertexComponent* comp : tc.second)
                    {
                        if (comp->prim_type.ID == PrimID::Float)
                        {
                            constexpr float eps = 1e-5f * 1e-5f;
                            float* i_data = (float*)&comp->data[i * comp->elem_size];
                            float* j_data = (float*)&comp->data[j * comp->elem_size];

                            for (uint32_t k = 0; k < comp->prim_type.count; ++k)
                            {
                                float diff = i_data[k] - j_data[k];
                                if (diff * diff > eps)
                                {
                                    goto label;
                                }
                            }
                        }
                        else if (comp->prim_type.ID == PrimID::UInt8 || comp->prim_type.ID == PrimID::Int8)
                        {
                            uint8_t* i_data = (uint8_t*)&comp->data[i * comp->elem_size];
                            uint8_t* j_data = (uint8_t*)&comp->data[j * comp->elem_size];

                            for (uint32_t k = 0; k < comp->prim_type.count; ++k)
                            {
                                if (i_data[k] != j_data[k])
                                {
                                    goto label;
                                }
                            }
                        }
                        else
                        {
                            CONVASSERT("Vertex component comparison not implemented" == 0);
                        }
                    }
                }
                old_to_new[j] = old_to_new[i];

                label:
                continue;
            }
        }

        std::map<VertexComponentType, std::vector<VertexComponent*>> new_submesh_vcomps;

        for (const auto& src_tc : submesh.components)
        {
            const std::vector<VertexComponent*>& src_vcomps = submesh.components[src_tc.first];
            std::vector<VertexComponent*>& dst_vcomps = new_submesh_vcomps[src_tc.first];
            dst_vcomps.resize(src_vcomps.size());

            for (size_t i = 0; i < src_vcomps.size(); ++i)
            {
                const VertexComponent& src_comp = *src_vcomps[i];
                VertexComponent& dst_comp = *(dst_vcomps[i] = &new_vertex_components[src_comp.ID]);

                dst_comp.ID = src_comp.ID;
                dst_comp.comp_type = src_comp.comp_type;
                dst_comp.elem_size = src_comp.elem_size;
                dst_comp.prim_type = src_comp.prim_type;

                dst_comp.elem_count = new_vertex_count;
                dst_comp.data.resize(new_vertex_count * dst_comp.elem_size);

                for (size_t j = 0; j < new_vertex_count; ++j)
                {
                    memcpy(&dst_comp.data[j * dst_comp.elem_size], &src_comp.data[new_to_old[j] * dst_comp.elem_size], dst_comp.elem_size);
                }
            }
        }

        submesh.components.swap(new_submesh_vcomps);

        for (size_t i = 0; i < submesh.indices.size(); ++i)
        {
            submesh.indices[i] = uint16_t(old_to_new[submesh.indices[i]]);
        };
#ifdef PRINT_JOIN_RESULTS
        old_vertex_total += old_vertex_count;
        new_vertex_total += new_vertex_count;
#endif
    });

    vertex_components.swap(new_vertex_components);

#ifdef PRINT_JOIN_RESULTS
    gbl_log << (old_vertex_total - new_vertex_total) << "/" << old_vertex_total << " removed, " << std::fixed << std::setprecision(2) << (float(old_vertex_total - new_vertex_total) / float(old_vertex_total)) * 100.0f << "% smaller" << std::endl;
#endif
#undef TOTALS
}

void Scene::recalculate_model_bounds()
{
    std::vector<std::pair<glm::vec3, glm::vec3>> submesh_bounds;
    submesh_bounds.resize(submeshes.size());

    std::for_each(exeq_mode, submeshes.begin(), submeshes.end(), [this, &submesh_bounds](const SubMesh& submesh)
    {
        glm::vec3 min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const auto& tc : submesh.components)
        {
            for (const VertexComponent* vcomp : tc.second)
            {
                if (vcomp->comp_type == VertexComponentType::Vertex || vcomp->comp_type == VertexComponentType::SkinnableVertex)
                {
                    glm::vec3* positions = (glm::vec3*)vcomp->data.data();
                    
                    for (int64_t i = 0; i < vcomp->elem_count; ++i)
                    {
                        min = glm::min(min, positions[i]);
                        max = glm::max(max, positions[i]);
                    }
                }
            }       
        }

        submesh_bounds[submesh.ID] = { min, max };
    });

    std::for_each(exeq_mode, models.begin(), models.end(), [this, &submesh_bounds](Model& model)
    {
        glm::vec3 min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (size_t i = 0; i < model.mesh->submeshes.size(); ++i)
        {
            auto& sb = submesh_bounds[model.mesh->submeshes[i]->ID];
            min = glm::min(min, sb.first);
            max = glm::max(max, sb.second);
        }

        model.bounds->min = min;
        model.bounds->max = max;
    });
}

bool approx_eq(const glm::mat4& matrix1, const glm::mat4& matrix2, float epsilon) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (glm::abs(matrix1[i][j] - matrix2[i][j]) > epsilon) {
                return false;
            }
        }
    }
    return true;
}

void Scene::recalculate_global_transforms()
{
    std::for_each(exeq_mode, global_transforms.begin(), global_transforms.end(), [this](GlobalTransform& global_transform)
    {
        glm::mat4 transform(1.0f);
        for (Node* node = global_transform.node; node; node = node->parent)
        {
            transform = node->transform * transform;
        }

        global_transform.transform = transform;
    });
}

void Scene::FileObjectBlocks::unpack_node_element(std::istream& stream, uint32_t elem_ID, Node& node_obj)
{
    ASSERTBLOCKREQUIRED(node);
    assert(node->elem_size == 69 || node->elem_size == 84);

    if (node->elem_size == 84)
    {
        seek(stream, node->data_offset + elem_ID * 84 + 16, std::ios::beg);
        node_obj.transform = read<glm::mat4>(stream);
    }
    else if (node->elem_size == 69)
    {
        seek(stream, node->data_offset + elem_ID * 69 + 4, std::ios::beg);
        node_obj.transform = read<glm::mat4>(stream);
    }
}

void Scene::FileObjectBlocks::unpack_world_matrix_element(std::istream& stream, uint32_t elem_ID, GlobalTransform& global_transform)
{
    ASSERTBLOCKREQUIRED(world_matrix);
    assert(world_matrix->elem_size == 48);

    if (world_matrix->elem_size == 48)
    {
        seek(stream, world_matrix->data_offset + elem_ID * 48, std::ios::beg);
        global_transform.transform = read_3x4(stream);
    }
}

void Scene::FileObjectBlocks::unpack_mesh_segment_element(std::istream& stream, uint32_t elem_ID, const PhyreHeader& header, SubMesh& submesh)
{
    ASSERTBLOCKREQUIRED(mesh_segment);

    assert(mesh_segment->elem_size == 108 || mesh_segment->elem_size == 61);

    int64_t index_count = 0;
    int64_t index_data_offset = back().data_offset + back().data_size +
        int64_t(header.array_link_size) +
        int64_t(header.object_link_size) +
        int64_t(header.object_array_link_size) +
        int64_t(header.header_class_object_block_count) * 4 +
        int64_t(header.header_class_child_count) * 16 +
        int64_t(header.shared_data_size) +
        int64_t(header.shared_data_count) * 12;

    int64_t index_data_size = 0;

    if (mesh_segment->elem_size == 108)
    {
        seek(stream, mesh_segment->data_offset + (int64_t)elem_ID * 108, std::ios::beg);

        submesh.file_material_ID = read<int32_t>(stream);
        seek(stream, 52, std::ios::cur);
        index_count = read<int32_t>(stream);
        seek(stream, 32, std::ios::cur);
        index_data_offset += read<int32_t>(stream);
        seek(stream, 4, std::ios::cur);
        index_data_size = read<int32_t>(stream);
    }
    else if (mesh_segment->elem_size == 61)
    {
        seek(stream, mesh_segment->data_offset + (int64_t)elem_ID * 61, std::ios::beg);

        submesh.file_material_ID = read<int32_t>(stream);
        seek(stream, 9, std::ios::cur);
        uint32_t draw_type = read<int8_t>(stream);
        seek(stream, 28, std::ios::cur);
        index_count = read<int32_t>(stream);
        seek(stream, 7, std::ios::cur);
        index_data_offset += read<int32_t>(stream);
        index_data_size = read<int32_t>(stream);

        CONVASSERT(draw_type == 2); //triangles
    }
    CONVASSERT(index_data_size == index_count * 2);

    seek(stream, index_data_offset, std::ios::beg);
    
    submesh.indices.resize(index_count);
    read(stream, submesh.indices.data(), index_count * 2);
}

void Scene::FileObjectBlocks::unpack_data_block_element(std::istream& stream, uint32_t elem_ID, const PhyreHeader& header, VertexComponent& vcomp)
{
    ASSERTBLOCKREQUIRED(data_block);
    CONVASSERT(data_block->elem_size == 64 || data_block->elem_size == 27);

    int64_t vertex_data_size = 0;
    int64_t vertex_data_offset = back().data_offset + back().data_size +
        int64_t(header.array_link_size) +
        int64_t(header.object_link_size) +
        int64_t(header.object_array_link_size) +
        int64_t(header.header_class_object_block_count) * 4 +
        int64_t(header.header_class_child_count) * 16 +
        int64_t(header.shared_data_size) +
        int64_t(header.shared_data_count) * 12 +
        int64_t(header.indices_size);

    if (data_block->elem_size == 64)
    {
        seek(stream, data_block->data_offset + 64 * elem_ID, std::ios::beg);
        vcomp.elem_size = read<uint32_t>(stream);
        vcomp.elem_count = read<uint32_t>(stream);
        seek(stream, 40, std::ios::cur);
        vertex_data_offset += read<uint32_t>(stream);
        seek(stream, 4, std::ios::cur);
        vertex_data_size = read<uint32_t>(stream);
    }
    else if (data_block->elem_size == 27)
    {
        seek(stream, data_block->data_offset + 27 * elem_ID, std::ios::beg);
        vcomp.elem_size = read<uint32_t>(stream);
        vcomp.elem_count = read<uint32_t>(stream);
        seek(stream, 11, std::ios::cur);
        vertex_data_offset += read<uint32_t>(stream);
        vertex_data_size = read<uint32_t>(stream);
    }

    CONVASSERT(vertex_data_size == vcomp.elem_count * vcomp.elem_size);
    vcomp.data.resize(vertex_data_size);

    seek(stream, vertex_data_offset, std::ios::beg);
    read(stream, vcomp.data.data(), vertex_data_size);
}

void Scene::FileObjectBlocks::unpack_vertex_stream_element(std::istream& stream, uint32_t elem_ID, const std::vector<SharedData>& shared_data, VertexComponent& vcomp)
{
    ASSERTBLOCKREQUIRED(vertex_stream);
    CONVASSERT(!shared_data.empty());
    CONVASSERT(vertex_stream->elem_size == 7 || vertex_stream->elem_size == 12);

    ObjectLink* vcomp_name_link = GETVALIDSINGLE(FindParentsObjectLinks(vertex_stream->object_links, elem_ID));
    CONVASSERT(vcomp_name_link->shared_data_ID < UINT32_MAX);

    std::string vcomp_name = (char*)shared_data[vcomp_name_link->shared_data_ID].data.data();
    vcomp.comp_type = StringToEnum<::VertexComponentType>(vcomp_name.c_str());

    if (vcomp.comp_type == ::VertexComponentType::INVALID)
    {
        throw std::exception(("Unrecognised vertex component type - " + vcomp_name).c_str());
    }

    if (vertex_stream->elem_size == 7)
    {
        seek(stream, vertex_stream->data_offset + 7 * elem_ID + 5, std::ios::beg);
        vcomp.prim_type = PrimType(read<uint8_t>(stream));
    }
    else if (vertex_stream->elem_size == 12)
    {
        seek(stream, vertex_stream->data_offset + 12 * elem_ID + 8, std::ios::beg);
        vcomp.prim_type = PrimType(read<uint8_t>(stream));
    }

    if (vcomp.prim_type.ID == PrimID::INVALID)
    {
        throw std::exception(("Unrecognised vertex component primitive type"));
    }
}

void Scene::FileObjectBlocks::unpack_model_bounds(std::istream& stream, uint32_t elem_ID, ModelBBox& bounds)
{
    ASSERTBLOCKREQUIRED(mesh_instance_bounds);
    CONVASSERT(mesh_instance_bounds->elem_size == 26 || mesh_instance_bounds->elem_size == 36);

    if (mesh_instance_bounds->elem_size == 26)
    {
        seek(stream, mesh_instance_bounds->data_offset + 26 * elem_ID, std::ios::beg);
        bounds.min = read<glm::vec3>(stream);
        seek(stream, 1, std::ios::cur);
        bounds.max = bounds.min + read<glm::vec3>(stream);
    }
    else if (mesh_instance_bounds->elem_size == 36)
    {
        seek(stream, mesh_instance_bounds->data_offset + 36 * elem_ID, std::ios::beg);
        bounds.min = read<glm::vec3>(stream);
        seek(stream, 4, std::ios::cur);
        bounds.max = bounds.min + read<glm::vec3>(stream);
    }
}

uint32_t Scene::FileObjectBlocks::unpack_bone_bounds(std::istream& stream, uint32_t elem_ID, BoneBBox& bounds)
{
    ASSERTBLOCKREQUIRED(mesh_skel_bounds);
    CONVASSERT(mesh_skel_bounds->elem_size == 32);

    seek(stream, mesh_skel_bounds->data_offset + 32 * elem_ID, std::ios::beg);
    bounds.min = read<glm::vec3>(stream);
    uint32_t bone_ID = read<uint32_t>(stream);
    bounds.max = bounds.min = read<glm::vec3>(stream);

    return bone_ID;
}

void Scene::FileObjectBlocks::pack_node_element(std::ostream& stream, uint32_t elem_ID, const Node& node_obj)
{
    ASSERTBLOCKREQUIRED(node);
    assert(node->elem_size == 69 || node->elem_size == 84);

    if (node->elem_size == 84)
    {
        seek(stream, node->data_offset + elem_ID * 84 + 16, std::ios::beg);
        write<glm::mat4>(stream, node_obj.transform);
    }
    else if (node->elem_size == 69)
    {
        seek(stream, node->data_offset + elem_ID * 69 + 4, std::ios::beg);
        write<glm::mat4>(stream, node_obj.transform);
    }
}

void Scene::FileObjectBlocks::pack_world_matrix_element(std::ostream& stream, uint32_t elem_ID, const GlobalTransform& global_transform)
{
    ASSERTBLOCKREQUIRED(world_matrix);
    assert(world_matrix->elem_size == 48);

    if (world_matrix->elem_size == 48)
    {
        seek(stream, world_matrix->data_offset + elem_ID * 48, std::ios::beg);
        write_3x4(stream, global_transform.transform);
    }
}

void Scene::FileObjectBlocks::pack_mesh_segment_element(std::ostream& stream, uint32_t elem_ID, const PhyreHeader& header, const SubMesh& submesh, uint32_t indices_offset)
{
    ASSERTBLOCKREQUIRED(mesh_segment);

    assert(mesh_segment->elem_size == 108 || mesh_segment->elem_size == 61);
 
    auto minmax_index = std::minmax_element(submesh.indices.begin(), submesh.indices.end());

    int64_t index_data_offset_base = back().data_offset + back().data_size +
        int64_t(header.array_link_size) +
        int64_t(header.object_link_size) +
        int64_t(header.object_array_link_size) +
        int64_t(header.header_class_object_block_count) * 4 +
        int64_t(header.header_class_child_count) * 16 +
        int64_t(header.shared_data_size) +
        int64_t(header.shared_data_count) * 12;

    if (mesh_segment->elem_size == 108)
    {
        seek(stream, mesh_segment->data_offset + (int64_t)elem_ID * 108, std::ios::beg);

        seek(stream, 8, std::ios::cur);
        write<uint32_t>(stream, submesh.bones.size());
        seek(stream, 36, std::ios::cur);
        write<uint32_t>(stream, *minmax_index.first);
        write<uint32_t>(stream, *minmax_index.second);
        write<uint32_t>(stream, submesh.indices.size());
        seek(stream, 32, std::ios::cur);
        write<uint32_t>(stream, indices_offset - index_data_offset_base);
        seek(stream, 4, std::ios::cur);
        write<uint32_t>(stream, submesh.indices.size() * 2);
    }
    else if (mesh_segment->elem_size == 61)
    {
        seek(stream, mesh_segment->data_offset + (int64_t)elem_ID * 61, std::ios::beg);
        
        seek(stream, 8, std::ios::cur);
        write<uint32_t>(stream, submesh.bones.size());
        seek(stream, 22, std::ios::cur);
        write<uint32_t>(stream, *minmax_index.first);
        write<uint32_t>(stream, *minmax_index.second);
        write<uint32_t>(stream, submesh.indices.size());
        seek(stream, 7, std::ios::cur);
        write<uint32_t>(stream, indices_offset - index_data_offset_base);
        write<uint32_t>(stream, submesh.indices.size() * 2);
    }

    seek(stream, indices_offset, std::ios::beg);
    write(stream, submesh.indices.data(), submesh.indices.size() * 2);
}

void Scene::FileObjectBlocks::pack_data_block_element(std::ostream& stream, uint32_t elem_ID, const PhyreHeader& header, const VertexComponent& vcomp, uint32_t vertices_offset)
{
    ASSERTBLOCKREQUIRED(data_block);
    CONVASSERT(data_block->elem_size == 64 || data_block->elem_size == 27);

    int64_t vertex_data_offset_base = back().data_offset + back().data_size +
        int64_t(header.array_link_size) +
        int64_t(header.object_link_size) +
        int64_t(header.object_array_link_size) +
        int64_t(header.header_class_object_block_count) * 4 +
        int64_t(header.header_class_child_count) * 16 +
        int64_t(header.shared_data_size) +
        int64_t(header.shared_data_count) * 12 +
        int64_t(header.indices_size);

    if (data_block->elem_size == 64)
    {
        seek(stream, data_block->data_offset + 64 * elem_ID, std::ios::beg);

        seek(stream, 4, std::ios::cur);
        write<uint32_t>(stream, vcomp.elem_count);
        seek(stream, 40, std::ios::cur);
        write<uint32_t>(stream, vertices_offset - vertex_data_offset_base);
        seek(stream, 4, std::ios::cur);
        write<uint32_t>(stream, vcomp.elem_count * vcomp.elem_size);
    }
    else if (data_block->elem_size == 27)
    {
        seek(stream, data_block->data_offset + 27 * elem_ID, std::ios::beg);

        seek(stream, 4, std::ios::cur);
        write<uint32_t>(stream, vcomp.elem_count);
        seek(stream, 11, std::ios::cur);
        write<uint32_t>(stream, vertices_offset - vertex_data_offset_base);
        write<uint32_t>(stream, vcomp.elem_count * vcomp.elem_size);
    }

    seek(stream, vertices_offset, std::ios::beg);
    write(stream, vcomp.data.data(), vcomp.elem_count * vcomp.elem_size);
}

void Scene::FileObjectBlocks::pack_model_bounds(std::ostream& stream, uint32_t elem_ID, const ModelBBox& bounds)
{
    ASSERTBLOCKREQUIRED(mesh_instance_bounds);
    CONVASSERT(mesh_instance_bounds->elem_size == 26 || mesh_instance_bounds->elem_size == 36);

    if (mesh_instance_bounds->elem_size == 26)
    {
        seek(stream, mesh_instance_bounds->data_offset + 26 * elem_ID, std::ios::beg);
        write<glm::vec3>(stream, bounds.min);
        seek(stream, 1, std::ios::cur);
        write<glm::vec3>(stream, bounds.max - bounds.min);
    }
    else if (mesh_instance_bounds->elem_size == 36)
    {
        seek(stream, mesh_instance_bounds->data_offset + 36 * elem_ID, std::ios::beg);
        write<glm::vec3>(stream, bounds.min);
        seek(stream, 4, std::ios::cur);
        write<glm::vec3>(stream, bounds.max - bounds.min);
    }
}

bool Scene::evaluate_local_material_textures(const std::string& orig_scene_path, const std::string& intr_scene_path)
{
    try
    {
        std::map<std::string, std::string> ref_name_id_map;
        std::map<std::string, std::string> ref_name_file_map;

        gbl_log << "Evaluating material textures" << std::endl;

        if (!IO::FindLocalReferences(ref_name_id_map, orig_scene_path) ||
            !IO::EvaluateReferencesFromLocalSearch(ref_name_file_map, ref_name_id_map, intr_scene_path, "png"))
        {
            gbl_warn << "Failed to find any or all material textures" << std::endl;
        }

        for (int64_t i = 0; i < materials.size(); ++i)
        {
            std::string diffuse_ref = IO::ToLower(IO::Normalise(materials[i].texture_ref));

            for (auto& asset_ref_mapping : ref_name_file_map)
            {
                std::string norm_ref = IO::ToLower(IO::Normalise(asset_ref_mapping.first));
                if (norm_ref.contains(diffuse_ref) || IO::ReplaceFirst(norm_ref, "d3d11/", "").contains(diffuse_ref))
                {
                    materials[i].diffuse_texture_path = asset_ref_mapping.second;

                    std::string normal_ref = IO::ReplaceFirst(asset_ref_mapping.first, ".", "_n.");
                    if (auto it = ref_name_file_map.find(normal_ref); it != ref_name_file_map.end())
                    {
                        materials[i].normal_texture_path = it->second;
                    }
                    std::string specular_ref = IO::ReplaceFirst(asset_ref_mapping.first, ".", "_spec.");
                    if (auto it = ref_name_file_map.find(specular_ref); it != ref_name_file_map.end())
                    {
                        materials[i].specular_texture_path = it->second;
                    }
                    break;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        gbl_err << "Error whilst trying to evaluate local material textures for " << orig_scene_path << " at " << intr_scene_path << " - " << e.what() << std::endl;
        return false;
    }
    return true;
}

void SubMesh::clear()
{
    bones.clear();
    indices.clear();
    for (auto& tc : components)
    {
        for (auto& comp : tc.second)
        {
            comp->data.clear();
            comp->elem_count = 0;
        }
    }
}