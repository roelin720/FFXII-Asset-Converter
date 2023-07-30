#include "AssimpConversion.h"
#include "ConverterInterface.h"
#include "../code/PostProcessing/CalcTangentsProcess.h"

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

        for (uint64_t i = 0; i < additional_meshes.size(); ++i)
        {
            aScene.mMeshes[scene.submeshes.size() + i] = additional_meshes[i];
        }

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

            if (auto pos_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Vertex, VertexComponentType::SkinnableVertex))
            {
                CONVASSERT(pos_comps->size() == 1);
                CONVASSERT(pos_comps->at(0)->elem_size == 12);

                aScene.mMeshes[i]->mNumVertices = (uint32_t)aScene.mMeshes[i]->mNumVertices;
                aScene.mMeshes[i]->mVertices = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mVertices, pos_comps->at(0)->data.data(), pos_comps->at(0)->data.size());
            }
            else
            {
                gbl_err << "Position vertex componenet not present in submesh " + std::to_string(i) << std::endl;
                return false;
            }

            if (auto normal_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Normal, VertexComponentType::SkinnableNormal))
            {
                CONVASSERT(normal_comps->at(0)->elem_size == 12);

                aScene.mMeshes[i]->mNormals = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mNormals, normal_comps->at(0)->data.data(), normal_comps->at(0)->data.size());
            }
            ;
            if (auto uv_comps = find_vcomps(scene.submeshes[i], VertexComponentType::ST))
            {
                CONVASSERT(uv_comps->size() <= AI_MAX_NUMBER_OF_TEXTURECOORDS);

                aScene.mMeshes[i]->mTextureCoordsNames = new aiString * [AI_MAX_NUMBER_OF_TEXTURECOORDS];
                memset(aScene.mMeshes[i]->mTextureCoordsNames, 0, AI_MAX_NUMBER_OF_TEXTURECOORDS * 8ull);

                for (int32_t k = 0; k < uv_comps->size(); ++k)
                {
                    CONVASSERT(uv_comps->at(k)->elem_size == 8);

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

            if (auto tangent_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Tangent, VertexComponentType::SkinnableTangent))
            {
                CONVASSERT(tangent_comps->at(0)->elem_size == 12);

                aScene.mMeshes[i]->mTangents = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mTangents, tangent_comps->at(0)->data.data(), tangent_comps->at(0)->data.size());
            }

            if (auto bitangent_comps = find_either_vcomps(scene.submeshes[i], VertexComponentType::Binormal, VertexComponentType::SkinnableBinormal))
            {
                CONVASSERT(bitangent_comps->at(0)->elem_size == 12);

                aScene.mMeshes[i]->mBitangents = new aiVector3D[aScene.mMeshes[i]->mNumVertices];
                memcpy(aScene.mMeshes[i]->mBitangents, bitangent_comps->at(0)->data.data(), bitangent_comps->at(0)->data.size());
            }

            if (auto colour_comps = find_vcomps(scene.submeshes[i], VertexComponentType::Color))
            {
                CONVASSERT(colour_comps->size() <= AI_MAX_NUMBER_OF_COLOR_SETS);

                for (int32_t k = 0; k < colour_comps->size(); ++k)
                {
                    CONVASSERT(colour_comps->at(k)->elem_size == 16);

                    aScene.mMeshes[i]->mColors[k] = new aiColor4D[aScene.mMeshes[i]->mNumVertices];
                    memcpy(aScene.mMeshes[i]->mColors[k], colour_comps->at(0)->data.data(), colour_comps->at(0)->data.size());
                }
            }

            if (!scene.submeshes[i].bones.empty())
            {
                std::vector<glm::vec4> bone_weight_arr(aScene.mMeshes[i]->mNumVertices);
                std::vector<glm::u8vec4> bone_id_arr(aScene.mMeshes[i]->mNumVertices);
                std::vector<std::vector<aiVertexWeight>> aibone_weight_arr(scene.submeshes[i].bones.size());

                if (auto weight_comps = find_vcomps(scene.submeshes[i], VertexComponentType::SkinWeights))
                {
                    CONVASSERT(weight_comps->size() == 1);
                    CONVASSERT(weight_comps->at(0)->elem_size == 16);

                    memcpy(bone_weight_arr.data(), weight_comps->at(0)->data.data(), weight_comps->at(0)->data.size());
                }

                if (auto id_comps = find_vcomps(scene.submeshes[i], VertexComponentType::SkinIndices))
                {
                    CONVASSERT(id_comps->size() == 1);
                    CONVASSERT(id_comps->at(0)->elem_size == 4);

                    memcpy(bone_id_arr.data(), id_comps->at(0)->data.data(), id_comps->at(0)->data.size());
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
        gbl_err << "Failed to convert scene to assimp scene - " << e.what() << std::endl;
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
                            gbl_warn << "submesh node " << submesh_node_name << " has more than one mesh. The rest will be discarded" << std::endl;
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
                    gbl_err << "Non triangle face detected on submesh " << submesh->name <<  std::endl;
                    continue;
                }
                submesh->indices[j * 3 + 0] = aSubmesh->mFaces[j].mIndices[0];
                submesh->indices[j * 3 + 1] = aSubmesh->mFaces[j].mIndices[1];
                submesh->indices[j * 3 + 2] = aSubmesh->mFaces[j].mIndices[2];
            }

            if (aSubmesh->mNumFaces & 1)
            {
                submesh->indices.push_back(0);
            }

            if (submesh->bones.empty() && aSubmesh->HasBones())
            {
                gbl_err << "submesh " << submesh->name << " shouldn't reference bones, but does" << std::endl;
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
                    [bone_name](const Bone& bone) { 
                        return bone.node->name == bone_name; 
                    });

                    if (it == submesh->mesh->skeleton.end())
                    {
                        gbl_err << "Failed to find bone '" << bone_name << "' in skeleton '" << submesh->mesh->skeleton[0].node->name << "' of submesh '" << submesh->name << "' " << std::endl;
                        return false;
                    }
                    else submesh->bones[j] = &*it;

                    uint32_t bone_global_ID = uint32_t(submesh->bones[j] - &submesh->mesh->skeleton[0]);

                    if (submesh->mesh->global_to_local_bone_ID.find(bone_global_ID) == submesh->mesh->global_to_local_bone_ID.end())
                    {
                        gbl_err << "Bone '" << bone_name << "' is not a usable bone in skeleton '" << submesh->mesh->skeleton[0].node->name << "' of submesh '" << submesh->name << "' " << std::endl;
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
                if (aColour_count != colour_comps->size())
                {
                    gbl_err << "submesh " << submesh->name << " doesn't have the correct number of colour channels (expected " << colour_comps->size() << " but contains " << aColour_count << ")" << std::endl;
                    return false;
                }

                for (int32_t j = 0; j < colour_comps->size(); ++j)
                {
                    colour_comps->at(j)->data.resize(sizeof(glm::vec4) * aSubmesh->mNumVertices);
                    colour_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                    memcpy(colour_comps->at(0)->data.data(), aSubmesh->mColors[j], colour_comps->at(0)->data.size());
                }
            }
            if (uv_comps)
            {
                size_t aUV_count = aSubmesh->GetNumUVChannels();
                if (aUV_count != uv_comps->size())
                {
                    gbl_err << "submesh " << submesh->name << " doesn't have the correct number of UV channels (expected " << uv_comps->size() << " but contains " << aUV_count << ")" << std::endl;
                    return false;
                }
                CONVASSERT(tbn_comp_count == aUV_count);

                for (int32_t j = 0; j < uv_comps->size(); ++j)
                {
                    uv_comps->at(j)->data.resize(sizeof(glm::vec2) * aSubmesh->mNumVertices);
                    uv_comps->at(j)->elem_count = aSubmesh->mNumVertices;
                    glm::vec2* uvs = (glm::vec2*)uv_comps->at(j)->data.data();

                    for (int32_t k = 0; k < aSubmesh->mNumVertices; ++k)
                    {
                        uvs[k] = glm::vec2(aSubmesh->mTextureCoords[j][k][0], 1.0f - aSubmesh->mTextureCoords[j][k][1]);
                    }

                    if (j > 0 && normal_comps) //the first TBN have already been calculated by assimp
                    {
                        TangentProcessor tangent_processor;
                        aiMesh tp_submesh;
                        memcpy(&tp_submesh, aSubmesh, sizeof(aiMesh));

                        tp_submesh.mTangents = nullptr;
                        tp_submesh.mBitangents = nullptr;
                        std::swap(tp_submesh.mTextureCoords[0], tp_submesh.mTextureCoords[j]);

                        if (!tangent_processor.process(&tp_submesh))
                        {
                            gbl_err << "Failed to calculate tangents/bitangets " << j << " for " << submesh->name << std::endl;
                            return false;
                        }
                        if (j < normal_comps->size())
                        {
                            normal_comps->at(j)->data.resize(sizeof(glm::vec3)* aSubmesh->mNumVertices);
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
                        delete[] tp_submesh.mTangents;
                        delete[] tp_submesh.mBitangents;

                        memset(&tp_submesh, 0, sizeof(aiMesh)); //prevent pointer destruction
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
                    [](const aiBoneWeight& a, const aiBoneWeight& b) -> bool {
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
                    gbl_warn << "Failed to find submesh_node " << submesh_node_name << std::endl;
                    continue;
                }
                else aSubmesh_node = it->second;

                scene.models[i].node->transform = scene.models[i].node->transform * Convert<glm::mat4>(aSubmesh_node->mTransformation);
            }
            else
            {
                //apply submesh_node transforms to submeshes
                if (scene.models[i].mesh->models.size() > 1)
                {
                    gbl_warn << "submesh_node transforms could not be uniquely applied. Undesirable transforms may occur" << std::endl;
                }

                for (int64_t j = 0; j < scene.models[i].mesh->submeshes.size(); ++j)
                {
                    SubMesh* submesh = scene.models[i].mesh->submeshes[j];
                    aiNode* aSubmesh_node = nullptr;
                    std::string submesh_node_name = get_submesh_node_name(scene.models[i].mesh->ID, submesh->ID);

                    if (auto it = aNode_map.find(submesh_node_name); it == aNode_map.end())
                    {
                        gbl_warn << "Failed to find submesh_node " << submesh_node_name << std::endl;
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
        gbl_err << "Failed to convert assimp scene to scene - " << e.what() << std::endl;
        return false;
    }
    return true;
}