#include "Texture2D.h"
#include <DirectXTex/DirectXTex.h>
#include "ConverterInterface.h"
#include <fstream>

#define CHECKBLOCKREQUIRED(b) if (!b) { gbl_err << "Error: Object block '"#b"' not present" << std::endl; return false; }
#define ASSERTBLOCKREQUIRED(b) if (!b || b->elem_count == 0) { throw std::exception("Error: Object block '"#b"' not present"); }

using namespace IO;
namespace
{
    const Texture2D::DDSPrefix BC5_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_BC5_UNORM,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
        .caps2 = DDS_FLAGS_VOLUME
    } };
    const Texture2D::DDSPrefix ARGB8_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_A8R8G8B8,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    } };
    const Texture2D::DDSPrefix DXT5_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_DXT5,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    } };
    const Texture2D::DDSPrefix DXT3_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_DXT3,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    } };
    const Texture2D::DDSPrefix DXT1_DDS_prefix = { {},
    {
        .size = sizeof(DirectX::DDS_HEADER),
        .flags = DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_TEXTURE,
        .depth = 1,
        .ddspf = DirectX::DDSPF_DXT1,
        .caps = DDS_SURFACE_FLAGS_TEXTURE,
    } };
    const Texture2D::DDSPrefix A8_DDS_prefix = { {},
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

    inline constexpr Texture2D::DDSPrefix get_dds_prefix(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_BC5_UNORM:      return BC5_DDS_prefix;
        case DXGI_FORMAT_BC2_UNORM:      return DXT3_DDS_prefix;
        case DXGI_FORMAT_BC3_UNORM:      return DXT5_DDS_prefix;
        case DXGI_FORMAT_BC1_UNORM:      return DXT1_DDS_prefix;
        case DXGI_FORMAT_B8G8R8A8_UNORM: return ARGB8_DDS_prefix;
        case DXGI_FORMAT_A8_UNORM:       return A8_DDS_prefix;
        default: return Texture2D::DDSPrefix();
        }
    }

    DXGI_FORMAT GetDXGIFormat(std::string format_name)
    {
        if (format_name == "BC5")        return DXGI_FORMAT_BC5_UNORM;
        else if (format_name == "DXT5")  return DXGI_FORMAT_BC3_UNORM;
        else if (format_name == "DXT3")  return DXGI_FORMAT_BC2_UNORM;
        else if (format_name == "DXT1")  return DXGI_FORMAT_BC1_UNORM;
        else if (format_name == "ARGB8") return DXGI_FORMAT_B8G8R8A8_UNORM;
        else if (format_name == "A8")    return DXGI_FORMAT_A8_UNORM;
        else
        {
            gbl_err << "Unrecognised dds format " << format_name << std::endl;
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    DirectX::WICCodecs GetCodecID(const std::string& path)
    {
        std::string ext = GetExtension(path);

        if (ext == "bmp")                  return DirectX::WIC_CODEC_BMP;
        if (ext == "jpg" || ext == "jpeg") return DirectX::WIC_CODEC_JPEG;
        if (ext == "png")                  return DirectX::WIC_CODEC_PNG;
        if (ext == "tiff")                 return DirectX::WIC_CODEC_TIFF;
        if (ext == "wmp")                  return DirectX::WIC_CODEC_WMP;

        return (DirectX::WICCodecs)0;
    }
}

bool Texture2D::FileObjectBlocks::initialise(std::istream& stream)
{
   int64_t stream_pos = tell(stream);

    if (!LoadObjectBlocks(stream, *this))
    {
        gbl_err << "Failed to load object blocks" << std::endl;
        return false;
    }

    asset_reference = FindObjectBlock(*this, "PAssetReference");
    texture2D       = FindObjectBlock(*this, "PTexture2D");
    cube_map        = FindObjectBlock(*this, "PTextureCubeMap");

    seek(stream, stream_pos, std::ios::beg);

    return true;
}

bool Texture2D::unpack(const std::string& orig_texture_file)
{
    try
    {
        std::ifstream stream = std::ifstream(orig_texture_file, std::ios::binary);
        if (!stream)
        {
            gbl_err << "IO Error: Could not open " << orig_texture_file << std::endl;
            return false;
        }

        if (!obj_blocks.initialise(stream))
        {
            gbl_err << "Failed to load object blocks" << std::endl;
            return false;
        }

        if (obj_blocks.cube_map != nullptr)
        {
            gbl_warn << "Cube-maps are currently not supported " << std::endl;
            gbl_err.ostream->flush();
            gbl_err.ostream = gbl_warn.ostream; //not desirable but a quick and dirty solution to making cubemap errors hidable in the GUI
            return false;
        }

        CHECKBLOCKREQUIRED(obj_blocks.asset_reference);
        CHECKBLOCKREQUIRED(obj_blocks.texture2D);
        ObjectBlock& ob_asset_ref = *obj_blocks.asset_reference;
        ObjectBlock& ob_texture = *obj_blocks.texture2D;

        seek(stream, 0, std::ios::beg);
        header = read<PhyreHeader>(stream);

        CONVASSERT(ob_asset_ref.arrays_size > 0);
        seek(stream, ob_asset_ref.data_offset + ob_asset_ref.objects_size, std::ios::beg);
        asset_ref_name = read<std::string>(stream);

        seek(stream, obj_blocks.back().data_offset + obj_blocks.back().objects_size + 11, std::ios::beg);
        dxgi_format = GetDXGIFormat(read<std::string>(stream));
        if (dxgi_format == DXGI_FORMAT_UNKNOWN)
        {
            return false;
        }

        prefix = get_dds_prefix(dxgi_format);

        CONVASSERT(ob_texture.data_size == 116 || ob_texture.data_size == 22);
        if (ob_texture.data_size == 116)
        {
            seek(stream, ob_texture.data_offset + 2, std::ios::beg);
            prefix.mipMapCount = read<uint32_t>(stream) + 1;

            seek(stream, ob_texture.data_offset + 28, std::ios::beg);
            prefix.width = read<uint32_t>(stream);
            prefix.height = read<uint32_t>(stream);
        }
        else if (ob_texture.data_size == 22)
        {
            seek(stream, ob_texture.data_offset + 2, std::ios::beg);
            prefix.mipMapCount = read<uint32_t>(stream) + 1;

            seek(stream, ob_texture.data_offset + 14, std::ios::beg);
            prefix.width = read<uint32_t>(stream);
            prefix.height = read<uint32_t>(stream);
        }

        size_t row_pitch = 0, slice_pitch = 0;
        if (HRESULT res = DirectX::ComputePitch(dxgi_format, prefix.width, prefix.height, row_pitch, slice_pitch, DirectX::CP_FLAGS_LEGACY_DWORD); res < 0)
        {
            gbl_err << "Failed load compute pitch for texture " << orig_texture_file << std::endl;
            return false;
        }
        prefix.pitchOrLinearSize = (uint32_t)row_pitch;

        int64_t tex_data_offset = obj_blocks.back().data_offset + obj_blocks.back().data_size +
            int64_t(header.array_link_size) +
            int64_t(header.object_link_size) +
            int64_t(header.object_array_link_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.shared_data_size) +
            int64_t(header.shared_data_count) * 12 +
            int64_t(header.indices_size);

        seek(stream, 0, std::ios::end);
        int64_t tex_data_length = tell(stream) - tex_data_offset;
        seek(stream, tex_data_offset, std::ios::beg);

        dds_pixel_data.resize(tex_data_length);
        read(stream, (char*)dds_pixel_data.data(), tex_data_length);
    }
    catch (std::exception& e)
    {
        gbl_err << "ERROR: " << e.what() << std::endl;
        return false;
    }
	return true;
}

bool Texture2D::save(const std::string& intr_texture_file)
{
    try
    {
        if (header.magic_bytes == 0)
        {
            gbl_err << "The texture has not been unpacked to be able to save " << intr_texture_file << std::endl;
            return false;
        }

        std::vector<char> dds_file_data;

        dds_file_data.resize(dds_pixel_data.size() + sizeof(prefix));
        memcpy(dds_file_data.data(), &prefix, sizeof(prefix));
        memcpy(dds_file_data.data() + sizeof(prefix), dds_pixel_data.data(), dds_pixel_data.size());

        DirectX::WICCodecs codec_ID = GetCodecID(intr_texture_file);
        if (!codec_ID)
        {
            gbl_err << "Unsupported output image format " << intr_texture_file << std::endl;
            return false;
        }

        DirectX::ScratchImage dds_image;
        if (HRESULT res = DirectX::LoadFromDDSMemory(dds_file_data.data(), dds_file_data.size(), DirectX::DDS_FLAGS_LEGACY_DWORD, nullptr, dds_image); res < 0)
        {
            gbl_err << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
            return false;
        }
        std::vector<char>().swap(dds_file_data);

        if (DirectX::IsCompressed(dds_image.GetMetadata().format))
        {
            DirectX::ScratchImage decompressed_image;
            std::swap(dds_image, decompressed_image);
            if (HRESULT res = DirectX::Decompress(*decompressed_image.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, dds_image); res < 0)
            {
                gbl_err << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        if (asset_ref_name.find("_n.dds") != std::string::npos)
        {
            DirectX::ScratchImage dx_norm_map;
            std::swap(dds_image, dx_norm_map);
            if (HRESULT res = DirectX::TransformImage(*dx_norm_map.GetImage(0, 0, 0), norm_to_3channels, dds_image); res < 0)
            {
                gbl_err << "Failed to transform normals" << std::endl;
                return false;
            }
        }

        if (HRESULT res = DirectX::SaveToWICFile(
            *dds_image.GetImage(0, 0, 0),
            DirectX::WIC_FLAGS_FORCE_SRGB,
            DirectX::GetWICCodec(codec_ID),
            std::wstring(intr_texture_file.begin(), intr_texture_file.end()).c_str()
        ); res < 0)
        {
            gbl_err << "Failed to save " << intr_texture_file << std::endl;
            return false;
        }
    }
    catch (std::exception& e)
    {
        gbl_err << "ERROR: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool Texture2D::apply(const std::string& intr_texture_file)
{
    try
    {
        if (header.magic_bytes == 0)
        {
            gbl_err << "The texture has not been unpacked to be able to apply " << intr_texture_file << std::endl;
            return false;
        }

        DirectX::ScratchImage wic_image;
        if (HRESULT res = DirectX::LoadFromWICFile(
            std::wstring(intr_texture_file.begin(), intr_texture_file.end()).c_str(),
            DirectX::WIC_FLAGS_IGNORE_SRGB,
            nullptr,
            wic_image
        ); res < 0)
        {
            gbl_err << "Failed to load " << intr_texture_file << std::endl;
            return false;
        }

        prefix.width = (uint32_t)wic_image.GetMetadata().width;
        prefix.height = (uint32_t)wic_image.GetMetadata().height;

        if (asset_ref_name.find("_n.dds") != std::string::npos)
        {
            DirectX::ScratchImage dx_norm_map;
            std::swap(wic_image, dx_norm_map);
            if (HRESULT res = DirectX::TransformImage(*dx_norm_map.GetImage(0, 0, 0), norm_to_2channels, wic_image); res < 0)
            {
                gbl_err << "Failed to transform normals" << std::endl;
                return false;
            }
        }

        if (!DirectX::IsCompressed(dxgi_format) && wic_image.GetMetadata().format != dxgi_format)
        {
            DirectX::ScratchImage backwards_image;
            std::swap(wic_image, backwards_image);
            if (HRESULT res = DirectX::Convert(*backwards_image.GetImage(0, 0, 0), dxgi_format, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, wic_image); res < 0)
            {
                gbl_err << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        if (wic_image.GetMetadata().mipLevels != prefix.mipMapCount)
        {
            DirectX::ScratchImage unmipped_image;
            std::swap(wic_image, unmipped_image);
            if (HRESULT res = DirectX::GenerateMipMaps(*unmipped_image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, prefix.mipMapCount, wic_image); res < 0)
            {
                gbl_err << "Failed generate mipmaps" << std::endl;
                return false;
            }
        }

        if (DirectX::IsCompressed(dxgi_format))
        {
            DirectX::ScratchImage uncompressed_image;
            std::swap(wic_image, uncompressed_image);
            if (HRESULT res = DirectX::Compress(uncompressed_image.GetImages(), uncompressed_image.GetImageCount(), uncompressed_image.GetMetadata(), dxgi_format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, wic_image); res < 0)
            {
                gbl_err << "Failed load DDS texture from memory (likely a formatting failure)" << std::endl;
                return false;
            }
        }

        dds_pixel_data.resize(wic_image.GetPixelsSize());
        memcpy(dds_pixel_data.data(), (char*)wic_image.GetPixels(), wic_image.GetPixelsSize());

        header.max_texture_size = (int32_t)wic_image.GetImage(0, 0, 0)->slicePitch;
    }
    catch (std::exception& e)
    {
        gbl_err << "ERROR: " << e.what() << std::endl;
        return false;
    }
	return true;
}

bool Texture2D::pack(const std::string& mod_texture_file, const std::string& orig_texture_file)
{
    try
    {
        if (header.magic_bytes == 0)
        {
            gbl_err << orig_texture_file << " has not been unpacked to be able to pack" << std::endl;
            return false;
        }

        std::ifstream istream = std::ifstream(orig_texture_file, std::ios::binary);
        if (!istream)
        {
            gbl_err << "IO Error: Could not open " << orig_texture_file << std::endl;
            return false;
        }

        std::ofstream ostream = std::ofstream(mod_texture_file, std::ios::binary);
        if (!ostream)
        {
            gbl_err << "IO Error: Could not open " << mod_texture_file << std::endl;
            return false;
        }

        std::vector<char> prelim_data;
        int64_t tex_data_offset = obj_blocks.back().data_offset + obj_blocks.back().data_size +
            int64_t(header.array_link_size) +
            int64_t(header.object_link_size) +
            int64_t(header.object_array_link_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.shared_data_size) +
            int64_t(header.shared_data_count) * 12 +
            int64_t(header.indices_size);

        seek(istream, 0, std::ios::beg);
        prelim_data.resize(tex_data_offset);
        istream.read((char*)prelim_data.data(), prelim_data.size());
        istream.close();
        seek(ostream, 0, std::ios::beg);
        write(ostream, prelim_data.data(), prelim_data.size());
        write(ostream, dds_pixel_data.data(), dds_pixel_data.size());

        std::vector<char>().swap(prelim_data);

        CHECKBLOCKREQUIRED(obj_blocks.texture2D);
        ObjectBlock& ob_texture = *obj_blocks.texture2D;

        CONVASSERT(ob_texture.data_size == 116 || ob_texture.data_size == 22);
        if (ob_texture.data_size == 116)
        {
            seek(ostream, ob_texture.data_offset + 28, std::ios::beg);
            write<uint32_t>(ostream, prefix.width);
            write<uint32_t>(ostream, prefix.height);
        }
        else if (ob_texture.data_size == 22)
        {
            seek(ostream, ob_texture.data_offset + 14, std::ios::beg);
            write<uint32_t>(ostream, prefix.width);
            write<uint32_t>(ostream, prefix.height);
        }

        seek(ostream, 0, std::ios::beg);
        write<PhyreHeader>(ostream, header);
    }
    catch (std::exception& e)
    {
        gbl_err << "ERROR: " << e.what() << std::endl;
        return false;
    }
	return true;
}
