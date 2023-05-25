#pragma once
#include "PhyreStructs.h"
#include <string>
#include <vector>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <dxgiformat.h>
#include "DirectXTex.h"

namespace Phyre 
{
    struct CriticalFailure { std::string message; };
    #define STRINGIZE_DETAIL(x) #x
    #define STRINGIZE(x) STRINGIZE_DETAIL(x)
    #define PHYASSERT(x) {if(!(x)) throw CriticalFailure{"CRITICAL FAILURE " __FILE__ " line " STRINGIZE(__LINE__) ": " #x};}
}

namespace PhyreIO
{
    bool ReadWholeFile(std::vector<Phyre::byte> &data, const std::string &path);
    std::string GetLastExtension(const std::string& phyre_path);
    std::string GetExtension(const std::string& phyre_path);
    bool IsDirectory(const std::string& path);
    bool VerifyPhyreHeader(const std::string &phyre_path);
    bool VerifyFileAccessible(const std::string& path);
    bool VerifyParentFolderAccessible(const std::string& path);
    bool VerifyDifferent(const std::string& in_path, const std::string& out_path);
    bool CopyFolderHierarchy(const std::string& dst, const std::string& src);

    std::vector<std::string> Segment(const std::string& file_name);
    std::string Join(const std::vector<std::string>& file_name);

    std::string FileName(const std::string& path);
    std::string TruePathCase(const std::string& path);
    std::string BranchFromPath(const std::string& deviating_path, const std::string& base_path);
    std::string Normalise(const std::string& path);

    std::string TmpPath();
    std::string CreateTmpPath(const std::string& folder_name);

    void CancelWrite(const std::string& path, std::ofstream* stream = nullptr);

    int64_t GetChunks(std::vector<Phyre::Chunk>& chunks, std::istream& stream, const std::vector<std::string>& filters = {});
    int32_t GetBoneID(const std::string& bone_name);
    int32_t GetMeshID(const std::string& mesh_name);

    DXGI_FORMAT GetDXGIFormat(std::string format_name);
    DirectX::WICCodecs GetCodecID(const std::string& path);

    template <typename T, typename T2>
    inline void write(std::ostream& stream, const T2& data)
    {
        stream.write((const char*)&data, sizeof(T));
    }
    inline void write(std::ostream& stream, const void* dst, size_t len)
    {
        stream.write((char*)dst, len);
    }

    template <typename T>
    inline T read(std::istream& stream) {
        T val;
        stream.read((char*)&val, sizeof(T));
        return val;
    }
    template <>
    inline std::string read<std::string>(std::istream& stream)
    {
        std::string tmp;
        tmp.reserve(64);
        for (char c; c = read<char>(stream); tmp += c);
        return tmp;
    }
    template <>
    inline aiMatrix4x4 read<aiMatrix4x4>(std::istream& stream) {
        aiMatrix4x4 m;
        for (unsigned int i = 0; i < 4; ++i) {
            for (unsigned int j = 0; j < 4; ++j) {
                m[j][i] = read<ai_real>(stream);
            }
        }
        return m;
    }
    inline void read(std::istream& stream, const void* dst, size_t len)
    {
        stream.read((char*)dst, len);
    }

    inline void seek(std::ostream& stream, std::streampos offset, std::ios_base::seekdir dir)
    {
        stream.seekp(offset, dir);
    }
    inline void seek(std::istream& stream, std::streampos offset, std::ios_base::seekdir dir)
    {
        stream.seekg(offset, dir);
    }
    inline void seekp(std::ostream& stream, std::streampos offset, std::ios_base::seekdir dir)
    {
        stream.seekp(offset, dir);
    }
    inline void seekg(std::istream& stream, std::streampos offset, std::ios_base::seekdir dir)
    {
        stream.seekg(offset, dir);
    }

    inline int64_t tell(std::ostream& stream)
    {
        return stream.tellp();
    }
    inline int64_t tell(std::istream& stream)
    {
        return stream.tellg();
    }
} 
