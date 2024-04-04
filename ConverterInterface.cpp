#include "ConverterInterface.h"
#include "FileIOUtils.h"
#include "VBFArchive.h"
#include <iostream>
#include <filesystem>
#include <windows.h>
#include <Dbghelp.h>
#include <tchar.h>
#include "args-parser/all.hpp"
#include <assimp/ParsingUtils.h>
#include <assimp/postprocess.h> 
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/BaseImporter.h>
#include <ranges>
#include <filesystem>
#include <set>
#include <Scene.h>
#include <AssimpConversion.h>
#include <Texture2D.h>

namespace fs = std::filesystem;

FILE* GlobalLogger::file = stdout;
std::stringstream GlobalLogger::sstream;
bool GlobalLogger::to_file = true;
GlobalLogger GlobalLoggers::InfoLogger(0);
GlobalLogger GlobalLoggers::KeyInfoLogger(1);
GlobalLogger GlobalLoggers::SuccessLogger(2);
GlobalLogger GlobalLoggers::WarningLogger(3);
GlobalLogger GlobalLoggers::ErrorLogger(4);

namespace
{
    typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

    void create_minidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
    {
        HMODULE mhLib = ::LoadLibrary(_T("dbghelp.dll"));
        MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");

        HANDLE  hFile = ::CreateFile(_T("core.dmp"), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        _MINIDUMP_EXCEPTION_INFORMATION ExInfo = {};
        ExInfo.ThreadId = ::GetCurrentThreadId();
        ExInfo.ExceptionPointers = apExceptionInfo;
        ExInfo.ClientPointers = FALSE;

        pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
        CloseHandle(hFile);
    }

    LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo)
    {
        create_minidump(apExceptionInfo);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    bool run_unpack(const char* exe, const std::string& orig_path, const std::string& intr_path, bool unpack_refs)
    {
        std::list<std::array<std::string, 2>> jobs;

        if (!IO::VerifyFileAccessible(orig_path) ||
            !IO::VerifyParentFolderAccessible(intr_path))
        {
            return false;
        }

        if (intr_path.ends_with("dds.phyre") || intr_path.ends_with("dae.phyre"))
        {
            LOG(ERR) << "intermediary_asset file type is invalid for conversion " << intr_path << std::endl;
            return false;
        }

        if (IO::IsDirectory(orig_path))
        {
            if (IO::CopyFolderHierarchy(intr_path, orig_path) == false)
            {
                return false;
            }

            for (const auto& entry : fs::recursive_directory_iterator(orig_path))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }
                std::string orig_file = entry.path().string();
                std::string intr_file;

                if (IO::GetExtension(orig_file) == "dae.phyre")
                {
                    intr_file = intr_path + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".fbx").string();
                }
                else if (IO::GetExtension(orig_file) == "dds.phyre")
                {
                    intr_file = intr_path + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".png").string();
                }
                else continue;

                jobs.push_back({ IO::Normalise(orig_file), IO::Normalise(intr_file) });
            }
        }
        else
        {
            if (!IO::VerifyHeader(orig_path) ||
                !IO::VerifyDifferent(orig_path, intr_path))
            {
                return false;
            }

            std::string ext = IO::GetExtension(orig_path);
            if (ext != "dae.phyre" && ext != "dds.phyre")
            {
                LOG(ERR) << "Unrecognised extension for file " << orig_path << std::endl;
                return false;
            }

            jobs.push_back({ orig_path, intr_path });
        }

        if (unpack_refs)
        {
            LOG(INFO) << "Evaluating asset references" << std::endl;

            std::map<std::string, std::string> ref_file_unpack_map;

            for (auto& job : jobs)
            {
                if (!job[0].ends_with("dae.phyre"))
                {
                    continue;
                }
                std::map<std::string, std::string> ref_name_id_map;
                std::map<std::string, std::string> ref_name_file_map;

                if (!IO::FindLocalReferences(ref_name_id_map, job[0]))
                {
                    LOG(ERR) << "Found no *.ah file near " << job[0] << " to unpack texture references" << std::endl;
                    continue;
                }
                IO::EvaluateReferencesFromLocalSearch(ref_name_file_map, ref_name_id_map, job[0], "dds.phyre");

                for (auto& name_file : ref_name_file_map)
                {
                    std::string orig_rel_root = std::filesystem::relative(name_file.second, fs::path(job[0]).parent_path()).parent_path().string();
                    std::string intr_root = fs::absolute(fs::path(job[1]).parent_path().string() + "/" + orig_rel_root).string();
                    fs::path intr_file = intr_root + "/" + fs::path(name_file.second).filename().string();

                    if (IO::GetExtension(name_file.first) == "dae.phyre")
                    {
                        intr_root = intr_file.replace_extension().replace_extension(".fbx").string();
                    }
                    else if (IO::GetExtension(name_file.first) == "dds.phyre")
                    {
                        intr_root = intr_file.replace_extension().replace_extension(".png").string();
                    }

                    ref_file_unpack_map[name_file.second] = intr_file.string();
                }
            }

            LOG(INFO) << "Unpacking asset references" << std::endl;

            for (const auto& file_unpack : ref_file_unpack_map)
            {
                std::string orig_file = IO::ToLower(IO::Normalise(file_unpack.first));
                auto job_it = jobs.end();

                for (auto it = jobs.begin(); it != jobs.end(); ++it)
                {
                    if (IO::ToLower(IO::Normalise(it->at(0))) == orig_file)
                    {
                        job_it = it;
                        break;
                    }
                }

                if (job_it != jobs.end() || !fs::exists(file_unpack.second))
                {
                    fs::create_directories(file_unpack.second.substr(0, file_unpack.second.find_last_of("/")));

                    const int   _argc = 4;
                    const char* _argv[4] = { exe, "unpack", file_unpack.first.c_str(), file_unpack.second.c_str() };
                    ConverterInterface::Run(_argc, _argv);
                }

                if (job_it != jobs.end())
                {
                    jobs.erase(job_it);
                }
            }
        }

        for (auto& job : jobs)
        {
            std::string ext = IO::GetExtension(job[0]);
            if (ext == "dae.phyre" && ConverterInterface::UnpackDAE(job[0], job[1]) == false ||
                ext == "dds.phyre" && ConverterInterface::UnpackDDS(job[0], job[1]) == false
            )
                LOG(ERR) << "FAILURE unpacking to " << IO::BranchFromPath(job[1], job[0]) << std::endl;
            else
                LOG(SUCCESS) << "Success unpacking to " << IO::BranchFromPath(job[1], job[0]) << std::endl;
        }

        return true;
    }

    bool run_pack(const char* exe, const std::string& orig_path, const std::string& intr_path, const std::string& mod_path)
    {
        std::vector<std::array<std::string, 3>> jobs;

        if (!IO::VerifyFileAccessible(orig_path) ||
            !IO::VerifyFileAccessible(intr_path) ||
            !IO::VerifyDifferent(orig_path, mod_path) ||
            !IO::VerifyParentFolderAccessible(mod_path))
        {
            return false;
        }

        if (intr_path.ends_with("dds.phyre") || intr_path.ends_with("dae.phyre"))
        {
            LOG(ERR) << "intermediary_asset file type is invalid for conversion " << intr_path << std::endl;
            return false;
        }

        if (IO::IsDirectory(orig_path))
        {
            if (!IO::IsDirectory(intr_path))
            {
                LOG(ERR) << "intermediary_asset folder path is not a directory " << intr_path << std::endl;
                return false;
            }
            if (!IO::CopyFolderHierarchy(mod_path, orig_path))
            {
                return false;
            }

            bool any_match_found = false;

            for (const auto& entry : fs::recursive_directory_iterator(intr_path))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }
                std::string intr_file = entry.path().string();
                std::string extensions[] = { "dae.phyre", "dds.phyre" };

                for (const std::string& ext : extensions)
                {
                    std::string orig_file = orig_path + "/" + fs::relative(intr_file, intr_path).replace_extension(ext).string();
                    if (!fs::exists(orig_file))
                    {
                        continue;
                    }
                    std::string mod_file = mod_path + "/" + fs::relative(orig_file, orig_path).string();

                    jobs.push_back({ orig_file, intr_file, mod_file });

                    any_match_found = true;
                }
            }

            if (!any_match_found)
            {
                LOG(WARN) << "Found no corresponding files across each folder" << std::endl;
            }
        }
        else
        {

            if (IO::IsDirectory(intr_path))
            {
                LOG(ERR) << "intermediary_asset asset path is a directory, but the original_asset_path is a file" << intr_path << std::endl;
                return false;
            }

            if (!IO::VerifyHeader(orig_path))
            {
                return false;
            }

            std::string ext = IO::GetExtension(orig_path);
            if (ext != "dae.phyre" && ext != "dds.phyre")
            {
                LOG(ERR) << "Unrecognised extension for file " << orig_path << std::endl;
                return false;
            }

            jobs.push_back({ orig_path, intr_path, mod_path });
        }

        for (auto& job : jobs)
        {
            std::string ext = IO::GetExtension(job[0]);
            if (ext == "dae.phyre" && ConverterInterface::PackDAE(job[0], job[1], job[2]) == false ||
                ext == "dds.phyre" && ConverterInterface::PackDDS(job[0], job[1], job[2]) == false
            )
                LOG(ERR) << "FAILURE packing to " << IO::BranchFromPath(job[2], job[0]) << std::endl;
            else
                LOG(SUCCESS) << "Success packing to " << IO::BranchFromPath(job[2], job[0]) << std::endl;
        }

        return true;
    }
}

bool ConverterInterface::Initialise()
{
    SetUnhandledExceptionFilter(unhandled_handler);

    if ((CoInitializeEx(NULL, COINIT_MULTITHREADED)) < 0) //needed for DirectXTex
    {
        LOG(ERR) << "COINIT_MULTITHREADED failed " << std::endl;
        return false;
    }

    return true;
}

void ConverterInterface::Free()
{
    CoUninitialize();
}

bool ConverterInterface::UnpackDAE(const std::string& original_path, const std::string& intermediary_path)
{
    LOG(INFO) << "Unpacking " << IO::FileName(original_path) << std::endl;
    LOG(INFO) << "(1) Importing " << IO::FileName(original_path) << std::endl;

    Scene scene;
    aiScene aScene;
    if (!scene.unpack(original_path) ||
        !scene.evaluate_local_material_textures(original_path, intermediary_path) ||
        !AssimpConversion::ConvertToAssimpScene(aScene, scene)
    ) {
        return false;
    }

    LOG(INFO) << "(2) Exporting " << IO::FileName(intermediary_path) << std::endl;

    Assimp::Exporter exporter;
    if (aiReturn ret = exporter.Export(&aScene, IO::GetLastExtension(intermediary_path), intermediary_path, 0))
    {
        LOG(ERR) << "Export Error: " << exporter.GetErrorString() << std::endl;
        IO::CancelWrite(intermediary_path);
        return false;
    }

    return true;
}

bool ConverterInterface::PackDAE(const std::string& original_path, const std::string& intermediary_path, const std::string& mod_output_path)
{
    LOG(INFO) << "Packing " << IO::FileName(original_path) << std::endl;
    LOG(INFO) << "(1) Importing " << IO::FileName(intermediary_path) << std::endl;

    uint32_t import_flags = aiProcess_CalcTangentSpace | aiProcess_Triangulate;

    Assimp::Importer intermediary_importer;
    const aiScene* intr_scene = intermediary_importer.ReadFile(intermediary_path, import_flags);
    if (!intr_scene)
    {
        LOG(ERR) << "Failed to import intermediary scene " << intermediary_path << " " << (intermediary_importer.GetErrorString()) << std::endl;
        return false;
    }

    LOG(INFO) << "(2) Importing " << IO::FileName(original_path) << std::endl;

    Scene scene;
    if (!scene.unpack(original_path))
    {
        return false;
    }

    LOG(INFO) << "(3) Applying " << IO::FileName(intermediary_path) << " To " << IO::FileName(original_path) << std::endl;

    AssimpConversion::ClearUnusedBones(*(aiScene*)intr_scene);
    if (!AssimpConversion::ConvertFromAssimpScene(*intr_scene, scene))
    {
        return false;
    }

    LOG(INFO) << "(4) Exporting " << IO::FileName(mod_output_path) << std::endl;

    if (!scene.pack(mod_output_path, original_path))
    {
        IO::CancelWrite(mod_output_path);
        return false;
    }

    return true;
}

bool ConverterInterface::UnpackDDS(const std::string& original_path, const std::string& intermediary_path)
{
    LOG(INFO) << "Unpacking " << IO::FileName(original_path) << std::endl;
    LOG(INFO) << "(1) Importing " << IO::FileName(original_path) << std::endl;

    Texture2D texture;
    if (!texture.unpack(original_path))
    {
        return false;
    }

    LOG(INFO) << "(2) Exporting " << IO::FileName(intermediary_path) << std::endl;

    if(!texture.save(intermediary_path))
    {
        IO::CancelWrite(intermediary_path);
        return false;
    }

    return true;
}

bool ConverterInterface::PackDDS(const std::string& original_path, const std::string& intermediary_path, const std::string& mod_output_path)
{
    LOG(INFO) << "Packing " << IO::FileName(original_path) << std::endl;
    LOG(INFO) << "(1) Importing " << IO::FileName(original_path) << std::endl;

    Texture2D texture;
    if (!texture.unpack(original_path))
    {
        return false;
    }

    LOG(INFO) << "(2) Applying " << IO::FileName(intermediary_path) << " To " << IO::FileName(original_path) << std::endl;

    if (!texture.apply(intermediary_path))
    {
        return false;
    }

    LOG(INFO) << "(3) Exporting " << IO::FileName(mod_output_path) << std::endl;

    if (!texture.pack(mod_output_path, original_path))
    {
        IO::CancelWrite(mod_output_path);
        return false;
    }

    return true;
}

bool ConverterInterface::Run(int argc, const char** argv)
{
    try 
    {
        Args::CmdLine cmd(argc, argv, Args::CmdLine::CmdLineOpt::CommandIsRequired);

        Args::Command unpack(SL("unpack"), Args::ValueOptions::ManyValues, false);
        unpack.setDescription(SL("unpack file(s)."));
        unpack.setLongDescription(SL("unpack file(s) into an intermiary format (fbx, png, etc.), given a original_asset_path then intermediary_asset_path as arguments."));

        Args::Command pack(SL("pack"), Args::ValueOptions::ManyValues, false);
        pack.setDescription(SL("pack file(s)."));
        pack.setLongDescription(SL("pack file(s)'s intermediary changes into new file(s), given an original_asset_path, intermediary_asset_path and modded_asset_path as arguments."));

        Args::Arg unpack_refs(SL('r'), SL("unpack_refs"), false, false);
        unpack_refs.setDescription(SL("unpack refs to dir."));
        unpack_refs.setLongDescription(SL("unpacks all file references (i.e. material textures). attemps to unpack references local to the asset."));

        Args::Help help;
        help.setAppDescription(SL(
        R"USAGE(
************************************************
*   FFXII Asset Converter ver.)USAGE" CONVERTER_VERSION R"USAGE( by Roelin, hosted on Nexus.
*
*   example: FFXIIConvert.exe unpack -r folder1/c1004.dae.phyre folder2/c1004.fbx
*   example: FFXIIConvert.exe pack folder1/c1004.dae.phyre folder2/c1004.fbx folder3/new_c1004.dae.phyre
*   
*   The optional argument -r or --unpack_refs automatically unpacks all of the model's texture references.
*   The term "original_asset_path" refers to the original, unmodified asset (.dae.phyre, .dds.phyre).
*   The term "intermediary_asset_path" refers to the unpacked, editable form of the asset (.fbx, .png, etc.).
*   The term "modded_asset_path" refers to the repacked, modified form of the asset (.dae.phyre, .dds.phyre).
*
*   It's possible for the original_asset_path and/or modded_asset_path to index directly into a vbf:
*       E.g: (...)/FFXII_TZA.vbf/gamedata/d3d11/artdata/chr/chara/c10/c1004/mws/c1004.dae.phyre
*   In which case, the asset files will be extracted, then injected back into the vbf after modifications are applied.
*   
*   Please contact me on Nexus if you encounter any bugs.
************************************************
)USAGE"));
        help.setExecutable(argv[0]);

        unpack.addArg(unpack_refs);
        cmd.addArg(unpack);
        cmd.addArg(pack);
        cmd.addArg(help);
        cmd.setPositionalDescription("original_asset_path intermediary_asset_path, modded_asset_path.");

        cmd.parse();

        std::string orig_path;
        std::string intr_path;
        std::string mod_path; 

        if (unpack.isDefined())
        {
            if (unpack.values().size() != 2)
            {
                LOG(ERR) << "Incorrect number of unpack arguments (expected 2, recieved " << unpack.values().size() << ")" << std::endl;
                return false;
            }
            orig_path = fs::absolute(IO::Normalise(unpack.values()[0])).string();
            intr_path = fs::absolute(IO::Normalise(unpack.values()[1])).string();
        }

        if (pack.isDefined())
        {
            if (pack.values().size() != 3)
            {
                LOG(ERR) << "Incorrect number of pack arguments (expected 3, recieved " << pack.values().size() << ")" << std::endl;
                return false;
            }
            orig_path = fs::absolute(IO::Normalise(pack.values()[0])).string();
            intr_path = fs::absolute(IO::Normalise(pack.values()[1])).string();
            mod_path = fs::absolute(IO::Normalise(pack.values()[2])).string();
        }

        if (orig_path.contains(".vbf"))
        {
            std::string vbf_path, vbf_file_name;
            if (!VBFUtils::Separate(orig_path, vbf_path, vbf_file_name))
            {
                LOG(ERR) << "Failed to deduce archive paths for " << orig_path << std::endl;
                return false;
            }
            LOG(INFO) << "Loading vbf " << vbf_path << std::endl;

            VBFArchive vbf;
            if (!vbf.load(vbf_path))
            {
                LOG(ERR) << "Failed to load" << vbf_path << std::endl;
                return false;
            }
            VBFArchive::TreeNode* node = vbf.find_node(vbf_file_name);
            if (node == nullptr)
            {
                LOG(ERR) << "Failed to find file/folder in vbf - " << vbf_file_name << std::endl;
                return false;
            }

            LOG(INFO) << "Extracting vbf files" << std::endl;

            std::string tmp = IO::CreateTmpPath("cmd_vbf_tmp_orig");
            if (tmp.empty())
            {
                LOG(ERR) << "Failed to create tmp vbf extraction path" << std::endl;
                return false;
            }
            if (!vbf.extract_all(*node, tmp))
            {
                LOG(ERR) << "Failed to extract files from vbf - " << vbf_file_name << std::endl;
                return false;
            }

            std::string tmp_file = tmp + (IO::IsDirectory(orig_path) ? "" : "/" + IO::FileName(orig_path));

            std::vector<const char*> args;
            args.reserve(7);
            args.push_back(argv[0]);
            args.push_back(pack.isDefined() ? "pack" : "unpack");
            args.push_back(tmp_file.c_str());
            args.push_back(intr_path.c_str());
            if (pack.isDefined()) args.push_back(mod_path.c_str());
            if (unpack_refs.isDefined()) args.push_back("-r");

            if (!Run(args.size(), args.data()))
            {
                fs::remove_all(tmp);
                return false;
            }
            
            fs::remove_all(tmp);
            return true;
        }

        if (pack.isDefined() && mod_path.contains(".vbf"))
        {
            std::string vbf_path, vbf_file_name;
            if (!VBFUtils::Separate(mod_path, vbf_path, vbf_file_name))
            {
                LOG(ERR) << "Failed to deduce archive paths for " << mod_path << std::endl;
                return false;
            }

            std::string tmp = IO::CreateTmpPath("cmd_vbf_tmp_out");
            if (tmp.empty())
            {
                LOG(ERR) << "Failed to create tmp vbf injection path" << std::endl;
                return false;
            }

            LOG(INFO) << "Loading vbf " << vbf_path << std::endl;

            VBFArchive vbf;
            if (!vbf.load(vbf_path))
            {
                LOG(ERR) << "Failed to load " << vbf_path << std::endl;
                return false;
            }
            std::string tmp_file = tmp + (IO::IsDirectory(mod_path) ? "" : "/" + IO::FileName(mod_path));

            int _argc = 5;
            const char* _argv[] = { argv[0], "pack", orig_path.c_str(), intr_path.c_str(), tmp_file.c_str() };

            if (!Run(_argc, _argv))
            {
                fs::remove_all(tmp);
                return false;
            }

            VBFArchive::TreeNode* node = vbf.find_node(vbf_file_name);
            if (node == nullptr)
            {
                LOG(ERR) << "Failed to find file/folder in vbf - " << vbf_file_name << std::endl;
                return false;
            }

            LOG(INFO) << "Injecting files into vbf" << std::endl;

            if (!vbf.inject_all(*node, tmp))
            {
                LOG(ERR) << "Failed to inject files into vbf - " << vbf_file_name << std::endl;
            }

            if(!vbf.update_header())
            {
                LOG(ERR) << "Failed to update vbf header of " << vbf_file_name << std::endl;
                return false;
            }
            fs::remove_all(tmp);
            return true;
        }

        if (unpack.isDefined())
        {
            return run_unpack(argv[0], orig_path, intr_path, unpack_refs.isDefined());
        }

        if (pack.isDefined())
        {
            return run_pack(argv[0], orig_path, intr_path, mod_path);
        }
    }
    catch (const Args::HelpHasBeenPrintedException&)
    {
    }
    catch (const Args::BaseException& e)
    {
        LOG(ERR) << e.desc() << std::endl;
        return false;
    }
    catch (const std::exception& e)
    {
        LOG(ERR) << "Unhandled Exception: " << e.what() << std::endl;
        return false;
    }
    catch (const ::CriticalFailure& cf)
    {
        LOG(ERR) << cf.message << std::endl;
        return false;
    }
    return false;
}

GlobalLogger::GlobalLogger(uint32_t ID) : ID(ID) {}

void GlobalLogger::write(const std::string& string)
{
    if (!file || !to_file)
    {
        sstream << string;
        return;
    }

    if (file != stdout && file != stderr)
    {
        uint32_t size = string.size();
        if (fwrite(&ID, 1, sizeof(ID), file) != sizeof(ID) ||
            fwrite(&size, 1, sizeof(size), file) != sizeof(size)
        ) {
            std::cerr << "Failed to write to log file" << std::endl;
        }
    }
    if (fwrite(string.data(), 1, string.size(), file) != string.size())
    {
        std::cerr << "Failed to write to log file" << std::endl;
    }
    fflush(file);
}

void GlobalLogger::direct_to_file()
{
    if (!to_file)
    {
        if (file)
        {
            fflush(file);
        }
        sstream.str("");
        sstream.clear();
        to_file = true;
    }
}
void GlobalLogger::direct_to_sstream()
{
    if (to_file)
    {
        if (file)
        {
            fflush(file);
        }
        sstream.str("");
        sstream.clear();
        to_file = false;
    }
}

TemporaryLogger::TemporaryLogger(GlobalLogger& logger)
    : logger(logger)
{}

TemporaryLogger::~TemporaryLogger()
{
    if (sstream.tellp() != 0)
    {
        logger.write(sstream.str());
    }
}
