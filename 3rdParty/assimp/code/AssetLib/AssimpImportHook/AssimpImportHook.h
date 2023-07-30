#include <assimp/BaseImporter.h>
#include <functional>

namespace Assimp 
{
/**
 * Used to provide a hook through which dae.phyre models can be imported into assimp.
 *
 * This was used in place of writing a proper custom assimp import/exporter because it's easier, 
 * but still allows for the use of assimp post processing filters.
 *
 */
    class AssimpImportHook : public BaseImporter {
    public:
        static constexpr char extension[] = "dae.phyre";
        /**
        * Set by the unpacker before importing via assimp
        *
        * @param 1, dae.phyre file path
        * @param 2, assimp scene
        *
        * @throws DeadlyImportError thrown if importing fails.
        */
        static std::function<void(const std::string&, aiScene*)> callback; //

        AssimpImportHook() : BaseImporter() {}

        bool CanRead(const std::string& filename, IOSystem* pIOHandler, bool checkSig) const override;
        const aiImporterDesc* GetInfo() const override;
        void GetExtensionList(std::set<std::string>& extensions) const;
        void InternReadFile(const std::string& pFile, aiScene* pScene, IOSystem* pIOHandler) override;
    };
}
