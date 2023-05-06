#include "PhyreInterface.h"
#include <iostream>
#include "AssimpImportHook/AssimpImportHook.h"
#include <assimp/cimport.h>
#include "PhyreIOUtils.h"
#include "PhyreDAEUnpack.h"
#include "PhyreDAEPack.h"
#include "PhyreDDSUnpack.h"
#include "PhyreDDSPack.h"
#include <filesystem>
#include <windows.h>
#include <Dbghelp.h>
#include <tchar.h>

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

const char* PhyreInterface::Usage()
{
    constexpr const char* usage_text = 
R"USAGE(
************************************************
*   FFXII Asset Converter ver.)USAGE" CONVERTER_VERSION R"USAGE( by Roelin, hosted on Nexus
*
*   Command Line Usage:
*       -u, --unpack INPUT<original_asset_path> OUTPUT<replacement_path>
*       -p, --pack INPUT<original_asset_path> INPUT<replacement_path> OUTPUT<mod_output_path>
*       -h, --help
*       
*   example: FFXIIConvert.exe --unpack folder1/c1004.dae.phyre folder2/c1004.fbx
*   example: FFXIIConvert.exe --pack folder1/c1004.dae.phyre folder2/c1004.fbx folder3/new_c1004.dae.phyre
*   
*   The term "original_asset_path" refers to the original, unmodified asset (.dae.phyre, .dds.phyre)
*   The term "replacement_path" refers to the unpacked, editable form of the asset (.fbx, .png, etc.)
*   The term "mod_output_path" refers to the repacked, modified form of the asset (.dae.phyre, .dds.phyre)
*   
*   Please contact me on Nexus if you encounter any bugs.
************************************************
)USAGE";
    return usage_text;
}

const char* PhyreInterface::UsagePrompt()
{
    constexpr const char* usage_prompt = "use --help to view the list of available commands";
    return usage_prompt;
}

bool PhyreInterface::Initialise()
{
    SetUnhandledExceptionFilter(unhandled_handler);

    if ((CoInitializeEx(nullptr, COINIT_MULTITHREADED)) < 0) //needed for DirectXTex
    {
        std::cerr << "COINIT_MULTITHREADED failed " << std::endl;
        return false;
    }

    Assimp::AssimpImportHook::callback = [](const std::string& pFile, aiScene* pScene) //needed for importing via assimp
    {
        Phyre::DAE::Import(pFile, pScene);
    };
    return true;
}

bool PhyreInterface::Run(int argc, const char** argv)
{
    namespace fs = std::filesystem;
    try
    {
        if (argc < 2)
        {
            std::cerr << "No command has been supplied" << std::endl;
            std::clog << UsagePrompt() << std::endl;
            return false;
        }

        std::string option = argv[1];
        if (option == "-h" || option == "--help")
        {
            std::clog << UsagePrompt() << std::endl;
            return true;
        }
        if (option == "-p" || option == "--pack")
        {
            if (argc != 5)
            {
                std::cerr << "Incorrect number of command arguments (expected 3)" << std::endl;
                std::clog << UsagePrompt() << std::endl;
                return false;
            }
            std::string orig_path = fs::absolute(argv[2]).string();
            std::string rep_path = fs::absolute(argv[3]).string();
            std::string out_path = fs::absolute(argv[4]).string();

            if (!PhyreIO::VerifyFileAccessible(orig_path) ||
                !PhyreIO::VerifyFileAccessible(rep_path) ||
                !PhyreIO::VerifyDifferent(orig_path, out_path) ||
                !PhyreIO::VerifyParentFolderAccessible(out_path))
            {
                return false;
            }

            if (PhyreIO::IsDirectory(orig_path))
            {
                if (!PhyreIO::IsDirectory(rep_path))
                {
                    std::cerr << "Replacement folder path is not a directory " << rep_path << std::endl;
                    return false;
                }
                if (!PhyreIO::CopyFolderHierarchy(out_path, orig_path))
                {
                    return false;
                }

                for (const auto& entry : fs::recursive_directory_iterator(rep_path))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }
                    std::string rep_file = entry.path().string();
                    std::string extensions[] = { "dae.phyre", "dds.phyre" };

                    for (const std::string& ext : extensions)
                    {
                        std::string orig_file = orig_path + "/" + fs::relative(rep_file, rep_path).replace_extension(ext).string();
                        if (!fs::exists(orig_file))
                        {
                            continue;
                        }
                        std::string out_file = out_path + "/" + fs::relative(orig_file, orig_path).string();

                        int _argc = 5;
                        const char* _argv[] =
                        {
                            argv[0], "--pack",
                            orig_file.c_str(),
                            rep_file.c_str(),
                            out_file.c_str()
                        };
                        Run(_argc, _argv);
                    }
                }
                return 0;
            }

            if (PhyreIO::IsDirectory(rep_path))
            {
                std::cerr << "Replacement asset path is a directory, but the original asset path is a file" << rep_path << std::endl;
                return false;
            }

            if (!PhyreIO::VerifyPhyreHeader(orig_path))
            {
                return false;
            }

            std::string ext = PhyreIO::GetExtension(orig_path);
            if (ext != "dae.phyre" && ext != "dds.phyre")
            {
                std::cerr << "Unrecognised extension for file " << orig_path << std::endl;
                return false;
            }


            if (ext == "dae.phyre" && Phyre::DAE::Pack(orig_path, rep_path, out_path) == false ||
                ext == "dds.phyre" && Phyre::DDS::Pack(orig_path, rep_path, out_path) == false)
            {
                std::cerr << "FAILURE packing to " << PhyreIO::BranchFromPath(out_path, orig_path) << std::endl;
                return false;
            }

            std::clog << "Success packing to " << PhyreIO::BranchFromPath(out_path, orig_path) << std::endl;
            return 0;
        }
        else if (option == "-u" || option == "--unpack")
        {
            if (argc != 4)
            {
                std::cerr << "Incorrect number of command arguments (expected 2)" << std::endl;
                std::clog << UsagePrompt() << std::endl;
                return false;
            }

            std::string orig_path = fs::absolute(argv[2]).string();
            std::string out_path = fs::absolute(argv[3]).string();

            if (!PhyreIO::VerifyFileAccessible(orig_path) ||
                !PhyreIO::VerifyDifferent(orig_path, out_path) ||
                !PhyreIO::VerifyParentFolderAccessible(out_path))
            {
                return false;
            }

            if (PhyreIO::IsDirectory(orig_path))
            {
                if (PhyreIO::CopyFolderHierarchy(out_path, orig_path) == false)
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
                    std::string out_file;

                    if (PhyreIO::GetExtension(orig_file) == "dae.phyre")
                    {
                        out_file = out_path + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".fbx").string();
                    }
                    else if (PhyreIO::GetExtension(orig_file) == "dds.phyre")
                    {
                        out_file = out_path + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".png").string();
                    }
                    else
                    {
                        continue;
                    }

                    int _argc = 4;
                    const char* _argv[] =
                    {
                        argv[0], "--unpack",
                        orig_file.c_str(),
                        out_file.c_str()
                    };
                    Run(_argc, _argv);
                }
                return 0;
            }

            if (!PhyreIO::VerifyPhyreHeader(orig_path))
            {
                return false;
            }

            std::string ext = PhyreIO::GetExtension(orig_path);
            if (ext != "dae.phyre" && ext != "dds.phyre")
            {
                std::cerr << "Unrecognised extension for file " << orig_path << std::endl;
                return false;
            }

            if (ext == "dae.phyre" && Phyre::DAE::Unpack(orig_path, out_path) == false ||
                ext == "dds.phyre" && Phyre::DDS::Unpack(orig_path, out_path) == false)
            {
                std::cerr << "FAILURE unpacking to " << PhyreIO::BranchFromPath(out_path, orig_path) << std::endl;
                return false;
            }
            std::clog << "Success unpacking to " << PhyreIO::BranchFromPath(out_path, orig_path) << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "Unrecognised command " << argv[1] << std::endl;
            std::clog << UsagePrompt() << std::endl;
            return false;
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled Exception: " << e.what() << std::endl;
        return false;
    }
    catch (const Phyre::CriticalFailure& cf)
    {
        std::cerr << cf.message << std::endl;
        return false;
    }
    return true;
}
