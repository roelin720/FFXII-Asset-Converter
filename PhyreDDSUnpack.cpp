#include "PhyreDDSUnpack.h"
#include "PhyreIOUtils.h"
#include <iostream>
#include <fstream>
#include "DDS.h"
#include "PhyreDDSPack.h"

using namespace PhyreIO;
using namespace Phyre;

namespace
{
    DEFINE_REQUIRED_CHUNKS(DDS, PAssetReference, PTexture2D);

    struct DDSMagic { uint32_t magic = DirectX::DDS_MAGIC; };
    struct DDSPrefix : public DDSMagic, public DirectX::DDS_HEADER {};
    static_assert(offsetof(DDSPrefix, size) == 4);

    const DDSPrefix BC5_DDS_prefix = {{},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_BC5_UNORM,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
        .caps2 = DDS_FLAGS_VOLUME
    }};
    const DDSPrefix ARGB8_DDS_prefix = {{},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_A8R8G8B8,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    }};
    const DDSPrefix DXT5_DDS_prefix = {{},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_DXT5,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    }};
    const DDSPrefix A8_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_A8,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    } };

    constexpr auto norm_to_3channels = [](DirectX::XMVECTOR* outPixels, const DirectX::XMVECTOR* inPixels, size_t width, size_t y)
    {
        UNREFERENCED_PARAMETER(y);
        for (size_t j = 0; j < width; ++j)
        {
            DirectX::XMVECTOR pix = inPixels[j];
            pix.m128_f32[1] = 1.0f - pix.m128_f32[1];

            float r = pix.m128_f32[0];
            float g = pix.m128_f32[1];
            float rn = r * 2.0f - 1.0f;
            float gn = g * 2.0f - 1.0f;
            float b = std::sqrtf(1.0f - rn * rn + gn * gn) / 2.0f + 0.5f;
            float a = pix.m128_f32[3];

            outPixels[j].m128_f32[0] = r;
            outPixels[j].m128_f32[1] = g;
            outPixels[j].m128_f32[2] = b;
            outPixels[j].m128_f32[3] = a;
        }
    };

    inline constexpr DDSPrefix get_dds_prefix(DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT_BC5_UNORM:      return BC5_DDS_prefix;
            case DXGI_FORMAT_BC3_UNORM:      return DXT5_DDS_prefix;
            case DXGI_FORMAT_B8G8R8A8_UNORM: return ARGB8_DDS_prefix;
            case DXGI_FORMAT_A8_UNORM:       return A8_DDS_prefix;
            default: return DDSPrefix();
        }
    }
}

bool Phyre::DDS::Unpack(const std::string& orig_file, const std::string& out_file)
{
    try {
        HRESULT result = 0;
        DirectX::WICCodecs out_codec_id = GetCodecID(out_file);
        if (!out_codec_id)
        {
            std::cerr << "Unsupported output image format " << out_file << std::endl;
            return false;
        }

        std::ifstream stream = std::ifstream(orig_file, std::ios::binary);
        if (!stream)
        {
            std::cerr << "IO Error: Could not open " << orig_file << std::endl;
            return false;
        }

        std::vector<Phyre::Chunk> chunks;
        if (PhyreIO::GetChunks(chunks, stream, RequredDDSChunkNames) < 0)
        {
            std::cerr << "IO Error: Failed to find PTexture2D chunk in " << orig_file << std::endl;
            return false;
        }

        Phyre::Chunk        c_asset_ref = chunks[size_t(RequredDDSChunks::PAssetReference)];
        Phyre::Chunk        c_texture = chunks[size_t(RequredDDSChunks::PTexture2D)];
        std::vector<byte>   dds_data;

        seek(stream, c_asset_ref.data_offset + 4, std::ios::beg);
        std::string asset_ref_name = read<std::string>(stream);

        seek(stream, c_texture.data_offset + 33, std::ios::beg);
        std::string tex_format = read<std::string>(stream);
        DXGI_FORMAT dxgi_format = GetDXGIFormat(tex_format);
        if (dxgi_format == DXGI_FORMAT_UNKNOWN)
        {
            return false;
        }

        DDSPrefix prefix = get_dds_prefix(dxgi_format);

        seek(stream, c_texture.data_offset + 14, std::ios::beg);
        prefix.width = read<uint32_t>(stream);
        prefix.height = read<uint32_t>(stream);

        size_t row_pitch = 0, slice_pitch = 0;
        if ((result = DirectX::ComputePitch(dxgi_format, prefix.width, prefix.height, row_pitch, slice_pitch, DirectX::CP_FLAGS_LEGACY_DWORD)) < 0)
        {
            std::cerr << "Failed load compute pitch for texture " << orig_file << std::endl;
            return false;
        }
        prefix.pitchOrLinearSize = (uint32_t)row_pitch;

        int64_t tex_data_offset = c_texture.data_offset + tex_format.size() + 71;
        seek(stream, 0, std::ios::end);
        int64_t tex_data_length = tell(stream) - tex_data_offset;
        seek(stream, tex_data_offset, std::ios::beg);

        dds_data.resize(sizeof(prefix) + tex_data_length);
        memcpy(dds_data.data(), &prefix, sizeof(prefix));
        stream.read((char*)dds_data.data() + sizeof(prefix), tex_data_length);
        stream.close();

        DirectX::ScratchImage dds_image;
        if ((result = DirectX::LoadFromDDSMemory(dds_data.data(), dds_data.size(), DirectX::DDS_FLAGS_LEGACY_DWORD, nullptr, dds_image) < 0))
        {
            std::cerr << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
            return false;
        }
        std::vector<byte>().swap(dds_data);

        if (DirectX::IsCompressed(dds_image.GetMetadata().format))
        {
            DirectX::ScratchImage decompressed_image;
            std::swap(dds_image, decompressed_image);
            if ((result = DirectX::Decompress(*decompressed_image.GetImage(0,0,0), DXGI_FORMAT_R8G8B8A8_UNORM, dds_image) < 0))
            {
                std::cerr << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        if (asset_ref_name.find("_n.dds") != std::string::npos)
        {
            DirectX::ScratchImage dx_norm_map;
            std::swap(dds_image, dx_norm_map);
            if ((result = DirectX::TransformImage(*dx_norm_map.GetImage(0, 0, 0), norm_to_3channels, dds_image) < 0))
            {
                std::cerr << "Failed to transform normals" << std::endl;
                return false;
            }
        }

        if ((result = DirectX::SaveToWICFile(
            *dds_image.GetImage(0, 0, 0),
            DirectX::WIC_FLAGS_FORCE_SRGB,
            DirectX::GetWICCodec(out_codec_id),
            std::wstring(out_file.begin(), out_file.end()).c_str()
        )) < 0)
        {
            std::cerr << "Failed to save " << out_file << std::endl;
            return false;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        PhyreIO::CancelWrite(out_file);
        return false;
    }

    return true;
}
