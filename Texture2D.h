#pragma once
#include <DirectXTex/DirectXTex.h>
#include "FileStructs.h"
#include "FileObjectBlock.h"
#include "FileIOUtils.h"
#include <dxgiformat.h>
#include "DDS.h"

class Texture2D
{
public:
    struct FileObjectBlocks : public std::vector<ObjectBlock>
    {
        ObjectBlock* asset_reference = nullptr;
        ObjectBlock* texture2D = nullptr;
        ObjectBlock* cube_map = nullptr;

        bool initialise(std::istream& stream);
    };
    struct DDSMagic { uint32_t magic = DirectX::DDS_MAGIC; };
    struct DDSPrefix : public DDSMagic, public DirectX::DDS_HEADER {};
    static_assert(offsetof(DDSPrefix, size) == 4);

    PhyreHeader header;
    FileObjectBlocks obj_blocks;
    std::string asset_ref_name;

    DDSPrefix prefix {0};
    DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;
    std::vector<char> dds_pixel_data;

    bool to_image(DirectX::ScratchImage& image) const;

    bool unpack(const std::string& orig_texture_path);
    bool save(const std::string& intr_texture_path) const;
    bool apply(const std::string& intr_texture_path);
    bool pack(const std::string& mod_texture_path, const std::string& orig_texture_path) const;
};