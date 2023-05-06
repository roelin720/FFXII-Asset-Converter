#include "PhyreDDSPack.h"
#include "PhyreIOUtils.h"
#include <iostream>
#include <fstream>
#include <dxgiformat.h>
#include "DDS.h"
#include "DirectXTex.h"

using namespace PhyreIO;
using namespace Phyre;

namespace
{
    DEFINE_REQUIRED_CHUNKS(DDS, PAssetReference, PTexture2D);

    constexpr auto norm_to_2channels = [](DirectX::XMVECTOR* outPixels, const DirectX::XMVECTOR* inPixels, size_t width, size_t y)
    {
        UNREFERENCED_PARAMETER(y);
        for (size_t j = 0; j < width; ++j)
        {
            DirectX::XMVECTOR pix = inPixels[j];
            pix.m128_f32[1] = 1.0f - pix.m128_f32[1];
            outPixels[j] = pix;
        }
    };
}

bool Phyre::DDS::Pack(const std::string& orig_file, const std::string& rep_file, const std::string& out_file)
{
    try 
    {
        HRESULT result = 0;
        DirectX::WICCodecs out_codec_id = PhyreIO::GetCodecID(rep_file);
        if (!out_codec_id)
        {
            std::cerr << "Unsupported input image format " << rep_file << std::endl;
            return false;
        }

        DirectX::ScratchImage wic_image;
        if ((result = DirectX::LoadFromWICFile(
            std::wstring(rep_file.begin(), rep_file.end()).c_str(),
            DirectX::WIC_FLAGS_IGNORE_SRGB,
            nullptr,
            wic_image
        )) < 0)
        {
            std::cerr << "Failed to load " << out_file << std::endl;
            return false;
        }

        std::ifstream istream = std::ifstream(orig_file, std::ios::binary);
        if (!istream)
        {
            std::cerr << "IO Error: Could not open " << orig_file << std::endl;
            return false;
        }

        std::vector<Phyre::Chunk> chunks;
        if (PhyreIO::GetChunks(chunks, istream, RequredDDSChunkNames) < 0)
        {
            std::cerr << "IO Error: Failed to find PTexture2D chunk in " << orig_file << std::endl;
            return false;
        }

        Phyre::Chunk c_asset_ref = chunks[size_t(RequredDDSChunks::PAssetReference)];
        Phyre::Chunk c_texture = chunks[size_t(RequredDDSChunks::PTexture2D)];

        seek(istream, c_asset_ref.data_offset + 4, std::ios::beg);
        std::string asset_ref_name = read<std::string>(istream);

        seek(istream, c_texture.data_offset + 33, std::ios::beg);
        std::string tex_format = read<std::string>(istream);
        DXGI_FORMAT dxgi_format = GetDXGIFormat(tex_format);
        if (dxgi_format == DXGI_FORMAT_UNKNOWN)
        {
            return false;
        }

        seek(istream, c_texture.data_offset + 2, std::ios::beg);
        uint32_t mipmap_count = read<uint32_t>(istream) + 1;
        seek(istream, c_texture.data_offset + 14, std::ios::beg);
        uint32_t orig_width = read<uint32_t>(istream);
        uint32_t orig_height = read<uint32_t>(istream);
        uint32_t new_width = (uint32_t)wic_image.GetMetadata().width;
        uint32_t new_height = (uint32_t)wic_image.GetMetadata().height;

        if (asset_ref_name.find("_n.dds") != std::string::npos)
        {
            DirectX::ScratchImage dx_norm_map;
            std::swap(wic_image, dx_norm_map);
            if ((result = DirectX::TransformImage(*dx_norm_map.GetImage(0, 0, 0), norm_to_2channels, wic_image) < 0))
            {
                std::cerr << "Failed to transform normals" << std::endl;
                return false;
            }
        }

        if (!DirectX::IsCompressed(dxgi_format) && wic_image.GetMetadata().format != dxgi_format)
        {
            DirectX::ScratchImage backwards_image;
            std::swap(wic_image, backwards_image);
            if ((result = DirectX::Convert(*backwards_image.GetImage(0, 0, 0), dxgi_format, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, wic_image) < 0))
            {
                std::cerr << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        if (wic_image.GetMetadata().mipLevels != mipmap_count)
        {
            DirectX::ScratchImage unmipped_image;
            std::swap(wic_image, unmipped_image);
            if ((result = DirectX::GenerateMipMaps(*unmipped_image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, mipmap_count, wic_image) < 0))
            {
                std::cerr << "Failed generate mipmaps" << std::endl;
                return false;
            }
        }

        if (DirectX::IsCompressed(dxgi_format))
        {
            DirectX::ScratchImage uncompressed_image;
            std::swap(wic_image, uncompressed_image);
            if ((result = DirectX::Compress(uncompressed_image.GetImages(), uncompressed_image.GetImageCount(), uncompressed_image.GetMetadata(), dxgi_format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, wic_image) < 0))
            {
                std::cerr << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        size_t orig_row_pitch = 0, orig_slice_pitch = 0;
        if ((result = DirectX::ComputePitch(dxgi_format, orig_width, orig_height, orig_row_pitch, orig_slice_pitch, DirectX::CP_FLAGS_LEGACY_DWORD)) < 0)
        {
            std::cerr << "Failed load compute pitch for texture " << orig_file << std::endl;
            return false;
        }

        std::vector<byte> orig_data;

        seek(istream, 0, std::ios::beg);
        orig_data.resize(c_texture.data_offset + tex_format.size() + 71);
        istream.read((char*)orig_data.data(), orig_data.size());
        istream.close();

        *(uint32_t*)(orig_data.data() + 80) = (int32_t)wic_image.GetImage(0, 0, 0)->slicePitch;
        *(uint32_t*)(orig_data.data() + c_texture.data_offset + 14) = (uint32_t)new_width;
        *(uint32_t*)(orig_data.data() + c_texture.data_offset + 18) = (uint32_t)new_height;

        std::ofstream ostream = std::ofstream(out_file, std::ios::binary);
        if (!ostream)
        {
            std::cerr << "IO Error: Could not open " << out_file << std::endl;
            return false;
        }
        
        ostream.write((char*)orig_data.data(), orig_data.size());
        ostream.write((char*)wic_image.GetPixels(), wic_image.GetPixelsSize());
    }
    catch (std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        PhyreIO::CancelWrite(out_file);
        return false;
    }
    return true;
}
