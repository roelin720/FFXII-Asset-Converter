#include "AssimpConversion.h"
#include "ConverterInterface.h"
#include "../code/PostProcessing/CalcTangentsProcess.h"
#include "../code/PostProcessing/GenVertexNormalsProcess.h"

using namespace AssimpConversion;

namespace
{
    struct aiBoneWeight
    {
        uint8_t bone_ID = 0;
        float   weight = 0.0f;
    };

    std::string get_submesh_node_name(uint32_t model_ID, uint32_t submesh_ID)
    {
        return "subnode_" + std::to_string(model_ID) + "_" + std::to_string(submesh_ID);
    }
    
    template<typename SubMeshT>
    auto* find_either_vcomps(SubMeshT& submesh, VertexComponentType opt_1, VertexComponentType opt_2)
    {
        auto it1 = submesh.components.find(opt_1);
        auto it2 = submesh.components.find(opt_2);
        constexpr auto null_ret = decltype(&it1->second){nullptr};
        CONVASSERT
        (
            (it1 == submesh.components.end() || it1->second.empty()) ||
            (it2 == submesh.components.end() || it2->second.empty())
        );
        if (it1 == submesh.components.end() || it1->second.empty()) 
        {
            if (it2 != submesh.components.end() && !it2->second.empty())
            { 
                return &it2->second; 
            }
        }
        else return &it1->second; 
        return null_ret;
    }

    template<typename SubMeshT>
    auto* find_vcomps(SubMeshT& submesh, VertexComponentType type)
    {
        auto it = submesh.components.find(type);
        constexpr auto null_ret = decltype(&it->second){nullptr};
        return it == submesh.components.end() || it->second.empty() ? null_ret : &it->second;
    }

    void store_nodes(std::vector<aiNode*>& aNodes, aiNode& aNode, const Node& node)
    {
        aNode.mNumChildren = node.children.size();
        aNode.mChildren = new aiNode * [node.children.size()];
        memset(aNode.mChildren, 0, aNode.mNumChildren * 8ull);

        for (int64_t i = 0; i < node.children.size(); ++i)
        {
            aNode.mChildren[i] = aNodes[node.children[i]->ID];
            aNode.mChildren[i]->mName = (aiString)node.children[i]->name;
            aNode.mChildren[i]->mParent = &aNode;
            aNode.mChildren[i]->mTransformation = Convert<aiMatrix4x4>(node.children[i]->transform);

            if (!node.children[i]->children.empty())
            {
                store_nodes(aNodes, *aNode.mChildren[i], *node.children[i]);
            }
        }
    }

    void gather_nodes(std::map<std::string, aiNode*>& aNode_map, aiNode& aNode)
    {
        for (uint32_t i = 0; i < aNode.mNumChildren; ++i)
        {
            aNode_map[aNode.mChildren[i]->mName.C_Str()] = aNode.mChildren[i];
            gather_nodes(aNode_map, *aNode.mChildren[i]);
        }
    }

    int extract_last_num(const std::string& input) 
    {
        int last_num = 0, cur_num = 0;
        bool in_num = false;

        for (char c : input)
        {
            if (std::isdigit(c)) 
            {
                cur_num = cur_num * 10 + (c - '0');
                in_num = true;
            }
            else if (in_num) 
            {
                last_num = cur_num;
                cur_num = 0;
                in_num = false;
            }
        }
        if (in_num) {
            last_num = cur_num;
        }
        return last_num;
    }

    class TangentProcessor : Assimp::CalcTangentsProcess
    {
        public: bool process(aiMesh* pMesh) { return Assimp::CalcTangentsProcess::ProcessMesh(pMesh, 0);  }
    };
}

bool AssimpConversion::ConvertToAssimpScene(aiScene& aScene, const Scene& scene)
{
    try
    {
        aiString missing_tex_material_name = aiString("missing_tex");
        aScene.mNumMaterials = scene.materials.size() + 1; //+ extra null material
        aScene.mMaterials = new aiMaterial * [scene.materials.size() + 1];
        memset(aScene.mMaterials, 0, size_t(aScene.mNumMaterials) * 8);

        aScene.mMaterials[scene.materials.size()] = new aiMaterial();
        aScene.mMaterials[scene.materials.size()]->AddProperty(&missing_tex_material_name, AI_MATKEY_NAME);

        for (size_t i = 0; i < scene.materials.size(); ++i)
        {
            aiString ai_material_name = aiString(IO::BaseStem(scene.materials[i].texture_ref));
            aiString ai_texture_ref_name = aiString(scene.materials[i].texture_ref);
            aiString ai_diffuse_tex_name = aiString(scene.materials[i].diffuse_texture_path);
            aiString ai_normal_tex_name = aiString(scene.materials[i].normal_texture_path);
            aiString ai_specular_tex_name = aiString(scene.materials[i].specular_texture_path);

            aScene.mMaterials[i] = new aiMaterial();
            aScene.mMaterials[i]->AddProperty(&ai_material_name, AI_MATKEY_NAME);
            if (ai_diffuse_tex_name.length > 0)
            {
                aScene.mMaterials[i]->AddProperty(&ai_diffuse_tex_name, AI_MATKEY_TEXTURE_DIFFUSE(0));
                if (ai_normal_tex_name.length > 0) aScene.mMaterials[i]->AddProperty(&ai_normal_tex_name, AI_MATKEY_TEXTURE_NORMALS(0));
                if (ai_specular_tex_name.length > 0) aScene.mMaterials[i]->AddProperty(&ai_specular_tex_name, AI_MATKEY_TEXTURE_SPECULAR(0));
            }
            else
            {
                aScene.mMaterials[i]->AddProperty(&ai_texture_ref_name, AI_MATKEY_TEXTURE_DIFFUSE(0));
            }
        }

        std::vector<aiNode*> aNodes;
        aNodes.resize(scene.nodes.size());

        for (int32_t i = 0; i < aNodes.size(); ++i)
        {
            aNodes[i] = new aiNode();
        }
        aScene.mRootNode = aNodes[0];

        store_nodes(aNodes, *aScene.mRootNode, *scene.root_node);

        std::vector<aiMesh*> additional_meshes;

        for (int64_t i = 0; i < scene.models.size(); ++i)
        {
            aiNode* node = aNodes[scene.models[i].node->ID];

            if (scene.models[i].mesh->submeshes.size() == 1)
            {
                node->mName = (aiString)get_submesh_node_name(scene.models[i].ID, scene.models[i].mesh->submeshes[0]->ID);
                node->mNumMeshes = 1;
                node->mMeshes = new uint32_t[1];
                node->mMeshes[0] = scene.models[i].mesh->submeshes[0]->ID;
            }
            else
            {
                std::vector<aiNode*> submesh_nodes;

                submesh_nodes.resize(scene.models[i].mesh->submeshes.size());

                for (int64_t j = 0; j < submesh_nodes.size(); ++j)
                {
                    submesh_nodes[j] = new aiNode();
                    submesh_nodes[j]->mName = (aiString)get_submesh_node_name(scene.models[i].ID, scene.models[i].mesh->submeshes[j]->ID);
                    submesh_nodes[j]->mNumMeshes = 1;
                    submesh_nodes[j]->mMeshes = new uint32_t[1];
                    submesh_nodes[j]->mMeshes[0] = scene.models[i].mesh->submeshes[j]->ID;
                }

                node->addChildren(submesh_nodes.size(), submesh_nodes.data());
            }

            //fixes a bug where the armature modifier doesn't show up 
            std::vector<bool> bones_used;
            bones_used.resize(scene.models[i].mesh->skeleton.size());

            for (size_t j = 0; j < scene.models[i].mesh->submeshes.size(); ++j)
            {
                for (Bone* bone : scene.models[i].mesh->submeshes[j]->bones)
                {
                    bones_used[bone->ID] = true;
                }
            }

            std::vector<Bone*> unused_bones;
            unused_bones.reserve(bones_used.size());

            for (size_t j = 1; j < bones_used.size(); ++j) //ignore root bone
            {
                if (!bones_used[j]) unused_bones.push_back(&scene.models[i].mesh->skeleton[j]);
            }
            if (unused_bones.empty())
            {
                continue;
            }

            aiNode* dummy_aNode = new aiNode("[ignore" + std::to_string(i) + "]");
            node->addChildren(1, &dummy_aNode);

            dummy_aNode->mMeshes = new unsigned int[1];
            dummy_aNode->mNumMeshes = 1;
            dummy_aNode->mMeshes[0] = scene.submeshes.size() + additional_meshes.size();

            aiMesh* dummy_aMesh = new aiMesh();
            dummy_aMesh->mNumBones = unused_bones.size();
            dummy_aMesh->mBones = new aiBone * [unused_bones.size()];
            for (size_t j = 0; j < unused_bones.size(); ++j)
            {
                dummy_aMesh->mBones[j] = new aiBone();
                dummy_aMesh->mBones[j]->mName = unused_bones[j]->node->name;
                dummy_aMesh->mBones[j]->mOffsetMatrix = Convert<aiMatrix4x4>(unused_bones[j]->pose);
            }
            dummy_aMesh->mNumFaces = 1;
            dummy_aMesh->mNumVertices = 1;
            dummy_aMesh->mNumUVComponents[0] = 1;
            dummy_aMesh->mVertices = new aiVector3D[1]();
            dummy_aMesh->mNormals = new aiVector3D[1]();
            dummy_aMesh->mTangents = new aiVector3D[1]();
            dummy_aMesh->mBitangents = new aiVector3D[1]();
            dummy_aMesh->mColors[0] = new aiColor4D[1]();
            dummy_aMesh->mTextureCoords[0] = new aiVector3D[1]();
            dummy_aMesh->mFaces = new aiFace[1]();
            dummy_aMesh->mFaces->mNumIndices = 1;
            dummy_aMesh->mFaces->mIndices = new unsigned int[1] { 0 };

            additional_meshes.push_back(dummy_aMesh);
        }

        aScene.mNumMeshes = scene.submeshes.size() + additional_meshes.size();
        aScene.mMeshes = new aiMesh * [aScene.mNumMeshes];
        memset(aScene.mMeshes, 0, size_t(aScene.mNumMeshes) * 8);
        memcpy(&aScene.mMeshes[scene.submeshes.size()], additional_meshes.data(), 8ull * additional_meshes.size());

        for (int64_t i = 0; i < scene.submeshes.size(); ++i)
        {
            aScene.mMeshes[i] = new aiMesh();
            aScene.mMeshes[i]->mName = (aiString)scene.submeshes[i].name;
            aScene.mMeshes[i]->mMaterialIndex = scene.submeshes[i].material ? scene.submeshes[i].material->ID : scene.materials.size();
            aScene.mMeshes[i]->mNumFaces = scene.submeshes[i].indices.size() / 3;
            aScene.mMeshes[i]->mFaces = new aiFace[aScene.mMeshes[i]->mNumFaces];
            memset(aScene.mMeshes[i]->mFaces, 0, size_t(aScene.mMeshes[i]->mNumFaces) * 8);

            for (uint64_t j = 0; j < aScene.mMeshes[i]->mNumFaces; ++j)
            {
                aScene.mMeshes[i]->mFaces[j].mNumIndices = 3;
                aScene.mMeshes[i]->mFaces[j].mIndices = new uint32_t[3];
                aScene.mMeshes[i]->mFaces[j].mIndices[0] = scene.submeshes[i].indices[j * 3 + 0];
                aScene.mMeshes[i]->mFaces[j].mIndices[1] = scene.submeshes[i].indices[j * 3 + 1];
                aScene.mMeshes[i]->mFaces[j].mIndices[2] = scene.submeshes[i].indices[j * 3 + 2];
            }

            aScene.mMeshes[i]->mNumVertices = scene.submeshes[i].components.begin()->second[0]->elem_count;

            auto uv_comps = find_vcomps(scene.submeshes[i], VertexComponentType::ST);
            auto pos_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Vertex, VertexComponentType::SkinnableVertex);
            auto normal_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Normal, VertexComponentType::SkinnableNormal);
            auto tangent_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Tangent, VertexComponentType::SkinnableTangent);
            auto bitangent_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Binormal, VertexComponentType::SkinnableBinormal);
            auto colour_comps = find_vcomps(scene.submeshes[i], VertexComponentType::Color);
            auto bone_weight_comps = find_vcomps(scene.submeshes[i], VertexComponentType::SkinWeights);
            auto bone_index_comps = find_vcomps(scene.submeshes[i], VertexComponentType::SkinIndices);

            if (pos_comps)
            {
                CONVASSERT(pos_comps->size() == 1);
                CONVASSERT(pos_comps->at(0)->elem_size == 12);
                CONVASSERT(pos_comps->at(0)->vertex_stream_count == 1);

                aScene.mMeshes[i]->mNumVertices = (uint32_t)aScene.mMeshes[i]->mNumVertices;
                aScene.mMeshes[i]->mVertices = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mVertices, pos_comps->at(0)->data.data(), pos_comps->at(0)->data.size());
            }
            else
            {
                LOG(ERR) << "Position vertex componenet not present in submesh " + std::to_string(i) << std::endl;
                return false;
            }
            
            if (scene.submeshes[i].has_mesh_keys_in_UVs)
            {
                uint32_t vertex_stream_count = uv_comps->at(1)->vertex_stream_count;

                for (uint32_t j = 1; j < uv_comps->size(); ++j)
                {
                    if (aScene.mMeshes[i]->mNumVertices == 0 || uv_comps->at(j)->elem_count % aScene.mMeshes[i]->mNumVertices != 0)
                    {
                        LOG(ERR) << "Submesh " << scene.submeshes[i].name << "'s mesh key's vertex count in vertex stream in UVs is not a multiple of the number of vertices, expected n * " << aScene.mMeshes[i]->mNumVertices << " but contains " << uv_comps->at(j)->elem_count << " at uv " << j << std::endl;
                        return false;
                    }

                    if (aScene.mMeshes[i]->mNumVertices == 0 || uv_comps->at(j)->elem_count / aScene.mMeshes[i]->mNumVertices != vertex_stream_count)
                    {
                        LOG(ERR) << "Submesh " << scene.submeshes[i].name << "'s mesh key's vertex count in vertex stream in UVs is not a multiple of the number of vertices, expected " << vertex_stream_count << " * " << aScene.mMeshes[i]->mNumVertices << " but contains " << uv_comps->at(j)->elem_count << " at uv " << j << std::endl;
                        return false;
                    }

                    if (uv_comps->at(j)->vertex_stream_count != vertex_stream_count)
                    {
                        LOG(ERR) << "Submesh " << scene.submeshes[i].name << "'s mesh key doesn't not have the correct number of vertex streams, expected " << vertex_stream_count << " but contains " << uv_comps->at(j)->vertex_stream_count << " at uv " << j << std::endl;
                        return false;
                    }
                }

                CONVASSERT(tangent_comps->size() == bitangent_comps->size() && tangent_comps->size() > 1);

                uint32_t anim_comp_count = uint32_t((uv_comps->size() - 1) / 2);
                aScene.mMeshes[i]->mNumAnimMeshes = vertex_stream_count * anim_comp_count /* + tangent_comps->size() - 1 + bitangent_comps->size() - 1*/;
                aScene.mMeshes[i]->mAnimMeshes = new aiAnimMesh * [aScene.mMeshes[i]->mNumAnimMeshes];
                aScene.mMeshes[i]->mMethod = aiMorphingMethod_VERTEX_BLEND;
                memset(aScene.mMeshes[i]->mAnimMeshes, 0, aScene.mMeshes[i]->mNumAnimMeshes * 8ull);

                for (uint32_t vs = 0; vs < vertex_stream_count; ++vs)
                {
                    for (uint32_t j = 0; j < anim_comp_count; ++j)
                    {
                        uint32_t vcomp_index = 1 + j * 2;
                        uint32_t anim_index = j + vs * anim_comp_count;
                        glm::vec2* uv_sets[2] = { (glm::vec2*)uv_comps->at(vcomp_index)->data.data(), (glm::vec2*)uv_comps->at(vcomp_index + 1)->data.data() };
                        
                        CONVASSERT(normal_comps != nullptr);

                        aScene.mMeshes[i]->mAnimMeshes[anim_index] = new aiAnimMesh();
                        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNumVertices = aScene.mMeshes[i]->mNumVertices;
                        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mName = "AnimKey" + std::to_string(anim_index);
                                                      
                        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mVertices = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNormals = new aiVector3D[aScene.mMeshes[i]->mNumVertices];  //assimp needs this to import back. Not sure if new ones should be generated, but none exist in the original scene
                        
                        for (uint32_t k = 0; k < aScene.mMeshes[i]->mNumVertices; ++k)
                        {
                            uint32_t uv_index = k + vs * aScene.mMeshes[i]->mNumVertices;
                            aScene.mMeshes[i]->mAnimMeshes[anim_index]->mVertices[k] = aiVector3D(uv_sets[0][uv_index].x, uv_sets[0][uv_index].y, uv_sets[1][uv_index].x);
                            aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNormals[k] = ((aiVector3D*)normal_comps->at(0)->data.data())[k];
                        }
                    }
                }

                /*
                ** the following code exists to store the extra tangents and bitangets that come with anim meshes
                ** but these values don't seem to actually be used in-game so they're being ommited
                ** this is because the main normals, tangents and bitangents all contain the same values, which cannot be correct
                */
                //auto assign_unknown_meshes = [&aScene, &normal_comps, &bitangent_comps, &tangent_comps, i](decltype(tangent_comps)& comps, std::string type, size_t mesh_offset)
                //{
                //    for (uint32_t t = 1; t < comps->size(); ++t)
                //    {
                //        uint32_t anim_index = (t - 1) + mesh_offset;
                //        aScene.mMeshes[i]->mAnimMeshes[anim_index] = new aiAnimMesh();
                //        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNumVertices = aScene.mMeshes[i]->mNumVertices;
                //        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mName = type + "Key" + std::to_string(t - 1);
                //
                //        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mVertices = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                //        aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNormals = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                //
                //        for (uint32_t k = 0; k < aScene.mMeshes[i]->mNumVertices; ++k)
                //        {
                //            if (k == 0)
                //            {
                //                std::cout << t << std::endl;
                //                std::cout << (glm::vec3&)((((glm::vec3*)comps->at(t)->data.data())[k])) << std::endl;;
                //                std::cout << ((glm::vec3*)normal_comps->at(0)->data.data())[k] << std::endl;;
                //                std::cout << ((glm::vec3*)tangent_comps->at(0)->data.data())[k] << std::endl;;
                //                std::cout << ((glm::vec3*)bitangent_comps->at(0)->data.data())[k] << std::endl;;
                //            }
                //
                //            aScene.mMeshes[i]->mAnimMeshes[anim_index]->mVertices[k] = (aiVector3D&)((((glm::vec3*)comps->at(t)->data.data())[k]));
                //            aScene.mMeshes[i]->mAnimMeshes[anim_index]->mNormals[k] = ((aiVector3D*)normal_comps->at(0)->data.data())[k];
                //        }
                //    }
                //};
                //assign_unknown_meshes(tangent_comps, "UnknownT", vertex_stream_count * anim_comp_count);
                //assign_unknown_meshes(bitangent_comps, "UnknownB", vertex_stream_count * anim_comp_count + tangent_comps->size() - 1);
            }

            if (scene.submeshes[i].has_colours_in_UVs)
            {
                aScene.mMeshes[i]->mColors[0] = new aiColor4D[aScene.mMeshes[i]->mNumVertices];

                glm::vec2* uv_sets[2] = { (glm::vec2*)uv_comps->at(1)->data.data(), (glm::vec2*)uv_comps->at(2)->data.data() };

                for (uint32_t k = 0; k < aScene.mMeshes[i]->mNumVertices; ++k)
                {
                    aScene.mMeshes[i]->mColors[0][k] = aiColor4D(uv_sets[0][k].x, uv_sets[0][k].y, uv_sets[1][k].x, uv_sets[1][k].y);
                }
            }

            if (normal_comps)
            {
                CONVASSERT(normal_comps->size() == 1);
                CONVASSERT(normal_comps->at(0)->elem_size == 12);
                CONVASSERT(normal_comps->at(0)->vertex_stream_count == 1);

                aScene.mMeshes[i]->mNormals = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mNormals, normal_comps->at(0)->data.data(), normal_comps->at(0)->data.size());
            }
            
            if (uv_comps)
            {
                CONVASSERT(uv_comps->size() <= AI_MAX_NUMBER_OF_TEXTURECOORDS);

                aScene.mMeshes[i]->mTextureCoordsNames = new aiString * [AI_MAX_NUMBER_OF_TEXTURECOORDS];
                memset(aScene.mMeshes[i]->mTextureCoordsNames, 0, AI_MAX_NUMBER_OF_TEXTURECOORDS * 8ull);

                uint32_t uv_comp_count = scene.submeshes[i].has_mesh_keys_in_UVs || scene.submeshes[i].has_colours_in_UVs ? 1 : uv_comps->size();
                for (int32_t k = 0; k < uv_comp_count; ++k)
                {
                    CONVASSERT(uv_comps->at(k)->elem_size == 8);
                    CONVASSERT(uv_comps->at(k)->vertex_stream_count == 1);

                    aScene.mMeshes[i]->mTextureCoordsNames[k] = new aiString(("UV" + std::to_string(i)).c_str());
                    aScene.mMeshes[i]->mTextureCoords[k] = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                    aScene.mMeshes[i]->mNumUVComponents[k] = 2;

                    glm::vec2* uv_data = (glm::vec2*)uv_comps->at(k)->data.data();
                    for (int32_t l = 0; l < aScene.mMeshes[i]->mNumVertices; ++l)
                    {
                        aScene.mMeshes[i]->mTextureCoords[k][l] = aiVector3D(uv_data[l][0], 1.0f - uv_data[l][1], 0.0f);
                    }
                }
            }

            if (tangent_comps)
            {
                CONVASSERT(tangent_comps->at(0)->elem_size == 12);
                CONVASSERT(tangent_comps->at(0)->vertex_stream_count == 1);

                aScene.mMeshes[i]->mTangents = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mTangents, tangent_comps->at(0)->data.data(), tangent_comps->at(0)->data.size());
            }

            if (bitangent_comps)
            {
                CONVASSERT(bitangent_comps->at(0)->elem_size == 12);
                CONVASSERT(bitangent_comps->at(0)->vertex_stream_count == 1);

                aScene.mMeshes[i]->mBitangents = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mBitangents, bitangent_comps->at(0)->data.data(), bitangent_comps->at(0)->data.size());
            }

            if (colour_comps)
            {
                CONVASSERT(colour_comps->size() + scene.submeshes[i].has_colours_in_UVs <= AI_MAX_NUMBER_OF_COLOR_SETS);

                for (int32_t k = 0; k < colour_comps->size(); ++k)
                {
                    CONVASSERT(colour_comps->at(k)->elem_size == 16);
                    CONVASSERT(colour_comps->at(k)->vertex_stream_count == 1);

                    int32_t colour_set_index = scene.submeshes[i].has_colours_in_UVs ? k + 1 : k;
                    aScene.mMeshes[i]->mColors[colour_set_index] = new aiColor4D[aScene.mMeshes[i]->mNumVertices];
                    memcpy(aScene.mMeshes[i]->mColors[colour_set_index], colour_comps->at(k)->data.data(), colour_comps->at(k)->data.size());
                }
            }

            if (!scene.submeshes[i].bones.empty())
            {
                std::vector<glm::vec4> bone_weight_arr(aScene.mMeshes[i]->mNumVertices);
                std::vector<glm::u8vec4> bone_id_arr(aScene.mMeshes[i]->mNumVertices);
                std::vector<std::vector<aiVertexWeight>> aibone_weight_arr(scene.submeshes[i].bones.size());

                if (bone_weight_comps)
                {
                    CONVASSERT(bone_weight_comps->size() == 1);
                    CONVASSERT(bone_weight_comps->at(0)->elem_size == 16);
                    CONVASSERT(bone_weight_comps->at(0)->vertex_stream_count == 1);

                    memcpy(bone_weight_arr.data(), bone_weight_comps->at(0)->data.data(), bone_weight_comps->at(0)->data.size());
                }

                if (bone_index_comps)
                {
                    CONVASSERT(bone_index_comps->size() == 1);
                    CONVASSERT(bone_index_comps->at(0)->elem_size == 4);
                    CONVASSERT(bone_index_comps->at(0)->vertex_stream_count == 1);

                    memcpy(bone_id_arr.data(), bone_index_comps->at(0)->data.data(), bone_index_comps->at(0)->data.size());
                }

                for (uint32_t j = 0; j < aScene.mMeshes[i]->mNumVertices; ++j)
                {
                    for (uint32_t k = 0; k < 4; ++k)
                    {
                        aibone_weight_arr[bone_id_arr[j][k]].push_back(aiVertexWeight(j, bone_weight_arr[j][k]));
                    }
                }

                aScene.mMeshes[i]->mNumBones = scene.submeshes[i].bones.size();
                aScene.mMeshes[i]->mBones = new aiBone * [scene.submeshes[i].bones.size()];
                memset(aScene.mMeshes[i]->mBones, 0, size_t(aScene.mMeshes[i]->mNumBones) * 8);

                for (uint64_t j = 0; j < scene.submeshes[i].bones.size(); ++j)
                {
                    aScene.mMeshes[i]->mBones[j] = new aiBone();
                    aScene.mMeshes[i]->mBones[j]->mName = scene.submeshes[i].bones[j]->node->name;
                    aScene.mMeshes[i]->mBones[j]->mOffsetMatrix = *(aiMatrix4x4*)&scene.submeshes[i].bones[j]->pose;

                    aScene.mMeshes[i]->mBones[j]->mNumWeights = (uint32_t)aibone_weight_arr[j].size();
                    if (aScene.mMeshes[i]->mBones[j]->mNumWeights)
                    {
                        aScene.mMeshes[i]->mBones[j]->mWeights = new aiVertexWeight[aScene.mMeshes[i]->mBones[j]->mNumWeights];
                        memcpy(aScene.mMeshes[i]->mBones[j]->mWeights, aibone_weight_arr[j].data(), sizeof(aiVertexWeight) * aScene.mMeshes[i]->mBones[j]->mNumWeights);
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERR) << "Failed to convert scene to assimp scene - " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool AssimpConversion::ConvertFromAssimpScene(const aiScene& aScene, Scene& scene)
{
    try
    {
        std::map<std::string, aiNode*> aNode_map;
        gather_nodes(aNode_map, *aScene.mRootNode);
    
        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            if (auto it = aNode_map.find(scene.nodes[i].name); it != aNode_map.end())
            {
                Node* node = &scene.nodes[i];
                aiNode* aNode = it->second;
    
                node->transform = Convert<glm::mat4>(aNode->mTransformation);
            }
        }

        std::vector<aiMesh*> aSubmeshes;
        aSubmeshes.resize(scene.submeshes.size());

        for (int64_t i = 0; i < scene.models.size(); ++i)
        {
            aiMesh* aSubmesh = nullptr;
            for (int64_t j = 0; j < scene.models[i].mesh->submeshes.size(); ++j)
            {
                std::string submesh_node_name = get_submesh_node_name(scene.models[i].ID, scene.models[i].mesh->submeshes[j]->ID);
                if (auto it = aNode_map.find(submesh_node_name); it != aNode_map.end())
                {
                    if (it->second->mNumMeshes > 0)
                    {
                        if (it->second->mNumMeshes > 1)
                        {
                            LOG(WARN) << "submesh node " << submesh_node_name << " has more than one mesh. The rest will be discarded" << std::endl;
                        }

                        aSubmesh = aScene.mMeshes[it->second->mMeshes[0]];
                        if (aSubmesh->mNumFaces == 0 || aSubmesh->mNumVertices < 3)
                        {
                            aSubmesh = nullptr;
                        }
                        aSubmeshes[scene.models[i].mesh->submeshes[j]->ID] = aSubmesh;
                    }
                }
            }          
        }
        
        for (size_t i = 0; i < scene.submeshes.size(); ++i)
        {
            SubMesh* submesh = &scene.submeshes[i];
            aiMesh* aSubmesh = aSubmeshes[i];
           
            if (aSubmesh == nullptr)
            {
                submesh->clear();
                continue;
            }

            submesh->indices.resize(size_t(aSubmesh->mNumFaces) * 3);

            for (size_t j = 0; j < aSubmesh->mNumFaces; ++j)
            {
                if (aSubmesh->mFaces[j].mNumIndices != 3)
                {
                    LOG(ERR) << "Non-triangle face detected on submesh " << submesh->name <<  std::endl;
                    continue;
                }
                submesh->indices[j * 3 + 0] = aSubmesh->mFaces[j].mIndices[0];
                submesh->indices[j * 3 + 1] = aSubmesh->mFaces[j].mIndices[1];
                submesh->indices[j * 3 + 2] = aSubmesh->mFaces[j].mIndices[2];
            }

            if (submesh->bones.empty() && aSubmesh->HasBones())
            {
                LOG(ERR) << "submesh " << submesh->name << " shouldn't reference bones, but does" << std::endl;
                return false;
            }

            submesh->bones.resize(aSubmesh->mNumBones);

            if (aSubmesh->HasBones())
            {
                for (uint32_t j = 0; j < aSubmesh->mNumBones; ++j)
                {
                    const aiBone* aBone = aSubmesh->mBones[j];
                    std::string bone_name = aBone->mName.C_Str();

                    auto it = std::find_if(submesh->mesh->skeleton.begin(), submesh->mesh->skeleton.end(),
                    [bone_name](const Bone& bone) 
                    { 
                        return bone.node->name == bone_name; 
                    });

                    if (it == submesh->mesh->skeleton.end())
                    {
                        LOG(ERR) << "Failed to find bone '" << bone_name << "' in skeleton '" << submesh->mesh->skeleton[0].node->name << "' of submesh '" << submesh->name << "' " << std::endl;
                        return false;
                    }
                    else submesh->bones[j] = &*it;

                    uint32_t bone_global_ID = uint32_t(submesh->bones[j] - &submesh->mesh->skeleton[0]);

                    if (submesh->mesh->global_to_local_bone_ID.find(bone_global_ID) == submesh->mesh->global_to_local_bone_ID.end())
                    {
                        LOG(ERR) << "Bone '" << bone_name << "' is not a usable bone in skeleton '" << submesh->mesh->skeleton[0].node->name << "' of submesh '" << submesh->name << "' " << std::endl;
                        return false;
                    }
                }
            }

            //a submesh might have multiple normals, tangents, bitangents, but these can't be stored in the file via assimp nor be easilly visible and editable in blender
            //hence, they're all reconstructed using the first set of normals as a base

            auto uv_comps = find_vcomps(*submesh, VertexComponentType::ST);
            auto pos_comps = find_either_vcomps(*submesh, VertexComponentType::Vertex, VertexComponentType::SkinnableVertex);
            auto normal_comps = find_either_vcomps(*submesh, VertexComponentType::Normal, VertexComponentType::SkinnableNormal);
            auto tangent_comps = find_either_vcomps(*submesh, VertexComponentType::Tangent, VertexComponentType::SkinnableTangent);
            auto bitangent_comps = find_either_vcomps(*submesh, VertexComponentType::Binormal, VertexComponentType::SkinnableBinormal);
            auto colour_comps = find_vcomps(*submesh, VertexComponentType::Color);
            auto bone_weight_comps = find_vcomps(*submesh, VertexComponentType::SkinWeights);
            auto bone_index_comps = find_vcomps(*submesh, VertexComponentType::SkinIndices);

            CONVASSERT(pos_comps && pos_comps->size() == 1);
            CONVASSERT(normal_comps || (!tangent_comps && !bitangent_comps));
            CONVASSERT(aSubmesh->HasNormals() || !aSubmesh->HasTangentsAndBitangents());

            size_t tbn_comp_count = std::max({ (normal_comps ? normal_comps->size() : 0), (tangent_comps ? tangent_comps->size() : 0), (bitangent_comps ? bitangent_comps->size() : 0) });
            
            if (pos_comps)
            {
                pos_comps->at(0)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                pos_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                memcpy(pos_comps->at(0)->data.data(), aSubmesh->mVertices, pos_comps->at(0)->data.size());
            }
            if (normal_comps) //the rest of the TBN is dealt with in the UV section
            {
                normal_comps->at(0)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                normal_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                memcpy(normal_comps->at(0)->data.data(), aSubmesh->mNormals, normal_comps->at(0)->data.size());
            }
            if (tangent_comps)
            {
                tangent_comps->at(0)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                tangent_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                memcpy(tangent_comps->at(0)->data.data(), aSubmesh->mTangents, tangent_comps->at(0)->data.size());
            }
            if (bitangent_comps)
            {
                bitangent_comps->at(0)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                bitangent_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                memcpy(bitangent_comps->at(0)->data.data(), aSubmesh->mBitangents, bitangent_comps->at(0)->data.size());
            }

            if (colour_comps)
            {
                size_t aColour_count = aSubmesh->GetNumColorChannels();
                size_t expected_colour_count = colour_comps->size() + submesh->has_colours_in_UVs;
                if (aColour_count != expected_colour_count)
                {
                    LOG(ERR) << "submesh " << submesh->name << " doesn't have the correct number of colour channels (expected " << expected_colour_count << " but contains " << aColour_count << ")" << std::endl;
                    return false;
                }

                for (int32_t j = 0; j < colour_comps->size(); ++j)
                {
                    int32_t colour_set_index = submesh->has_colours_in_UVs ? j + 1 : j;
                    colour_comps->at(j)->data.resize(sizeof(glm::vec4)* aSubmesh->mNumVertices);
                    colour_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                    memcpy(colour_comps->at(j)->data.data(), aSubmesh->mColors[colour_set_index], colour_comps->at(j)->data.size());
                }
            }

            if (submesh->has_mesh_keys_in_UVs)
            {
                uint32_t vertex_stream_count = uv_comps->at(1)->vertex_stream_count;
                uint32_t anim_comp_count = (uv_comps->size() - 1) / 2;
                size_t expected_anim_mesh_count = vertex_stream_count * anim_comp_count /* + tangent_comps->size() - 1 + bitangent_comps->size() - 1 */;
                if (aSubmesh->mNumAnimMeshes != expected_anim_mesh_count)
                {
                    LOG(ERR) << "Expected " << expected_anim_mesh_count << " morph/shape keys from submesh " << submesh->name << " but got " << aSubmesh->mNumAnimMeshes << std::endl;
                    return false;
                }
            }

            if (uv_comps)
            {
                size_t aUV_count = aSubmesh->GetNumUVChannels();
                size_t expected_UV_count = submesh->has_mesh_keys_in_UVs || submesh->has_colours_in_UVs ? 1 : uv_comps->size();
                if (aUV_count != expected_UV_count)
                {
                    LOG(ERR) << "submesh " << submesh->name << " doesn't have the correct number of UV channels (expected " << expected_UV_count << " but contains " << aUV_count << ")" << std::endl;
                    return false;
                }

                for (int32_t j = 0; j < uv_comps->size(); ++j)
                {
                    if ((j == 0 || (!submesh->has_mesh_keys_in_UVs && !submesh->has_colours_in_UVs)))
                    {
                        uv_comps->at(j)->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices);
                        uv_comps->at(j)->elem_count = aSubmesh->mNumVertices;

                        glm::vec2* uv_data = (glm::vec2*)uv_comps->at(j)->data.data();

                        for (int32_t k = 0; k < aSubmesh->mNumVertices; ++k)
                        {
                            uv_data[k] = glm::vec2(aSubmesh->mTextureCoords[j][k][0], 1.0f - aSubmesh->mTextureCoords[j][k][1]);
                        }
                    }

                    if (j > 0)
                    {
                        if (submesh->has_colours_in_UVs && (j & 1) == 1)
                        {
                            uv_comps->at(j)->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices);
                            uv_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                            uv_comps->at(j + size_t(1))->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices);
                            uv_comps->at(j + size_t(1))->elem_count = aSubmesh->mNumVertices;
                            glm::vec2* uv_data0 = (glm::vec2*)uv_comps->at(j)->data.data();
                            glm::vec2* uv_data1 = (glm::vec2*)uv_comps->at(j + size_t(1))->data.data();

                            for (int32_t l = 0; l < aSubmesh->mNumVertices; ++l)
                            {
                                aiColor4D& colour = aSubmesh->mColors[0][l];
                                uv_data0[l] = glm::vec2(colour[0], colour[1]);
                                uv_data1[l] = glm::vec2(colour[2], colour[3]);
                            }
                        }
                        else if (submesh->has_mesh_keys_in_UVs && (j & 1) == 1)
                        {
                            uint32_t vertex_stream_count = uv_comps->at(j)->vertex_stream_count;
                            uint32_t anim_comp_count = (uv_comps->size() - 1) / 2;

                            uv_comps->at(j)->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices * vertex_stream_count);
                            uv_comps->at(j)->elem_count = aSubmesh->mNumVertices * vertex_stream_count;
                            uv_comps->at(j + size_t(1))->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices * vertex_stream_count);
                            uv_comps->at(j + size_t(1))->elem_count = aSubmesh->mNumVertices * vertex_stream_count;
                            glm::vec2* uv_data0 = (glm::vec2*)uv_comps->at(j)->data.data();
                            glm::vec2* uv_data1 = (glm::vec2*)uv_comps->at(j + size_t(1))->data.data();

                            auto mesh_begin = aSubmesh->mAnimMeshes, mesh_end = aSubmesh->mAnimMeshes + aSubmesh->mNumAnimMeshes;
                            auto initialise_mesh_list = [&aSubmesh, &submesh](std::vector<aiAnimMesh*>& list, const std::string& type, size_t expected_elem_count)
                            {
                                auto mesh_begin = aSubmesh->mAnimMeshes, mesh_end = aSubmesh->mAnimMeshes + aSubmesh->mNumAnimMeshes;
                                std::copy_if(mesh_begin, mesh_end, std::back_inserter(list), [&type](const aiAnimMesh* m) { return std::string(m->mName.C_Str()).contains(type); });
                                std::sort(list.begin(), list.end(), [](auto a, auto b) { return extract_last_num(a->mName.C_Str()) < extract_last_num(b->mName.C_Str()); });
                                if (list.size() != expected_elem_count)
                                {
                                    LOG(ERR) << "submesh " << submesh->name << " doesn't have the correct number of shape keys of type " << type << " (expected " << expected_elem_count << " but contains " << list.size() << ")" << std::endl;
                                    return false;
                                }
                                return true;
                            };

                            std::vector<aiAnimMesh*> anim_meshes;
                            //std::vector<aiAnimMesh*> tangent_meshes;
                            //std::vector<aiAnimMesh*> bitangent_meshes;
                            if (!initialise_mesh_list(anim_meshes, "Anim", anim_comp_count * vertex_stream_count)
                            //    || !initialise_mesh_list(tangent_meshes, "UnknownT", tangent_comps->size() - 1)
                            //    || !initialise_mesh_list(bitangent_meshes, "UnknownB", bitangent_comps->size() - 1
                            ) {
                                return false;
                            }

                            for (int64_t vs = 0; vs < vertex_stream_count; ++vs)
                            {
                                uint32_t anim_index = ((j - 1) / 2) + vs * anim_comp_count;

                                aiAnimMesh* anim_mesh = anim_meshes[anim_index];

                                if (!anim_mesh->HasPositions())
                                {
                                    LOG(ERR) << "Morph/shape key " << anim_mesh->mName << " on submesh \"" << submesh->name << "\" has no positions" << std::endl;
                                    return false;
                                }
                                CONVASSERT(aSubmesh->mNumVertices == anim_mesh->mNumVertices);

                                for (int32_t l = 0; l < aSubmesh->mNumVertices; ++l)
                                {
                                    uint32_t uv_index = l + vs * aSubmesh->mNumVertices;

                                    //if (uv_data0[uv_index] != glm::vec2(anim_mesh->mVertices[l].x, anim_mesh->mVertices[l].y)) __debugbreak();
                                    //if (uv_data1[uv_index] != glm::vec2(anim_mesh->mVertices[l].z, 0.0f)) __debugbreak();

                                    uv_data0[uv_index] = glm::vec2(anim_mesh->mVertices[l].x, anim_mesh->mVertices[l].y);
                                    uv_data1[uv_index] = glm::vec2(anim_mesh->mVertices[l].z, 0.0f);
                                }
                            }

                            auto initialise_tangents = [normal_comps, &aSubmesh, &submesh](decltype(tangent_comps)& comps)
                            {
                                for (size_t c = 1; c < comps->size(); ++c)
                                {
                                    comps->at(c)->data.resize(sizeof(glm::vec3)* aSubmesh->mNumVertices);
                                    comps->at(c)->elem_count = aSubmesh->mNumVertices;
                                }
                            };
                            initialise_tangents(tangent_comps);
                            initialise_tangents(bitangent_comps);
                            /*
                            ** the following code exists to store the extra tangents and bitangets that come with anim meshes
                            ** but these values don't seem to actually be used in-game so they're being ommited
                            ** this is because the main normals, tangents and bitangents all contain the same values, which cannot be correct
                            */
                            //auto assign_unknown_meshes = [normal_comps ,&aSubmesh, &submesh](decltype(tangent_comps)& comps, decltype(tangent_meshes)& meshes)
                            //{
                            //    for (size_t c = 1; c < comps->size(); ++c)
                            //    {
                            //        aiAnimMesh* anim_mesh = meshes[c - 1];
                            //
                            //        if (!anim_mesh->HasPositions())
                            //        {
                            //            LOG(ERR) << "Morph/shape key " << anim_mesh->mName << " on submesh \"" << submesh->name << "\" has no positions" << std::endl;
                            //            return false;
                            //        }
                            //        CONVASSERT(aSubmesh->mNumVertices == anim_mesh->mNumVertices);
                            //
                            //        comps->at(c)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                            //        comps->at(c)->elem_count = aSubmesh->mNumVertices;
                            //        glm::vec3* data = (glm::vec3*)comps->at(0)->data.data();
                            //
                            //        for (int32_t l = 0; l < aSubmesh->mNumVertices; ++l)
                            //        {
                            //            data[l] = (((glm::vec3*)normal_comps->at(0)->data.data())[l]);
                            //        }
                            //    }
                            //    return true;
                            //};
                            //if (!assign_unknown_meshes(tangent_comps, tangent_meshes) ||
                            //    !assign_unknown_meshes(bitangent_comps, bitangent_meshes)
                            //) {
                            //    return false;
                            //}
                        }
                        
                        if (tbn_comp_count > 1 && !submesh->has_mesh_keys_in_UVs)
                        {
                            Assimp::GenVertexNormalsProcess normal_processor;
                            TangentProcessor tangent_processor;
                            aiMesh tp_submesh;
                            memcpy(&tp_submesh, aSubmesh, sizeof(aiMesh));

                            tp_submesh.mNormals = nullptr;
                            tp_submesh.mTangents = nullptr;
                            tp_submesh.mBitangents = nullptr;

                            if (!normal_processor.GenMeshVertexNormals(&tp_submesh, i))
                            {
                                LOG(ERR) << "Failed to calculate normals " << j << " for " << submesh->name << std::endl;
                                memset(&tp_submesh, 0, sizeof(aiMesh)); //prevent pointer destruction
                                return false;
                            }
                            if (!tangent_processor.process(&tp_submesh))
                            {
                                LOG(ERR) << "Failed to calculate tangents/bitangets " << j << " for " << submesh->name << std::endl;
                                memset(&tp_submesh, 0, sizeof(aiMesh)); //prevent pointer destruction
                                return false;
                            }
                            if (j < normal_comps->size())
                            {
                                normal_comps->at(j)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                                normal_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                                memcpy(normal_comps->at(j)->data.data(), tp_submesh.mNormals, normal_comps->at(j)->data.size());
                            }
                            if (j < tangent_comps->size())
                            {
                                tangent_comps->at(j)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                                tangent_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                                memcpy(tangent_comps->at(j)->data.data(), tp_submesh.mTangents, tangent_comps->at(j)->data.size());
                            }
                            if (j < bitangent_comps->size())
                            {
                                bitangent_comps->at(j)->data.resize(sizeof(glm::vec3) * aSubmesh->mNumVertices);
                                bitangent_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                                memcpy(bitangent_comps->at(j)->data.data(), tp_submesh.mBitangents, bitangent_comps->at(j)->data.size());
                            }
                            delete[] tp_submesh.mNormals;
                            delete[] tp_submesh.mTangents;
                            delete[] tp_submesh.mBitangents;

                            memset(&tp_submesh, 0, sizeof(aiMesh)); //prevent pointer destruction
                        }
                    }                  
                }
            }          

            if (aSubmesh->HasBones())
            {
                CONVASSERT(bone_weight_comps != nullptr && bone_index_comps != nullptr);

                std::vector<std::vector<aiBoneWeight>> aBone_weights(aSubmesh->mNumVertices);

                for (uint32_t j = 0; j < aSubmesh->mNumBones; ++j)
                {
                    const aiBone& aBone = *aSubmesh->mBones[j];

                    for (uint32_t k = 0; k < aBone.mNumWeights; ++k)
                    {
                        int32_t vertex_ID = aBone.mWeights[k].mVertexId;
                        aBone_weights[vertex_ID].push_back(aiBoneWeight{(uint8_t)j, aBone.mWeights[k].mWeight });
                    }
                }

                for (uint32_t j = 0; j < aSubmesh->mNumVertices; ++j)
                {
                    std::sort(aBone_weights[j].begin(), aBone_weights[j].end(),
                    [](const aiBoneWeight& a, const aiBoneWeight& b) -> bool 
                    {
                        return a.weight > b.weight;
                    });
                    //not sure what happened to make the following necessary
                    std::set<int8_t> used_ids;
                    aBone_weights[j].erase(std::remove_if(aBone_weights[j].begin(), aBone_weights[j].end(), [&](const aiBoneWeight& bw) { return !used_ids.insert(bw.bone_ID).second; }), aBone_weights[j].end());
                    aBone_weights[j].resize(4);
                    { // satisfy requirement for weights to be of only 3 significant figures
                        float sum = 0.0f;
                        for (auto& b : aBone_weights[j])
                        {
                            sum += b.weight;
                        }
                        if (sum > 0.0001f)
                        {
                            float sum_inv = 1.0f / sum;
                            float sum2 = 0.0f;
                            for (auto& b : aBone_weights[j])
                            {
                                b.weight = std::round(b.weight * sum_inv * 100) / 100;
                                sum2 += b.weight;
                            }
                            float rem = 1.0f - sum2;

                            for (auto& b : aBone_weights[j])
                            {
                                if (rem > 0.0f || b.weight >= -rem)
                                {
                                    b.weight = std::round((b.weight + rem) * 100) / 100;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (bone_weight_comps)
                {
                    bone_weight_comps->at(0)->data.resize(sizeof(glm::vec4) * aSubmesh->mNumVertices);
                    bone_weight_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                    glm::vec4* bone_weights = (glm::vec4*)bone_weight_comps->at(0)->data.data();

                    for (uint32_t j = 0; j < aSubmesh->mNumVertices; ++j)
                    {
                        bone_weights[j][0] = aBone_weights[j][0].weight;
                        bone_weights[j][1] = aBone_weights[j][1].weight;
                        bone_weights[j][2] = aBone_weights[j][2].weight;
                        bone_weights[j][3] = aBone_weights[j][3].weight;
                    }
                }
                if (bone_index_comps)
                {
                    bone_index_comps->at(0)->data.resize(4ull * aSubmesh->mNumVertices);
                    bone_index_comps->at(0)->elem_count = aSubmesh->mNumVertices;
                    glm::u8vec4* bone_indices = (glm::u8vec4*)bone_index_comps->at(0)->data.data();

                    for (uint32_t j = 0; j < aSubmesh->mNumVertices; ++j)
                    {
                        bone_indices[j][0] = aBone_weights[j][0].bone_ID;
                        bone_indices[j][1] = aBone_weights[j][1].bone_ID;
                        bone_indices[j][2] = aBone_weights[j][2].bone_ID;
                        bone_indices[j][3] = aBone_weights[j][3].bone_ID;
                    }
                }
            }
        }

        scene.join_identical_vertices();

        //applying submesh transforms
        for (int64_t i = 0; i < scene.models.size(); ++i)
        {
            if (scene.models[i].mesh->submeshes.size() == 1)
            {
                //apply the submesh_node transform to model's node
                aiNode* aSubmesh_node = nullptr;
                std::string submesh_node_name = get_submesh_node_name(scene.models[i].mesh->ID, scene.models[i].mesh->submeshes[0]->ID);

                if (auto it = aNode_map.find(submesh_node_name); it == aNode_map.end())
                {
                    LOG(WARN) << "Failed to find submesh_node " << submesh_node_name << std::endl;
                    continue;
                }
                else aSubmesh_node = it->second;

                scene.models[i].node->transform *= Convert<glm::mat4>(aSubmesh_node->mTransformation);
            }
            else
            {
                //apply submesh_node transforms to submeshes
                if (scene.models[i].mesh->models.size() > 1)
                {
                    LOG(WARN) << "submesh_node transforms could not be uniquely applied. Undesirable transforms may occur" << std::endl;
                }

                for (int64_t j = 0; j < scene.models[i].mesh->submeshes.size(); ++j)
                {
                    SubMesh* submesh = scene.models[i].mesh->submeshes[j];
                    aiNode* aSubmesh_node = nullptr;
                    std::string submesh_node_name = get_submesh_node_name(scene.models[i].mesh->ID, submesh->ID);

                    if (auto it = aNode_map.find(submesh_node_name); it == aNode_map.end())
                    {
                        LOG(WARN) << "Failed to find submesh_node " << submesh_node_name << std::endl;
                        continue;
                    }
                    else aSubmesh_node = it->second;

                    glm::mat4 transform = Convert<glm::mat4>(aSubmesh_node->mTransformation);
                    glm::mat3 normal_transform = glm::mat3(glm::transpose(glm::inverse(transform)));

                    for (auto& tc : submesh->components)
                    {
                        for (auto& component : tc.second)
                        {
                            if (tc.first == VertexComponentType::Vertex || tc.first == VertexComponentType::SkinnableVertex)
                            {
                                glm::vec3* positions = (glm::vec3*)component->data.data();

                                for (uint64_t k = 0; k < component->elem_count; ++k)
                                {
                                    positions[k] = glm::vec3(transform * glm::vec4(positions[k], 1.0f));
                                }
                            }
                            if (tc.first == VertexComponentType::Normal || tc.first == VertexComponentType::SkinnableNormal ||
                                tc.first == VertexComponentType::Tangent || tc.first == VertexComponentType::SkinnableTangent ||
                                tc.first == VertexComponentType::Binormal || tc.first == VertexComponentType::SkinnableBinormal
                            ) {
                                glm::vec3* normals = (glm::vec3*)component->data.data();

                                for (uint64_t k = 0; k < component->elem_count; ++k)
                                {
                                    normals[k] = normal_transform * normals[k];
                                }
                            }
                        }
                    }
                }
            }
        }

        scene.recalculate_model_bounds();
        scene.recalculate_global_transforms();
    }
    catch (const std::exception& e)
    {
        LOG(ERR) << "Failed to convert assimp scene to scene - " << e.what() << std::endl;
        return false;
    }
    return true;
}

void AssimpConversion::ClearUnusedBones(aiScene& aScene)
{
    for (uint32_t i = 0; i < aScene.mNumMeshes; ++i)
    {
        aiMesh* aSubmesh = aScene.mMeshes[i];

        if (aSubmesh->HasBones())
        {
            std::vector<uint32_t> used_local_bone_ids;
            used_local_bone_ids.reserve(aSubmesh->mNumBones);

            uint32_t old_bone_count = aSubmesh->mNumBones;
            aiBone** old_bones = aSubmesh->mBones;

            for (uint32_t j = 0; j < old_bone_count; ++j)
            {
                const aiBone* aBone = old_bones[j];

                if (aBone->mNumWeights > 0)
                {
                    used_local_bone_ids.push_back(j);
                }
            }

            aSubmesh->mNumBones = used_local_bone_ids.size();
            aSubmesh->mBones = new aiBone * [used_local_bone_ids.size()];

            for (uint32_t j = 0; j < used_local_bone_ids.size(); ++j)
            {
                aSubmesh->mBones[j] = old_bones[used_local_bone_ids[j]];
                old_bones[used_local_bone_ids[j]] = nullptr;
            }

            for (uint32_t j = 0; j < old_bone_count; ++j)
            {
                delete old_bones[j];
            }
            delete[] old_bones;
        }
    }
}
