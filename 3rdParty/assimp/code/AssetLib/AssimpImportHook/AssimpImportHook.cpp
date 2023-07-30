#include "AssimpImportHook.h"

std::function<void(const std::string&, aiScene*)> Assimp::AssimpImportHook::callback;

bool Assimp::AssimpImportHook::CanRead(const std::string& filename, IOSystem* pIOHandler, bool checkSig) const
{
    pIOHandler;
    checkSig;
    return BaseImporter::SimpleExtensionCheck(filename, extension);
}

const aiImporterDesc* Assimp::AssimpImportHook::GetInfo() const
{
    const static aiImporterDesc desc = {
        (std::string(extension) + " Importer").c_str(),
        "",
        "",
        "",
        aiImporterFlags_SupportBinaryFlavour,
        0,
        0,
        0,
        0,
        extension
    };
    return &desc;
}

void Assimp::AssimpImportHook::GetExtensionList(std::set<std::string>& extensions) const
{
    extensions = { extension };
}

void Assimp::AssimpImportHook::InternReadFile(const std::string& pFile, aiScene* pScene, IOSystem* pIOHandler)
{
    pIOHandler;
    if (callback)
    {
        callback(pFile, pScene);
    }
}
