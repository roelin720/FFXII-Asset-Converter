#include "FileIOUtils.h"
#include "ConverterInterface.h"
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace
{
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
}

std::string IO::Strip(std::string string)
{
    std::string::const_iterator b = string.begin(), e = string.end();
    while (b != e && isspace(*b)) ++b;
    while (b != e && isspace(*(e - 1))) --e;
    return std::string(b, e);
}

std::string IO::ToUpper(std::string string)
{
    for (int i = 0; i < string.length(); i++)
    {
        string[i] = std::toupper(string[i]);
    }
    return string;
}

std::string IO::ToLower(std::string string)
{
    for (int i = 0; i < string.length(); i++)
    {
        string[i] = tolower(string[i]);
    }
    return string;
}

std::string IO::ReplaceFirst(std::string string, const std::string& src, const std::string& dst)
{
    size_t index = string.find(src);
    if (index != std::string::npos && !src.empty())
    {
        string.replace(index, src.size(), dst);
    }
    return string;
}

std::string IO::ReplaceLast(std::string string, const std::string& src, const std::string& dst)
{
    size_t index = string.rfind(src);
    if (index != std::string::npos && !src.empty())
    {
        string.replace(index, src.size(), dst);
    }
    return string;
}

bool IO::ReadWholeFile(std::vector<char>& data, const std::string& path)
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
        gbl_err << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

std::string IO::GetLastExtension(const std::string& path)
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

std::string IO::GetExtension(const std::string& path)
{
    try
    {
        size_t parent_end = path.find_last_of("\\/:");
        if (parent_end == std::string::npos) parent_end = 0;
        size_t ext_start = path.find_first_of(".", parent_end + 1);
        if(ext_start == std::string::npos) return "";
        return ToLower(path.substr(ext_start + 1));
    }
    catch (const std::exception&)
    {
        gbl_warn << "Failed to retrieve extension on " << path << std::endl;
        return "";
    }
}

bool IO::IsDirectory(const std::string& path)
{
    return fs::is_directory(path);
}

bool IO::VerifyParentFolderAccessible(const std::string& path)
{
    try
    {
        size_t parent_end = path.find_last_of("\\/:");
        if (std::string::npos == parent_end)
        {
            gbl_err << "File's parent folder is not accessible \"" << path << "\"" << std::endl;
            return false;
        }
        std::string dir = path.substr(0, parent_end);

        struct _stat64 stat_buffer;
        if (_stat64(dir.c_str(), &stat_buffer) != 0 || (stat_buffer.st_mode & S_IFDIR) == 0)
        {
            gbl_err << "File's parent folder is not accessible \"" << path << "\"" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool IO::VerifyDifferent(const std::string& in_path, const std::string& out_path)
{
    try
    {
        if (std::filesystem::equivalent(in_path, out_path))
        {
            std::cout << "Input and output paths must not be the same " << std::endl;
            return false;
        }
    }
    catch (std::exception&) {}
    return true;
}

bool IO::CopyFolderHierarchy(const std::string& dst, const std::string& src)
{
    try
    {
        fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::directories_only | fs::copy_options::skip_existing);
        return true;
    }
    catch (std::exception& e)
    {
        std::cout << "Failed to copy folder hierarchy " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> IO::Split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();

        std::string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);

        prev = pos + delim.length();

    } while (pos < str.length() && prev < str.length());

    return tokens;
}

std::vector<std::string> IO::Segment(const std::string& file_name)
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

std::string IO::Join(const std::vector<std::string>& path)
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

bool IO::VerifyFileAccessible(const std::string& path)
{
    struct _stat64 stat_buffer;
    if (path == "." || _stat64(path.c_str(), &stat_buffer) != 0)
    {
        gbl_err << "File is not accessible \"" << path << "\"" << std::endl;
        return false;
    }
    return true;
}

void IO::CancelWrite(const std::string& path, std::ofstream* stream)
{
    try 
    {
        if(stream) stream->close();
        if(path.size()) std::filesystem::remove(path);
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "IO ERROR - failed to delete incomplete file: " << e.what() << " " << ec.message() << std::endl;
    }
}

bool IO::VerifyHeader(const std::string& phyre_path)
{
    try {
        std::ifstream file(phyre_path);
        std::string header;
        header.resize(5, '_');
        file.read((char*)header.data(), 5);
        if (header != "RYHPT")
        {
            gbl_err << "IO ERROR: Incompatible file structure detected" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

std::string IO::FileName(const std::string& path)
{
    size_t last_parent = path.find_last_of("/\\");
    return last_parent == std::string::npos ? path : path.substr(path.find_last_of("/\\") + 1);
}

std::string IO::BaseStem(const std::string& path)
{
    std::string stem = FileName(path);
    return stem.substr(0, stem.find_first_of("."));
}

std::string IO::TruePathCase(const std::string& path)
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
        gbl_err << "IO ERROR: Failed to retrive path's true case - " << e.what() << " " << ec.message() << std::endl;
        return path;
    }
    return path;
}

std::string IO::ParentPath(const std::string& path)
{
    try
    {
        return std::filesystem::path(path).parent_path().string();
    }
    catch (const std::exception& e)
    {
        gbl_err << "IO ERROR: Failed to retrive path \"" << path << "\" parent path - " << e.what() << std::endl;
    }
    return "";
}

std::string IO::BranchFromPath(const std::string& deviating_path, const std::string& base_path)
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

std::string IO::Normalise(const std::string& path)
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

        for (std::string::const_iterator i = b; i + 1 != e; ++i)
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

std::string IO::TmpPath()
{
    try
    {
        std::string temp_path = "tmp";

        if (!CreateDirectoryA("tmp", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            temp_path.resize(512);
            GetTempPathA((DWORD)temp_path.size(), temp_path.data());

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

std::string IO::CreateTmpPath(const std::string& folder_name)
{
    std::string path = TmpPath() + "/" + folder_name;
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove_all(path);
    }
    if (!CreateDirectoryA(path.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
    {
        gbl_err << "Failed to create temp directory " << path << std::endl;
        return "";
    }
    return path;
}

std::string IO::BasePath(const std::string& path_a, const std::string& path_b)
{
    std::vector<std::string> a_segs = IO::Segment(path_a);
    std::vector<std::string> b_segs = IO::Segment(path_b);
    std::string base;
    base.reserve(path_a.size());

    for (size_t i = 0, len = std::min(a_segs.size(), b_segs.size()); i < len; ++i)
    {
        if (ToLower(a_segs[i]) != ToLower(b_segs[i]))
        {
            return base;
        }
        if (!base.empty()) base += "/";
        base += a_segs[i];
    }
    return base;
}

bool IO::ReadReferences(std::map<std::string, std::string>& ref_name_to_id_map, const std::string& ref_list_path)
{
    try
    {
        std::vector<std::pair<std::string, std::string>> ref_name_to_id_list;

        if (std::filesystem::exists(ref_list_path))
        {
            std::ifstream stream(ref_list_path); 
            for (std::string line; std::getline(stream, line);)
            {
                if (line.contains('\"'))
                {
                    size_t q1 = line.find('\"');        if (q1 == line.npos) continue;
                    size_t q2 = line.find('\"', q1 + 1);    if (q2 == line.npos) continue;
                    line = line.substr(q1 + 1, q2 - q1 - 1);
                }
                if (line.ends_with(".dds.phyre") && IO::ToLower(line).contains("d3d11"))
                {
                    ref_name_to_id_list.push_back({ line, line });
                }
            }
        }

        std::string srm_ref_list_path = ReplaceLast(ToLower(ref_list_path), ".ah", "_srm.ah");

        std::vector<std::string> srm_id_list;
        srm_id_list.reserve(ref_name_to_id_list.size());

        if (std::filesystem::exists(srm_ref_list_path))
        {
            std::ifstream stream(srm_ref_list_path);
            for (std::string line; std::getline(stream, line);)
            {
                if (line.ends_with(".dds.phyre"))
                {
                    srm_id_list.push_back(line);
                }
            }
            if (srm_id_list.size() != ref_name_to_id_list.size())
            {
                gbl_warn << "reference files do not correspond - " << srm_ref_list_path << " is not the same length as " << ref_list_path << std::endl;
            }
            else
            {
                for (size_t i = 0; i < ref_name_to_id_list.size(); ++i)
                {
                    ref_name_to_id_list[i].second = srm_id_list[i];
                }
            }
        }
        ref_name_to_id_map.insert(ref_name_to_id_list.begin(), ref_name_to_id_list.end());
    }
    catch (std::exception& e)
    {
        gbl_err << "Error while trying to read reference file \"" << ref_list_path << "\" - " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool IO::FindLocalReferences(std::map<std::string, std::string>& ref_name_to_id_map, const std::string& phyre_path)
{
    try
    {
        std::filesystem::path parent_path = std::filesystem::absolute(phyre_path).parent_path();
        std::vector<std::filesystem::path> search_dirs = { parent_path, parent_path.parent_path(), parent_path.parent_path().parent_path() };

        for (auto& search_dir : search_dirs)
        {
            for (auto& dir_entry : fs::directory_iterator(search_dir))
            {
                std::string de_name = dir_entry.path().string();
                if (dir_entry.is_regular_file() && (IO::GetExtension(de_name) == "ah" || IO::GetExtension(de_name) == "ahwin32") && !ToLower(de_name).ends_with("_srm.ah"))
                {
                    ReadReferences(ref_name_to_id_map, dir_entry.path().string());
                }
            }
        }
    }
    catch (std::exception& e)
    {
        gbl_err << "Error whilst attempting to aquire local references for file \"" << phyre_path << "\" - " << e.what() << std::endl;
        return false;
    }
    if (ref_name_to_id_map.empty())
    {
        return false;
    }
    return true;
}

bool IO::EvaluateReferencesFromLocalSearch(std::map<std::string, std::string>& ref_name_to_file_map, const std::map<std::string, std::string>& ref_name_to_id_map, const std::string& phyre_path, const std::string& extension)
{
    try
    {
        std::vector<std::string> path_segs = Segment(phyre_path);
        if (path_segs.empty()) return false;
        path_segs.pop_back();

        for (auto& seg : path_segs) seg = ToLower(seg);

        for (const auto& name_to_id : ref_name_to_id_map)
        {
            std::string id_stem = ToLower(BaseStem(name_to_id.second));
            if (id_stem.empty() || id_stem.contains("cubemap")) //cube-maps aren't supported right now
            {
                continue;
            }

            {
                std::string filename = ToLower(BaseStem(name_to_id.second)) + "." + extension;

                std::vector<std::string> ref_segs = Segment(name_to_id.second);
                if (ref_segs.empty()) return false;
                ref_segs.pop_back();

                for (auto& seg : ref_segs) seg = ToLower(seg);

                for (size_t i = 0; i < ref_segs.size(); ++i)
                {
                    for (size_t j = 0; j < path_segs.size(); ++j)
                    {
                        if (ref_segs[i] == path_segs[j])
                        {
                            std::string candidate =
                                IO::Join(std::vector<std::string>(path_segs.begin(), path_segs.begin() + j)) + "/" +
                                IO::Join(std::vector<std::string>(ref_segs.begin() + i, ref_segs.end())) + "/" +
                                filename;

                            if (std::filesystem::exists(candidate))
                            {
                                ref_name_to_file_map[name_to_id.first] = candidate;
                                goto label0;
                                break;
                            }
                        }
                    }
                }
            }

            gbl_warn << "Failed to find local file to evaluate reference " << name_to_id.second << " near " << phyre_path << std::endl;

            label0:
            continue;
        }
    }
    catch (std::exception& e)
    {
        gbl_err << "Error whilst attempting to find and evaluate reference - " << e.what() << std::endl;
        return false;
    }
    return true;
}
