#include "PhyreDAEPack.h"
#include "PhyreIOUtils.h"
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h> 
#include <iostream>
#include <list>

using namespace Assimp;
using namespace PhyreIO;
using namespace Phyre;

namespace 
{
    void assign_dummy_data(aiMesh& mesh, int32_t index) 
    {
        mesh.mName = aiString("DummyMesh" + std::to_string(index));
        mesh.mNumFaces = 1;
        mesh.mNumVertices = 3;
        mesh.mNumUVComponents[0] = 1;
        mesh.mVertices = new aiVector3D[3]();
        mesh.mNormals = new aiVector3D[3]();
        mesh.mTangents = new aiVector3D[3]();
        mesh.mBitangents = new aiVector3D[3]();
        mesh.mColors[0] = new aiColor4D[3]();
        mesh.mTextureCoords[0] = new aiVector3D[3]();
        mesh.mFaces = new aiFace[1]();
        mesh.mFaces->mNumIndices = 3;
        mesh.mFaces->mIndices = new unsigned int[3] { 0, 1, 2 };
    }
}

bool Phyre::DAE::Pack(const std::string& original_path, const std::string& replacement_path, const std::string& output_path)
{
    std::clog << "Importing " << PhyreIO::FileName(replacement_path) << std::endl;

    uint32_t import_flags = BaseImporter::SimpleExtensionCheck(replacement_path, "dae.phyre") ? 0 : aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate;

    Assimp::Importer replacement_importer;
    const aiScene* replacement_scene = replacement_importer.ReadFile(replacement_path, import_flags);
    if (!replacement_scene) 
    {
        std::cerr << "Failed to import replacement scene " << output_path << " " << (replacement_importer.GetErrorString()) << std::endl;
        return false;
    }

    FlipNormals((aiScene*)replacement_scene);

    std::clog << "Importing " << PhyreIO::FileName(original_path) << std::endl;

    Assimp::Importer original_importer;
    const aiScene* original_scene = original_importer.ReadFile(original_path, 0);
    if (!original_scene) 
    {
        std::cerr << "Failed to import original scene " << output_path << " " << (original_importer.GetErrorString()) << std::endl;
        return false;
    }

    std::clog << "Exporting " << PhyreIO::FileName(output_path) << std::endl;

    if (!Export((aiScene*)original_scene, replacement_scene, original_path, output_path))
    {
        PhyreIO::CancelWrite(output_path);
        return false;
    }
    return true;
}

bool Phyre::DAE::Export(aiScene* orig_scene, const aiScene* ref_scene, const std::string& original_path, const std::string& output_path)
{
    std::list<aiMesh> dummy_meshes;
    try 
    {
        int64_t orig_mesh_data_offset = 0;
        int64_t orig_bone_ref_offset = 0;
        int64_t orig_bone_node_count = 0;
        orig_scene->mMetaData->Get("mesh_data_offset", (int32_t&)orig_mesh_data_offset);
        orig_scene->mMetaData->Get("bone_ref_offset", (int32_t&)orig_bone_ref_offset);
        orig_scene->mMetaData->Get("bone_node_count", (int32_t&)orig_bone_node_count);

        std::ofstream ostream(output_path, std::ios::binary);
        if (!ostream.good())
        {
            std::cerr << "Failed to open file " << output_path;
            return false;
        }
        std::ifstream istream(original_path, std::ios::binary);
        if (!istream.good())
        {
            std::cerr << "Failed to open file " << original_path;
            return false;
        }
        std::vector<Phyre::byte> orig_data;

        orig_data.resize(orig_mesh_data_offset);
        read(istream, orig_data.data(), orig_mesh_data_offset);
        istream.close();

        int64_t mesh_count = orig_scene->mRootNode->mChildren[0]->mNumMeshes;

        std::vector<aiMesh*> ref_meshes(mesh_count, nullptr);
        for (int64_t i = 0; i < mesh_count; ++i)
        {
            aiString mesh_name = aiString("Mesh" + std::to_string(i));
            aiMesh** mesh_end = ref_scene->mMeshes + ref_scene->mNumMeshes;
            aiMesh** mesh = std::find_if(ref_scene->mMeshes, mesh_end, [&mesh_name](const aiMesh* m) { return m->mName == mesh_name; });
            if (mesh != mesh_end)
            {
                ref_meshes[i] = *mesh;
                continue;
            }                
            aiNode* mesh_parent = ref_scene->mRootNode->FindNode(("Mesh" + std::to_string(i)).c_str());
            if (mesh_parent && mesh_parent->mNumMeshes > 0) 
            {
                ref_meshes[i] = ref_scene->mMeshes[mesh_parent->mMeshes[0]];
                continue;
            }        
        }

        for (uint32_t i = 0; i < mesh_count; ++i) 
        {
            aiMesh*& rmesh = ref_meshes[i];
            if (rmesh == nullptr || rmesh->mNumVertices <= 2 || rmesh->mNumFaces <= 0)
            {
                dummy_meshes.push_back(aiMesh());
                assign_dummy_data(dummy_meshes.back(), i);
                rmesh = &dummy_meshes.back();
            }
        }

        int16_t bone_ref_map[256];
        int16_t max_bone_ref = 0;
        memset(bone_ref_map, -1, sizeof(bone_ref_map));

        for (int64_t i = 0, data_offset = orig_bone_ref_offset; i < mesh_count; ++i) 
        {
            aiMesh& omesh = *orig_scene->mMeshes[i];
            for (int64_t j = 0; j < omesh.mNumBones; ++j) 
            {
                int16_t x = (*(int16_t*)&orig_data[data_offset + j * 4]);
                int16_t y = (*(int16_t*)&orig_data[data_offset + j * 4 + 2]);
                bone_ref_map[x] = y;
                max_bone_ref = std::max(max_bone_ref, x);
            }
            data_offset += (int64_t)omesh.mNumBones * 4;
        }

        std::vector<std::vector<int32_t>> bone_weight_map(mesh_count);

        uint64_t data_offset = orig_bone_ref_offset;
        for (uint32_t i = 0; i < mesh_count; ++i) 
        {
            aiMesh& omesh = *orig_scene->mMeshes[i];
            aiMesh& rmesh = *ref_meshes[i];

            bone_weight_map[i].resize(rmesh.mNumBones);

            std::vector<int16_t> unused_bone_ref_map(bone_ref_map, bone_ref_map + max_bone_ref + 1);

            uint32_t valid_bone_count = 0;
            for (uint32_t j = 0; j < rmesh.mNumBones && valid_bone_count < omesh.mNumBones; ++j) 
            {
                const aiBone& bone = *rmesh.mBones[j];
                int16_t bone_id = (int16_t)GetBoneID(bone.mName.C_Str());

                if (bone_id > 0 && bone.mNumWeights > 0) 
                {
                    if (unused_bone_ref_map[bone_id] == -1) 
                    {
                        std::cerr << "Failed to find bone mapping for bone " << bone_id << " on mesh " << i  << " (possible duplicate replacement?)" << std::endl;
                        return false;
                    }
                    bone_weight_map[i][j] = valid_bone_count;
                    (*(int16_t*)&orig_data[data_offset + (int64_t)valid_bone_count * 4]) = (int16_t)bone_id;
                    (*(int16_t*)&orig_data[data_offset + (int64_t)valid_bone_count * 4 + 2]) = unused_bone_ref_map[bone_id];
                    unused_bone_ref_map[bone_id] = -1;
                    valid_bone_count++;
                }
                else 
                {
                    bone_weight_map[i][j] = -1;
                }
            }
            for (uint32_t j = valid_bone_count; j < omesh.mNumBones; ++j) 
            {
                int16_t bone_id = -1;
                for (uint16_t k = 0; k < (uint16_t)unused_bone_ref_map.size(); ++k) 
                {
                    if (unused_bone_ref_map[k] != -1) 
                    {
                        bone_id = k;
                        break;
                    }
                }
                if (bone_id == -1) 
                {
                    std::cerr << "Failed to find fill unused bone for mesh " << i << std::endl;
                    return false;
                }
                (*(int16_t*)&orig_data[data_offset + (int64_t)j * 4]) = (int16_t)bone_id;
                (*(int16_t*)&orig_data[data_offset + (int64_t)j * 4 + 2]) = (int16_t)unused_bone_ref_map[bone_id];
                unused_bone_ref_map[bone_id] = -1;
            }
            data_offset += 4 * (int64_t)omesh.mNumBones;
        }

        seek(ostream, 0, std::ios_base::beg);
        write(ostream, orig_data.data(), orig_mesh_data_offset);

        for (uint32_t i = 0; i < mesh_count; ++i)
        {
            Phyre::Mesh& omesh = (Phyre::Mesh&)*orig_scene->mMeshes[i];
            aiMesh& rmesh = *ref_meshes[i];

            omesh.meta.face_offset = (int32_t)tell(ostream) - orig_mesh_data_offset;

            uint16_t max_vertex_index = 0;

            for (uint32_t j = 0; j < rmesh.mNumFaces; ++j)
            {
                aiFace& face = rmesh.mFaces[j];

                if (face.mNumIndices != 3) 
                {
                    std::cerr << "Non-triangle faces detected" << std::endl;
                    write(ostream, "\0\0\0\0\0", 6);
                    continue;
                }
    
                write<uint16_t>(ostream, face.mIndices[1]);
                write<uint16_t>(ostream, face.mIndices[0]);
                write<uint16_t>(ostream, face.mIndices[2]);
                max_vertex_index = std::max(max_vertex_index, (uint16_t)std::max(face.mIndices[0], std::max(face.mIndices[1], face.mIndices[2])));
            }

            if (rmesh.mNumFaces & 1)
            {
                write<uint16_t>(ostream, 0);
            }

            seek(ostream, omesh.meta.offset + 38, std::ios_base::beg);

            write<int32_t>(ostream, max_vertex_index);
            write<int32_t>(ostream, rmesh.mNumFaces * 3);

            seek(ostream, 7, std::ios_base::cur);

            write<int32_t>(ostream, omesh.meta.face_offset);
            write<int32_t>(ostream, 2 * rmesh.mNumFaces * 3);

            seek(ostream, 0, std::ios_base::end);
        }

        int64_t mesh_metadata_end = tell(ostream);
        int64_t vertex_offset = mesh_metadata_end - orig_mesh_data_offset;

        seek(ostream, 72, std::ios_base::beg);
        write<int32_t>(ostream, vertex_offset);

        seek(ostream, 0, std::ios_base::end);

        for (uint32_t i = 0; i < mesh_count; ++i)
        {
            Phyre::Mesh& omesh = (Phyre::Mesh&)*orig_scene->mMeshes[i];
            aiMesh& rmesh = *ref_meshes[i];

            int64_t mesh_data_curpos = tell(ostream);

            seek(ostream, omesh.pos_meta.offset + 4, std::ios_base::beg);
            write<int32_t>(ostream, rmesh.mNumVertices);

            seek(ostream, 11, std::ios_base::cur);
            write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
            write<int32_t>(ostream, 12 * rmesh.mNumVertices);

            seek(ostream, 0, std::ios_base::end);

            for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
            {
                write<aiVector3D>(ostream, rmesh.mVertices[j]);
            }

            if (omesh.HasNormals())
            {
                if (rmesh.HasNormals() == false) 
                {
                    std::cerr << "Per-vertex normals are missing" << std::endl;
                    return false;
                }

                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.norm_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 12 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                {
                    write<aiVector3D>(ostream, rmesh.mNormals[j]);
                }
            }

            if (omesh.HasTextureCoords(0)) 
            {
                if (rmesh.HasTextureCoords(0) == false) 
                {
                    std::cerr << "Per-vertex uvs are missing" << std::endl;
                    return false;
                }
                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.uv_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 8 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                {     
                    write<aiVector2D>(ostream, aiVector2D(rmesh.mTextureCoords[0][j][0], rmesh.mTextureCoords[0][j][1]));
                }
            }

            if (omesh.HasTangentsAndBitangents()) 
            {
                if (rmesh.HasTangentsAndBitangents() == false) 
                {
                    std::cerr << "Per-vertex tangents and bitangents are missing" << std::endl;
                    return false;
                }
                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.tang_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 12 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                {
                    write<aiVector3D>(ostream, rmesh.mTangents[j]);
                }

                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.bitang_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 12 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                {
                    write<aiVector3D>(ostream, rmesh.mBitangents[j]);
                }
            }

            if (omesh.HasVertexColors(0))
            {
                mesh_data_curpos = tell(ostream);

                seek(ostream, omesh.color_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 16 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                if (rmesh.HasVertexColors(0)) 
                {
                    for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                    {
                        write<aiColor4D>(ostream, rmesh.mColors[0][j]);
                    }
                }
                else 
                {
                    for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                    {
                        write<aiColor4D>(ostream, aiColor4D(0.0f, 0.0f, 0.0f, 1.0f));
                    }
                }
            }

            if (omesh.HasBones())
            {
                std::vector<std::vector<BoneWeight>> boneWeights(rmesh.mNumVertices);

                for (uint32_t j = 0; j < rmesh.mNumBones; ++j)
                {
                    const aiBone& bone = *rmesh.mBones[j];
                    int32_t boneID = bone_weight_map[i][j];

                    if (boneID < 0) continue;

                    for (uint32_t k = 0; k < bone.mNumWeights; ++k)
                    {
                        int32_t vertexID = bone.mWeights[k].mVertexId;

                        boneWeights[vertexID].push_back(BoneWeight());
                        BoneWeight& boneWeight = boneWeights[vertexID].back();
                        boneWeight.boneID = boneID;
                        boneWeight.weight = bone.mWeights[k].mWeight;
                    }
                }
                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j) 
                {
                    std::sort(boneWeights[j].begin(), boneWeights[j].end(),
                    [](const BoneWeight& a, const BoneWeight& b) -> bool {
                        return a.weight > b.weight;
                    });
                    //not sure what happened to make the following necessary
                    std::set<int8_t> used_ids;
                    boneWeights[j].erase(std::remove_if(boneWeights[j].begin(), boneWeights[j].end(), [&](const BoneWeight& bw) { return !used_ids.insert(bw.boneID).second; }), boneWeights[j].end());
                    boneWeights[j].resize(4);
                    { // satisfy requirement for weights to be of only 3 significant figures
                        float sum = 0.0f;
                        for (auto& b : boneWeights[j]) 
                        {
                            sum += b.weight;
                        }
                        if (sum > 0.0001f)
                        {
                            float sum_inv = 1.0f / sum;
                            float sum2 = 0.0f;
                            for (auto& b : boneWeights[j])
                            {
                                b.weight = std::round(b.weight * sum_inv * 100) / 100;
                                sum2 += b.weight;
                            }
                            float rem = 1.0f - sum2;

                            for (auto& b : boneWeights[j])
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

                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.weight_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 16 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j) 
                {
                    boneWeights[j].resize(4);
                    write<aiVector4D>(ostream, aiVector4D{
                        boneWeights[j][0].weight,
                        boneWeights[j][1].weight,
                        boneWeights[j][2].weight,
                        boneWeights[j][3].weight 
                    });
                }

                mesh_data_curpos = tell(ostream);
                seek(ostream, omesh.boneID_meta.offset + 4, std::ios_base::beg);
                write<int32_t>(ostream, rmesh.mNumVertices);

                seek(ostream, 11, std::ios_base::cur);
                write<int32_t>(ostream, mesh_data_curpos - mesh_metadata_end);
                write<int32_t>(ostream, 4 * rmesh.mNumVertices);

                seek(ostream, 0, std::ios_base::end);

                for (uint32_t j = 0; j < rmesh.mNumVertices; ++j)
                {
                    boneWeights[j].resize(4);
                    write<aiVector4B>(ostream, aiVector4B{
                        boneWeights[j][0].boneID,
                        boneWeights[j][1].boneID,
                        boneWeights[j][2].boneID,
                        boneWeights[j][3].boneID 
                    });
                }
            }
        }

        int64_t mesh_data_end = tell(ostream);

        seek(ostream, 76, std::ios_base::beg);
        write<int32_t>(ostream, mesh_data_end - mesh_metadata_end);
    }
    catch (std::exception& e) 
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }

    return true;
}