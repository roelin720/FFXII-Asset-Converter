#pragma once
#include "FileStructs.h"
#include <string>
#include <vector>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include "glm/glm.hpp"
#include <assert.h>
#include <set>

struct CriticalFailure { std::string message; };

#ifndef NDEBUG 
#define CONVASSERT(x) assert(x)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define CONVASSERT(x) {if(!(x)) throw ::CriticalFailure(std::string("ASSERT FAILURE ") +  __FILENAME__ + (" line " STRINGIZE(__LINE__) ": " #x));}
#endif

namespace IO
{
    std::string Strip(std::string string);
    std::string ToUpper(std::string string);
    std::string ToLower(std::string string);
    std::string ReplaceFirst(std::string string, const std::string& src, const std::string& dst);
    std::string ReplaceLast(std::string string, const std::string& src, const std::string& dst);

    bool ReadWholeFile(std::vector<char> &data, const std::string &path);
    std::string GetLastExtension(const std::string& phyre_path);
    std::string GetExtension(const std::string& phyre_path);
    bool IsDirectory(const std::string& path);
    bool VerifyHeader(const std::string &phyre_path);
    bool VerifyFileAccessible(const std::string& path);
    bool VerifyParentFolderAccessible(const std::string& path);
    bool VerifyDifferent(const std::string& in_path, const std::string& out_path);
    bool CopyFolderHierarchy(const std::string& dst, const std::string& src);

    std::vector<std::string> Split(const std::string& string, const std::string& delim);
    std::vector<std::string> Segment(const std::string& file_name);
    std::string Join(const std::vector<std::string>& file_name);

    std::string FileName(const std::string& path);
    std::string BaseStem(const std::string& path);
    std::string TruePathCase(const std::string& path);
    std::string BranchFromPath(const std::string& deviating_path, const std::string& base_path);
    std::string ParentPath(const std::string& path);
    std::string BasePath(const std::string& path1, const std::string& base_path);
    std::string Normalise(const std::string& path);

    std::string TmpPath();
    std::string CreateTmpPath(const std::string& folder_name);

    void CancelWrite(const std::string& path, std::ofstream* stream = nullptr);

    bool ReadReferences(std::map<std::string, std::string>& ref_name_to_id_map, const std::string& ref_list_path);
    bool FindLocalReferences(std::map<std::string, std::string>& ref_name_to_id_map, const std::string& phyre_path);
    bool EvaluateReferencesFromLocalSearch(std::map<std::string, std::string>& ref_name_to_file_map, const std::map<std::string, std::string>& ref_name_to_id_map, const std::string& phyre_path, const std::string& extension);

    //stream writing functions:

    template <typename T, typename T2>
    inline void write(std::ostream& stream, const T2& data)
    {
        stream.write((const char*)&data, sizeof(T));
    }
    inline void write(std::ostream& stream, const void* dst, size_t len)
    {
        stream.write((char*)dst, len);
    }
    
    template<typename T, typename T2>
    inline void var_write(std::ostream& stream, T2 data)
    {
        typename std::make_unsigned<T>::type v = data;
        do write<uint8_t>(stream, v & 127 | (!!(v >> 7) << 7)); while (v >>= 7);
    }

    inline void write_3x4(std::ostream& stream, const glm::mat4& m)
    {
        const float* mf = (float*)&m;
        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 3; ++j) {
                write<float>(stream, mf[(i + 1) * 4 + j]);
            }
            write<float>(stream, mf[i]);
        }
    }

    template <typename T>
    inline T read(std::istream& stream) {
        T val = T();
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
        for (unsigned i = 0; i < 4; ++i) {
            for (unsigned j = 0; j < 4; ++j) {
                m[j][i] = read<ai_real>(stream);
            }
        }
        return m;
    }

    inline void read(std::istream& stream, const void* dst, size_t len)
    {
        stream.read((char*)dst, len);
    }

    template<typename T>
    inline T var_read(std::istream& stream)
    {
        using U = typename std::make_unsigned<T>::type;
        U v = 0, i = 0;
        for (uint8_t r = 128; r & 128; ++i)
        {
            r = read<uint8_t>(stream);
            v |= U(r & 127) << 7 * i;
        }
        return T(v);
    }

    inline glm::mat4 read_3x4(std::istream& stream)
    {
        glm::mat4 m{};
        float* mf = (float*)&m;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                mf[(i + 1) * 4 + j] = read<float>(stream);
            }
            mf[i] = read<float>(stream);
        }
        mf[15] = 1.0f;
        return m;
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

//math printing functions

#define fmtf(f) std::fixed << std::setprecision(4) << std::showpoint << f

inline std::ostream& operator<<(std::ostream& out, const glm::vec2& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]);
}

inline std::ostream& operator<<(std::ostream& out, const glm::vec3& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]);
}

inline std::ostream& operator<<(std::ostream& out, const glm::vec4& val)
{
    return out << fmtf(val[0]) << "\t" << fmtf(val[1]) << "\t" << fmtf(val[2]) << "\t" << fmtf(val[3]);
}

inline std::ostream& operator<<(std::ostream& out, const glm::u8vec4& val)
{
    return out << (int)val[0] << "\t" << (int)val[1] << "\t" << (int)val[2] << "\t" << (int)val[3];
}

inline std::ostream& operator<<(std::ostream& out, const glm::mat4& val)
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

#undef fmtf