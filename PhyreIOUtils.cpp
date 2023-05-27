#include "PhyreIOUtils.h"
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace
{
    inline std::string to_lower(const std::string& str) 
    {
        std::string cpy = str;
        for (int i = 0; i < cpy.length(); i++)
        {
            cpy[i] = tolower(cpy[i]);
        }
        return cpy;
    }

    std::vector<std::string> get_abs_segments(const std::string& path)
    {
        std::vector<std::string> segments;
        std::string abs_path = std::filesystem::absolute(path).string();
        std::replace(abs_path.begin(), abs_path.end(), '/', '\\');
      
        segments.reserve(std::count(abs_path.begin(), abs_path.end(), '\\') + 1);
        segments.push_back(std::string());
        segments.back().reserve(abs_path.size());

        for (int i = 0; i < abs_path.size(); ++i)
        {
            if (abs_path[i] == '\\')
            {
                segments.push_back(std::string());
                segments.back().reserve(abs_path.size());
            }
            else
            {
                segments.back().push_back(abs_path[i]);
            }
        }
        return segments;
    }

    std::string strip(const std::string& str)
    {
        std::string::const_iterator b = str.begin(), e = str.end();
        while (b != e && isspace(*b)) {
            ++b;
        }
        while (b != e && isspace(*(e - 1))) {
            --e;
        }
        return std::string(b, e);
    }
}

bool PhyreIO::ReadWholeFile(std::vector<Phyre::byte>& data, const std::string& path)
{
    try
    {
        std::ifstream stream(path, std::ios::in | std::ios::binary);
        seek(stream, 0, std::ios_base::end);
        std::size_t size = tell(stream);
        seek(stream, 0, std::ios_base::beg);
        data.resize(size);
        stream.read((char*)&data[0], size);
        stream.close();
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

std::string PhyreIO::GetLastExtension(const std::string& path)
{
    try
    {
        return path.substr(path.find_last_of(".") + 1);
    }
    catch (const std::exception&)
    {
        return "";
    }
}

std::string PhyreIO::GetExtension(const std::string& path)
{
    try
    {
        size_t parent_end = path.find_last_of("\\/:");
        if (parent_end == std::string::npos) parent_end = 0;
        size_t ext_start = path.find_first_of(".", parent_end + 1);
        if(ext_start == std::string::npos) return "";
        return to_lower(path.substr(ext_start + 1));
    }
    catch (const std::exception&)
    {
        std::cerr << "Failed to retrieve extension on " << path << std::endl;
        return "";
    }
}

bool PhyreIO::IsDirectory(const std::string& path)
{
    return fs::is_directory(path);
}

bool PhyreIO::VerifyParentFolderAccessible(const std::string& path)
{
    try
    {
        size_t parent_end = path.find_last_of("\\/:");
        if (std::string::npos == parent_end)
        {
            std::cerr << "File's parent folder is not accessible \"" << path << "\"" << std::endl;
            return false;
        }
        std::string dir = path.substr(0, parent_end);

        struct _stat64 stat_buffer;
        if (_stat64(dir.c_str(), &stat_buffer) != 0 || (stat_buffer.st_mode & S_IFDIR) == 0)
        {
            std::cerr << "File's parent folder is not accessible \"" << path << "\"" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool PhyreIO::VerifyDifferent(const std::string& in_path, const std::string& out_path)
{
    try
    {
        if (std::filesystem::equivalent(in_path, out_path))
        {
            std::cout << "Input and output paths must not be the same " << std::endl;
            return false;
        }
    }
    catch (std::exception& e) {}
    return true;
}

bool PhyreIO::CopyFolderHierarchy(const std::string& dst, const std::string& src)
{
    try
    {
        fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::directories_only | fs::copy_options::update_existing);
        return true;
    }
    catch (std::exception& e)
    {
        std::cout << "Failed to copy folder hierarchy " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> PhyreIO::Segment(const std::string& file_name)
{
    std::vector<std::string> name_segs;
    name_segs.push_back(std::string());
    name_segs.back().reserve(32);
    for (char c : Normalise(file_name))
    {
        if ((c == '/' || c == '\\') && !name_segs.back().empty())
        {
            name_segs.push_back(std::string());
            name_segs.back().reserve(32);
        }
        else if (c != '\"')
        {
            name_segs.back().push_back(c);
        }
    }
    if (name_segs.back().empty())
    {
        return {};
    }
    return name_segs;
}

std::string PhyreIO::Join(const std::vector<std::string>& path)
{
    std::string path_str;
    if (path.size() == 1)
    {
        return path[0] + (path[0].contains(':') ? "//" : "");
    }
    if (!path.empty())
    {
        path_str.reserve(path.size() - 1 + path.size() * 16);
        path_str += path[0] + (path.size() == 1 ? "/" : "");
        for (int i = 1; i < path.size(); ++i)
        {
            (path_str += "/") += path[i];
        }
    }
    return path_str;
}

bool PhyreIO::VerifyFileAccessible(const std::string& path)
{
    struct _stat64 stat_buffer;
    if (path == "." || _stat64(path.c_str(), &stat_buffer) != 0)
    {
        std::cerr << "File is not accessible \"" << path << "\"" << std::endl;
        return false;
    }
    return true;
}

void PhyreIO::CancelWrite(const std::string& path, std::ofstream* stream)
{
    try 
    {
        if(stream) stream->close();
        if(path.size()) std::filesystem::remove(path);
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR - failed to delete incomplete file: " << e.what() << " " << ec.message() << std::endl;
    }
}

bool PhyreIO::VerifyPhyreHeader(const std::string& phyre_path)
{
    try {
        std::ifstream file(phyre_path);
        std::string header;
        header.resize(5, '_');
        file.read((char*)header.data(), 5);
        if (header != "RYHPT")
        {
            std::cerr << "IO ERROR: Incompatible file structure detected" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

std::string PhyreIO::FileName(const std::string& path)
{
    size_t last_parent = path.find_last_of("/\\");
    return last_parent == std::string::npos ? path : path.substr(path.find_last_of("/\\") + 1);
}

std::string PhyreIO::TruePathCase(const std::string& path)
{
    try 
    {
        std::vector<std::string> segments = get_abs_segments(path);
        if (segments.empty())
        {
            return path;
        }

        std::string true_path = segments[0];
        true_path[0] = std::toupper(true_path[0]);

        for (int i = 1; i < segments.size(); ++i)
        {
            std::string extended_path = true_path + "\\" + segments[i];

            WIN32_FIND_DATA fData = {};
            HANDLE hFind = FindFirstFileA(extended_path.c_str(), &fData);
            if (hFind == INVALID_HANDLE_VALUE)
            {
                for (int j = i; j < segments.size(); ++j)
                {
                    true_path += "\\" + segments[j];
                    return true_path;
                }
            }
            FindClose(hFind);
            true_path += "\\" + std::string(fData.cFileName);
        }
        return true_path;
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR: Failed to retrive path's true case - " << e.what() << " " << ec.message() << std::endl;
        return path;
    }
    return path;
}

std::string PhyreIO::BranchFromPath(const std::string& deviating_path, const std::string& base_path)
{
    std::string branch;
    std::vector<std::string> base_segs = get_abs_segments(TruePathCase(base_path));
    std::vector<std::string> devi_segs = get_abs_segments(TruePathCase(deviating_path));

    size_t i = 0;
    for(; i < std::min(base_segs.size(), devi_segs.size()); ++i)
    {
        if (base_segs[i] != devi_segs[i]) break;
    }
    for (; i < devi_segs.size(); ++i)
    {
        branch += "\\" + devi_segs[i];
    }
    return branch;
}

std::string PhyreIO::Normalise(const std::string& path)
{
    auto b = path.begin(), e = path.begin() + strlen(path.c_str());
    while (b != e && (isspace(*b) || *b == '\\' || *b == '/' || *b == '\"')) {
        ++b;
    }
    while (b != e && (isspace(*(e - 1)) || *(e - 1) == '\\' || *(e - 1) == '/' || *(e - 1) == '\"')) {
        --e;
    }
    if (b != e) 
    {
        std::string npath;
        npath.reserve(path.size());

        for (auto i = b; i + 1 != e; ++i)
        {
            if ((*i == '\\' || *i == '/'))
            {
                if ((*(i + 1) == '\\' || *(i + 1) == '/'))
                {
                    continue;
                }
                npath.push_back('/');
                continue;
            }
            npath.push_back(*i);
        }
        npath.push_back(*(e - 1));

        if (*(e - 1) == ':')
        {
            npath.push_back('/');
        }
        return npath;
    }

    return std::string();
}

std::string PhyreIO::TmpPath()
{
    try
    {
        std::string temp_path = "tmp";

        if (!CreateDirectoryA("tmp", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            temp_path.resize(512);
            GetTempPathA(temp_path.size(), temp_path.data());

            struct _stat64 stat_buffer;
            if (_stat64(temp_path.c_str(), &stat_buffer) == 0)
            {
                return std::filesystem::absolute(std::string(temp_path.begin(), temp_path.end() - 1)).string();
            }
            MessageBoxA(NULL, std::string("IO ERROR: Failed to retrieve/create temp path").c_str(), "CRITICAL FAILURE", MB_ICONERROR | MB_OK);
            exit(-1);
        }
        else
        {
            return std::filesystem::absolute("tmp").string();
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::string msg = std::string("IO ERROR: Failed to retrieve temp path - ") + e.what() + " " + ec.message();
        MessageBoxA(NULL, msg.c_str(), "CRITICAL FAILURE", MB_ICONERROR | MB_OK);
        exit(-1);
    }
    return "";
}

std::string PhyreIO::CreateTmpPath(const std::string& folder_name)
{
    std::string path = TmpPath() + "/" + folder_name;
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove_all(path);
    }
    if (!CreateDirectoryA(path.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
    {
        std::cerr << "failed to create temp directory " << path << std::endl;
        return "";
    }
    return path;
}

int64_t PhyreIO::GetChunks(std::vector<Phyre::Chunk>& chunks, std::istream& stream, const std::vector<std::string>& filters)
{
    try 
    {
        std::streampos prev_pos = tell(stream);

        seek(stream, 4, std::ios::beg);
        int64_t name_meta_offset = read<int32_t>(stream);
        int64_t chunk_offset = read<int32_t>(stream);
        seek(stream, 4, std::ios::cur);
        int64_t chunk_count = read<int32_t>(stream);
        seek(stream, name_meta_offset + 8, std::ios_base::beg);
        int64_t data_offset1 = read<int32_t>(stream);
        int64_t name_count = read<int32_t>(stream);
        int64_t prelim_data_count = read<int32_t>(stream);

        seek(stream, data_offset1 * 4 + 12, std::ios::cur);

        int64_t name_offsets_offset = tell(stream);
        std::vector<int64_t> name_offsets = std::vector<int64_t>(name_count);

        for (int64_t i = 0; i < name_count; ++i)
        {
            seek(stream, name_offsets_offset + i * 36 + 8, std::ios_base::beg);
            name_offsets[i] = read<int32_t>(stream);
        }

        seek(stream, name_offsets_offset + 36 * name_count + prelim_data_count * 24, std::ios_base::beg);

        int64_t names_base = (int32_t)tell(stream);

        std::vector<std::string> names = std::vector<std::string>(name_count);
        for (int64_t i = 0; i < name_count; i++)
        {
            seek(stream, names_base + name_offsets[i], std::ios_base::beg);
            names[i] = read<std::string>(stream);
        }

        seek(stream, name_meta_offset + chunk_offset, std::ios_base::beg);
        int64_t chunk_offset_sum = tell(stream) + 36 * chunk_count;
        size_t filtered_chunk_count = 0;

        chunks.resize(filters.size());
        for (int32_t i = 0; i < chunk_count; ++i)
        {
            Phyre::Chunk chunk =
            {
                .offset = tell(stream),
                .name = names[read<int32_t>(stream) - 1],
                .elem_count = read<int32_t>(stream),
                .data_offset = chunk_offset_sum
            };
            chunk_offset_sum += read<int32_t>(stream);

            if (filters.empty())
            {
                chunks.push_back(chunk);
            }
            else 
            {
                for (int32_t j = 0; j < filters.size(); ++j) 
                {
                    if (chunk.name == filters[j]) 
                    {
                        filtered_chunk_count += chunks[j].name.empty();
                        chunks[j] = chunk;
                    }
                }
            }
            seek(stream, 24, std::ios_base::cur);
        }

        if (chunks.empty() && !filters.empty() || filtered_chunk_count != filters.size()) 
        {
            return -1;
        }
        seek(stream, prev_pos, std::ios_base::beg);

        return chunk_offset_sum;
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        std::cerr << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return -1;
    }
    return -1;
}

int32_t PhyreIO::GetBoneID(const std::string& bone_name)
{
    try
    {
        return std::stoi(bone_name.substr(4));
    }
    catch (std::exception&)
    {
        throw std::runtime_error("Error: Bone ID could not be aquired from name " + bone_name + "");
    }
    return 0;
}

int32_t PhyreIO::GetMeshID(const std::string& mesh_name)
{
    try
    {
        size_t name_pos = mesh_name.find("Mesh");
        if (name_pos < 0)
        {
            return -1;
        }
        auto start_pos = mesh_name.begin() + name_pos + 4;
        auto end_pos = std::find_if(mesh_name.begin() + name_pos + 4, mesh_name.end(), [](const char& c) { return !isdigit(c); });
        std::string str_id = std::string(start_pos, end_pos);
        return std::stoi(str_id);
    }
    catch (std::exception&)
    {
        return -1;
    }
    return -1;
}

DXGI_FORMAT PhyreIO::GetDXGIFormat(std::string format_name)
{
    if (format_name == "BC5")        return DXGI_FORMAT_BC5_UNORM;
    else if (format_name == "DXT5")  return DXGI_FORMAT_BC3_UNORM;
    else if (format_name == "ARGB8") return DXGI_FORMAT_B8G8R8A8_UNORM;
    else if (format_name == "A8")    return DXGI_FORMAT_A8_UNORM;
    else
    {
        std::cerr << "Unrecognised dds format " << format_name << std::endl;
        return DXGI_FORMAT_UNKNOWN;
    }
}

DirectX::WICCodecs PhyreIO::GetCodecID(const std::string& path)
{
    std::string ext = GetExtension(path);

    if (ext == "bmp")                  return DirectX::WIC_CODEC_BMP;
    if (ext == "jpg" || ext == "jpeg") return DirectX::WIC_CODEC_JPEG;
    if (ext == "png")                  return DirectX::WIC_CODEC_PNG;
    if (ext == "tiff")                 return DirectX::WIC_CODEC_TIFF;
    if (ext == "wmp")                  return DirectX::WIC_CODEC_WMP;

    return (DirectX::WICCodecs)0;
}