#include "PhyreDAEUnpack.h"
#include "PhyreStructs.h"
#include "PhyreIOUtils.h"
#include <assimp/ParsingUtils.h>
#include <assimp/postprocess.h> 
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/BaseImporter.h>
#include <ranges>

using namespace Assimp;
using namespace Phyre::DAE;
using namespace PhyreIO;

namespace 
{
    DEFINE_REQUIRED_CHUNKS(DAE, PMesh, PMatrix4, PMeshSegment, PDataBlock, PSkinBoneRemap );
}

bool Phyre::DAE::Unpack(const std::string& orig_path, const std::string& out_path)
{
    std::clog << "Importing " << PhyreIO::FileName(orig_path) << std::endl;

    Assimp::Importer importer;
    if (const aiScene* scene = importer.ReadFile(orig_path, aiProcess_FlipUVs | aiProcess_Triangulate))
    {
        FlipNormals((aiScene*)scene);

        //currently in order to export the scene at the correct scale, the fbx exporter is modified directly to set "UnitScaleFactor" to 100
        //not ideal

        std::clog << "Exporting " << PhyreIO::FileName(out_path) << std::endl;

        Assimp::Exporter exporter;
        if (aiReturn ret = exporter.Export(scene, PhyreIO::GetLastExtension(out_path), out_path, scene->mFlags))
        {
            std::cerr << "Export Error: " << exporter.GetErrorString() << std::endl;
            PhyreIO::CancelWrite(out_path);
            return false;
        }
    }
    else
    {
        std::cerr << "Import Error: " << importer.GetErrorString() << std::endl;
        return false;
    }
    return true;
}

void Phyre::DAE::Import(const std::string &pFile, aiScene *pScene)
{
    std::ifstream stream(pFile, std::ios::binary);
    if (!stream)
    {
        throw DeadlyImportError("Could not open ", pFile);
    }
    pScene->mRootNode = new aiNode("Root");
    pScene->mMetaData = new aiMetadata();
    pScene->mMaterials = new aiMaterial *[pScene->mNumMaterials = 2];

    for (uint32_t i = 0; i < pScene->mNumMaterials; ++i)
    {
        std::string n = std::to_string(i + 1);
        aiString materialName = aiString("Material" + n);
        aiString diffuseTexName = aiString("Diffuse" + n);
        aiString normalTexName = aiString("Normal" + n);
        aiString specularTexName = aiString("Specular" + n);

        pScene->mMaterials[i] = new aiMaterial();
        pScene->mMaterials[i]->AddProperty(&diffuseTexName, AI_MATKEY_TEXTURE_DIFFUSE(0));
        pScene->mMaterials[i]->AddProperty(&normalTexName, AI_MATKEY_TEXTURE_NORMALS(0));
        pScene->mMaterials[i]->AddProperty(&specularTexName, AI_MATKEY_TEXTURE_SPECULAR(0));
        pScene->mMaterials[i]->AddProperty(&materialName, AI_MATKEY_NAME);
    }

    std::vector<Phyre::Chunk> chunks;
    int64_t chunks_end = PhyreIO::GetChunks(chunks, stream, RequredDAEChunkNames);
    if (chunks_end < 0)
    {
        throw DeadlyImportError("IO Error: Failed to retrieve all required chunks ", pFile);
    }
    Phyre::Chunk c_node             = chunks[size_t(RequredDAEChunks::PMesh)];
    Phyre::Chunk c_bone_transform   = chunks[size_t(RequredDAEChunks::PMatrix4)];
    Phyre::Chunk c_mesh_meta        = chunks[size_t(RequredDAEChunks::PMeshSegment)];
    Phyre::Chunk c_vert_comp        = chunks[size_t(RequredDAEChunks::PDataBlock)];
    Phyre::Chunk c_bone_ref         = chunks[size_t(RequredDAEChunks::PSkinBoneRemap)];

    int64_t mesh_data_offset = 0;
    seek(stream, 20, std::ios_base::beg);
    mesh_data_offset += read<int32_t>(stream);
    seek(stream, 4, std::ios_base::cur);
    mesh_data_offset += read<int32_t>(stream);
    seek(stream, 4, std::ios_base::cur);
    mesh_data_offset += read<int32_t>(stream);
    seek(stream, 8, std::ios_base::cur);
    mesh_data_offset += int64_t(read<int32_t>(stream)) * 12;
    mesh_data_offset += read<int32_t>(stream);
    seek(stream, 4, std::ios_base::cur);
    mesh_data_offset += int64_t(read<int32_t>(stream)) * 4;
    mesh_data_offset += int64_t(read<int32_t>(stream)) * 16;
    seek(stream, 4, std::ios_base::cur);
    mesh_data_offset += chunks_end;

    int64_t vertex_offset = read<int32_t>(stream);
    int64_t transform_count = 0;
    int64_t bone_node_count = 0;
    int64_t mesh_count = 0;

    seek(stream, c_node.data_offset, std::ios_base::beg);

    for (int64_t i = 0; i < c_node.elem_count; ++i)
    {
        mesh_count += read<int16_t>(stream);
        seek(stream, 8, std::ios_base::cur);
        transform_count += read<uint16_t>(stream);
        seek(stream, 3, std::ios_base::cur);
        bone_node_count += read<uint16_t>(stream);
        seek(stream, 23, std::ios_base::cur);
    }

    int64_t bone_ref_offset = c_bone_ref.data_offset;

    pScene->mMetaData->Add("mesh_data_offset", (int32_t&)mesh_data_offset);
    pScene->mMetaData->Add("bone_ref_offset", (int32_t&)bone_ref_offset);
    pScene->mMetaData->Add("bone_node_count", (int32_t&)bone_node_count);

    if (bone_node_count > 0)
    {
        while (read<int8_t>(stream) != -1);
        seek(stream, -1, std::ios_base::cur);
    }

    std::vector<int32_t> bone_parent_indices(bone_node_count);
    std::vector<int32_t> bone_child_counts(bone_node_count, 0);

    for (int64_t i = 0; i < bone_node_count; ++i)
    {
        int64_t parent_index = bone_parent_indices[i] = read<int32_t>(stream);
        if (parent_index >= 0)
        {
            ++bone_child_counts[parent_index];
        }
    }

    std::vector<aiNode*> bone_nodes;
    bone_nodes.resize(bone_node_count);

    pScene->mRootNode->mNumChildren = 1;
    pScene->mRootNode->mChildren = new aiNode * [1];

    if (bone_node_count > 0)
    {
        seek(stream, c_bone_transform.data_offset + 64 * transform_count, std::ios_base::beg);

        for (int32_t i = 0; i < bone_node_count; ++i)
        {
            int32_t parent_index = bone_parent_indices[i];
            int32_t child_count = bone_child_counts[i];

            aiNode *bone_node = bone_nodes[i] = new aiNode();
            bone_node->mChildren = child_count > 0 ? new aiNode *[child_count] : nullptr;
            bone_node->mTransformation = read<aiMatrix4x4>(stream);
            bone_node->mName = aiString("Bone" + std::to_string(i));

            if (parent_index >= 0)
            {
                aiNode *parent = bone_nodes[parent_index];
                parent->mChildren[parent->mNumChildren] = bone_node;
                parent->mNumChildren++;
                bone_node->mParent = parent;
            }
        }
        pScene->mRootNode->mChildren[0] = bone_nodes[0];
    }
    else
    {
        pScene->mRootNode->mChildren[0] = new aiNode("Bone0");
    }

    aiNode* mesh_root = pScene->mRootNode->mChildren[0];
    mesh_root->mParent = pScene->mRootNode;
    mesh_root->mNumMeshes = (uint32_t)mesh_count;
    mesh_root->mMeshes = new unsigned int[mesh_count];

    pScene->mNumMeshes = (uint32_t)mesh_count + !!(mesh_root->mNumChildren > 1);
    pScene->mMeshes = new aiMesh *[mesh_count + !!(mesh_root->mNumChildren > 1)];

    // exists to fix the bug where the armature modifier isn't applied everywhere if Bone0 has more than one child
    // a proper solution should be investigated
    if (mesh_root->mNumChildren > 1) 
    {
        aiMesh* bone0_mesh = pScene->mMeshes[mesh_count] = new aiMesh();
        pScene->mRootNode->mNumMeshes = 1;
        pScene->mRootNode->mMeshes = new unsigned int[1];
        pScene->mRootNode->mMeshes[0] = mesh_count;
        bone0_mesh->mName = "Mesh" + std::to_string(mesh_count) + "[ignore]";
        bone0_mesh->mNumFaces = 1;
        bone0_mesh->mNumVertices = 1;
        bone0_mesh->mNumUVComponents[0] = 1;
        bone0_mesh->mVertices = new aiVector3D[1]();
        bone0_mesh->mNormals = new aiVector3D[1]();
        bone0_mesh->mTangents = new aiVector3D[1]();
        bone0_mesh->mBitangents = new aiVector3D[1]();
        bone0_mesh->mColors[0] = new aiColor4D[1]();
        bone0_mesh->mTextureCoords[0] = new aiVector3D[1]();
        bone0_mesh->mFaces = new aiFace[1]();
        bone0_mesh->mFaces->mNumIndices = 1;
        bone0_mesh->mFaces->mIndices = new unsigned int[1] { 0 };
        bone0_mesh->mNumBones = mesh_root->mNumChildren - 1;
        bone0_mesh->mBones = new aiBone * [mesh_root->mNumChildren - 1];

        for (uint32_t i = 0; i < bone0_mesh->mNumBones; ++i)
        {
            bone0_mesh->mBones[i] = new aiBone();
            bone0_mesh->mBones[i]->mName = mesh_root->mChildren[i + 1]->mName;
        }
    }

    for (uint32_t i = 0; i < mesh_count; ++i)
    {
        mesh_root->mMeshes[i] = i;
        seek(stream, c_mesh_meta.data_offset + int64_t(i) * 61, std::ios_base::beg);

        Phyre::Mesh &mesh = (Phyre::Mesh&) * (pScene->mMeshes[i] = new Phyre::Mesh());

        mesh.mName = "Mesh" + std::to_string(i);
        mesh.mMaterialIndex = 0;

        mesh.meta.offset = tell(stream);

        seek(stream, 8, std::ios_base::cur);
        mesh.mNumBones = read<int32_t>(stream);
        seek(stream, 17, std::ios_base::cur);
        mesh.meta.component_count = read<int32_t>(stream);
        seek(stream, 9, std::ios_base::cur);
        mesh.mNumFaces = read<int32_t>(stream) / 3;
        seek(stream, 7, std::ios_base::cur);
        mesh.meta.face_offset = read<int32_t>(stream);

        seek(stream, mesh.meta.offset, std::ios_base::beg);
    }

    seek(stream, c_vert_comp.data_offset, std::ios_base::beg);

    for (uint32_t i = 0; i < mesh_count; ++i)
    {
        Phyre::Mesh &mesh = (Phyre::Mesh &)*(pScene->mMeshes[i]);

        for (uint32_t j = 0; j < mesh.meta.component_count; ++j)
        {
            mesh.component(j).offset = tell(stream);
            mesh.component(j).stride = read<int32_t>(stream);
            mesh.component(j).data_count = read<int32_t>(stream);
            seek(stream, 11, std::ios_base::cur);
            mesh.component(j).data_offset = read<int32_t>(stream);
            seek(stream, 4, std::ios_base::cur);
        }
    }

    for (uint32_t i = 0; i < mesh_count; ++i)
    {
        Phyre::Mesh &mesh = (Phyre::Mesh &)*(pScene->mMeshes[i]);

        if (mesh.pos_meta.offset)
        {
            mesh.mNumVertices = (uint32_t)mesh.pos_meta.data_count;
            mesh.mVertices = new aiVector3D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.pos_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                mesh.mVertices[j] = read<aiVector3D>(stream);
            }
        }

        if (mesh.norm_meta.offset)
        {
            mesh.mNormals = new aiVector3D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.norm_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                mesh.mNormals[j] = read<aiVector3D>(stream);
            }
        }

        if (mesh.uv_meta.offset)
        {
            mesh.mNumUVComponents[0] = 2;
            mesh.mTextureCoords[0] = new aiVector3D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.uv_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                aiVector2D uv = read<aiVector2D>(stream);
                mesh.mTextureCoords[0][j] = aiVector3D(uv.x, uv.y, 0.0f);
            }
        }

        if (mesh.tang_meta.offset)
        {
            mesh.mTangents = new aiVector3D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.tang_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                mesh.mTangents[j] = read<aiVector3D>(stream);
            }
        }
        
        if (mesh.bitang_meta.offset)
        {
            mesh.mBitangents = new aiVector3D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.bitang_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                mesh.mBitangents[j] = read<aiVector3D>(stream);
            }
        }

        if (mesh.color_meta.offset)
        {
            mesh.mColors[0] = new aiColor4D[mesh.mNumVertices];
            seek(stream, mesh_data_offset + vertex_offset + mesh.color_meta.data_offset, std::ios_base::beg);
            for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
            {
                mesh.mColors[0][j] = read<aiColor4D>(stream);
            }
        }
        
        mesh.mFaces = new aiFace[mesh.mNumFaces];
        seek(stream, mesh_data_offset + mesh.meta.face_offset, std::ios_base::beg);

        for (uint32_t j = 0; j < mesh.mNumFaces; ++j)
        {
            mesh.mFaces[j].mNumIndices = 3;
            mesh.mFaces[j].mIndices = new uint32_t[3];
            mesh.mFaces[j].mIndices[1] = read<uint16_t>(stream);
            mesh.mFaces[j].mIndices[0] = read<uint16_t>(stream);
            mesh.mFaces[j].mIndices[2] = read<uint16_t>(stream);
        }

        if (mesh.mNumBones)
        {
            std::vector<aiVector4D> bone_weight_arr(mesh.mNumVertices);
            std::vector<aiVector4B> bone_id_arr(mesh.mNumVertices);
            std::vector<std::vector<aiVertexWeight>> aibone_weight_arr(mesh.mNumBones);

            if (mesh.boneID_meta.offset && mesh.weight_meta.offset)
            {
                seek(stream, mesh_data_offset + vertex_offset + mesh.weight_meta.data_offset, std::ios_base::beg);
                for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
                {
                    bone_weight_arr[j] = read<aiVector4D>(stream);
                }

                seek(stream, mesh_data_offset + vertex_offset + mesh.boneID_meta.data_offset, std::ios_base::beg);            
                for (uint32_t j = 0; j < mesh.mNumVertices; ++j)
                {
                    bone_id_arr[j] = read<aiVector4B>(stream);
                    for (uint32_t k = 0; k < 4; ++k)
                    {
                        aibone_weight_arr[bone_id_arr[j][k]].push_back(aiVertexWeight(j, bone_weight_arr[j][k]));
                    }
                }
            }
          
            mesh.mBones = new aiBone *[mesh.mNumBones];

            seek(stream, bone_ref_offset, std::ios_base::beg);
            
            for (uint32_t j = 0; j < mesh.mNumBones; ++j)
            {
                aiBone *bone = mesh.mBones[j] = new aiBone();
                uint16_t boneID = read<uint16_t>(stream);
                seek(stream, 2, std::ios_base::cur);

                bone->mName = "Bone" + std::to_string(boneID);
                
                bone->mNumWeights = (uint32_t)aibone_weight_arr[j].size();
                if (bone->mNumWeights)
                {
                    bone->mWeights = new aiVertexWeight[bone->mNumWeights];
                    memcpy(bone->mWeights, aibone_weight_arr[j].data(), sizeof(aiVertexWeight) * bone->mNumWeights);
                }

                aiNode *bnode = bone_nodes[boneID];
                aiMatrix4x4 mat = bnode->mTransformation;
                while (bnode->mParent)
                {
                    bnode = bnode->mParent;
                    mat = bnode->mTransformation * mat;
                }
                bone->mOffsetMatrix = mat.Inverse();
            }

            bone_ref_offset = tell(stream);
        }
    }
}

void Phyre::DAE::FlipNormals(aiScene* scene)
{
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            if (mesh->HasNormals()) {
                mesh->mNormals[j].x *= -1.0f;
                mesh->mNormals[j].y *= -1.0f;
                mesh->mNormals[j].z *= -1.0f;

            }
            if (mesh->HasTangentsAndBitangents()) {
                mesh->mTangents[j].x *= -1.0f;
                mesh->mTangents[j].y *= -1.0f;
                mesh->mTangents[j].z *= -1.0f;
                mesh->mBitangents[j].x *= -1.0f;
                mesh->mBitangents[j].y *= -1.0f;
                mesh->mBitangents[j].z *= -1.0f;

            }
        }
    }
}
