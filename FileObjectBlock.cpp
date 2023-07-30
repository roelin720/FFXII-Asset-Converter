#include "FileObjectBlock.h"
#include "ConverterInterface.h"
#include "FileStructs.h"
#include "FileIOUtils.h"
#include <fstream>

using namespace IO;

namespace
{
    bool is_modified(const std::vector<uint32_t>& modified_block_IDs, uint32_t block_ID)
    {
        for (uint32_t ID : modified_block_IDs)
            if (block_ID == ID) return true;
        return false;
    };
}

namespace Linking
{
    using namespace IO;

    template<typename LinkType>
    struct LinkingContext
    {
        LinkType base = {};
        uint32_t elem_count = 0;
        uint32_t mask = 0;
    };

    template<typename LinkType>
    void load_parent_obj_offset(std::istream& stream, LinkType& link, uint32_t mask)
    {
        uint32_t v = var_read<uint32_t>(stream);
        link.parent_offset_flag = v & 1;
        (link.parent_offset_flag ? link.parent_field_offset : link.parent_obj_offset) = v >> 1;
    }
    void initialise_base(std::istream& stream, ArrayLink& link, uint32_t mask)
    {
        load_parent_obj_offset(stream, link, mask);
    }
    void initialise_base(std::istream& stream, ObjectLink& link, uint32_t mask)
    {
        load_parent_obj_offset(stream, link, mask);
        if (mask & uint32_t(SkipLinkField::ObjectBlockID)) link.obj_block_ID = var_read<uint32_t>(stream);
    }

    template<typename LinkType>
    void set_parent_obj_ID(std::istream& stream, LinkType& link, uint32_t index, uint32_t mask)
    {
        link.parent_obj_ID = index;
    }

    void load_dst_fields(std::istream& stream, ObjectLink& link, uint32_t index, uint32_t mask)
    {
        link.shared_data_ID = (mask & uint32_t(SkipLinkField::SharedDataID)) ? 0 : var_read<uint32_t>(stream) ;
        if (link.shared_data_ID == 0)
        {
            link.obj_ID = var_read<uint32_t>(stream);
            if (!(mask & uint32_t(SkipLinkField::ObjectBlockID))) link.obj_block_ID = var_read<uint32_t>(stream);
            if (!(mask & uint32_t(SkipLinkField::ObjectOffset))) link.obj_offset = var_read<uint32_t>(stream);
        }
        link.shared_data_ID -= 1;
        if (!(mask & uint32_t(SkipLinkField::ArrayCount))) 
        {
            link.obj_array_count = var_read<uint32_t>(stream);
        }
    }
    
    void load_dst_fields(std::istream& stream, ArrayLink& link, uint32_t index, uint32_t mask)
    {
        if(!(mask & uint32_t(SkipLinkField::ArrayCount))) link.count = var_read<uint32_t>(stream);
        link.offset = var_read<uint32_t>(stream);
    }

    template<typename LinkType>
    void load_link(std::istream& stream, LinkType& link, uint32_t index, uint32_t mask)
    {
        link.parent_obj_ID = index;
        load_dst_fields(stream, link, index, mask);
    }

    template<typename LinkType>
    inline std::vector<uint32_t> load_ID_list(std::istream& stream, LinkingContext<LinkType>& context)
    {
        uint32_t ID_count = var_read<uint32_t>(stream);
        std::vector<uint32_t> ID_list(ID_count);

        for (uint32_t i = 0; i < ID_count; ++i)
        {
            ID_list[i] = (context.elem_count >> 8) ? var_read<uint32_t>(stream) : read<uint8_t>(stream);
        }
        return ID_list;
    }

    template<typename LinkType>
    bool load_sequential(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        for (uint32_t i = 0; i < context.elem_count; ++i, ++link_it)
        {
            op(stream, *link_it = context.base, i, context.mask);
        }
        return true;
    }

    template<typename LinkType>
    bool load_unlisted(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        std::vector<uint32_t> ID_list = load_ID_list(stream, context);

        for (int32_t i = 0, j = 0; i < context.elem_count; ++i)
        {
            if (j < ID_list.size() && i > ID_list[j]) 
            {
                ++j;
            }
            if (j >= ID_list.size() || i != ID_list[j])
            {
                op(stream, *(link_it++) = context.base, uint32_t(i), context.mask);
            }
        }

        return true;
    }

    template<typename LinkType>
    bool load_listed(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        std::vector<uint32_t> ID_list = load_ID_list(stream, context);

        for (size_t i = 0; i < ID_list.size(); ++i, ++link_it)
        {
            op(stream, *link_it = context.base, ID_list[i], context.mask);
        }
        return true;
    }

    template<typename LinkType>
    bool load_strided(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        uint32_t parent_obj_ID = var_read<uint32_t>(stream);
        uint32_t stride = var_read<uint32_t>(stream);
        uint32_t count = var_read<uint32_t>(stream);
        for (uint32_t i = 0; i < count; ++i, ++link_it, parent_obj_ID += stride)
        {
            op(stream, *link_it = context.base, parent_obj_ID, context.mask);
        }
        return true;
    }

    template<typename LinkType>
    bool load_filtered_by_mask(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        std::vector<uint8_t> bytes((context.elem_count >> 3) + !!(context.elem_count & 7));
        read(stream, bytes.data(), bytes.size());

        uint8_t* cur_byte = bytes.data() - 1;      
        for (uint32_t i = 0; i < context.elem_count; ++i)
        {
            if ((i & 7 ? *cur_byte >>= 1 : *++cur_byte) & 1)
            {
                op(stream, *(link_it++) = context.base, i, context.mask);
            }
        }
        return true;
    }

    template<typename LinkType>
    bool load_sequential_with_IDs(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        uint32_t link_count = var_read<uint32_t>(stream);
        for (uint32_t i = 0; i < link_count; ++i, ++link_it)
        {
            *link_it = context.base;
            if (context.elem_count > 1) 
            {
                link_it->parent_obj_ID = var_read<uint32_t>(stream);
            }
            op(stream, *link_it, link_it->parent_obj_ID, context.mask);
        }
        return true;
    }

    template<typename LinkType>
    bool load_sequential_groups(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it);

    template<typename LinkType>
    static constexpr decltype(load_sequential<LinkType>)* loaders[] =
    {
        load_sequential<LinkType>,
        load_sequential_groups<LinkType>,
        load_listed<LinkType>,
        load_unlisted<LinkType>,
        load_filtered_by_mask<LinkType>,
        load_sequential_with_IDs<LinkType>,
        load_strided<LinkType>
    };

    template<typename LinkType>
    bool load_sequential_groups(std::istream& stream, void (*op)(std::istream&, LinkType&, uint32_t, uint32_t), LinkingContext<LinkType>& context, typename std::vector<LinkType>::iterator& link_it)
    {
        for (auto link_end = link_it + context.elem_count; link_it != link_end;)
        {
            uint32_t group_load_type = read<uint8_t>(stream);
            load_dst_fields(stream, context.base, 0, context.mask);

            if (group_load_type >= _countof(loaders<LinkType>))
            {
                gbl_err << "Unrecognised group object link loading type: " << (int)group_load_type << std::endl;
                return false;
            }
            if (!loaders<LinkType>[group_load_type](stream, set_parent_obj_ID<LinkType>, context, link_it))
            {
                return false;
            }
        }
        return true;
    }

    template<typename LinkType>
    bool load_links(std::istream& stream, std::vector<LinkType>& links, ObjectBlock& object_block)
    {
        auto link_it = links.begin();
        for (;link_it != links.end();)
        {
            uint32_t load_type_and_mask = read<uint8_t>(stream);
            uint32_t load_type = load_type_and_mask & 7;

            LinkingContext<LinkType> context = 
            {
                .elem_count = (uint32_t)object_block.elem_count,
                .mask = (uint32_t(load_type_and_mask) & (~uint32_t(7)))
            };

            initialise_base(stream, context.base, context.mask);

            if (load_type >= _countof(loaders<LinkType>))
            {
                gbl_err << "Unrecognised object link loading type: " << load_type << std::endl;
                return false;
            }
            
            if (!loaders<LinkType>[load_type](stream, load_link<LinkType>, context, link_it))
            {
                return false;
            }
        }
        if (link_it != links.end())
        {
            gbl_err << "Failed to fill all link data" << std::endl;
            return false;
        }
        return true;
    }

    template<typename LinkType>
    bool load_link_group(std::istream& stream, int64_t offset, std::vector<ObjectBlock>& object_blocks)
    {
        seek(stream, offset, std::ios::beg);
        for (int32_t i = 0; i < object_blocks.size(); ++i)
        {
            auto& link_group = object_blocks[i].link_group<LinkType>();

            link_group.offset = stream.tellg();
            if (!Linking::load_links<LinkType>(stream, link_group, object_blocks[i]))
            {
                gbl_err << "Failed to read " << link_group.name <<  " links" << std::endl;
                return false;
            }
            link_group.storage_size = stream.tellg() - link_group.offset;
        }
        return true;
    }
}

bool LoadObjectBlocks(std::istream& stream, std::vector<ObjectBlock>& object_blocks)
{
    try
    {
        std::streampos prev_pos = tell(stream);

        seek(stream, 0, std::ios::beg);

        PhyreHeader header = {};
        read(stream, &header, sizeof(header));

        seek(stream, header.header_size + 8, std::ios_base::beg);
        int64_t namespace_type_count = read<int32_t>(stream);
        int64_t namespace_class_count = read<int32_t>(stream);
        int64_t namespace_class_data_member_count = read<int32_t>(stream);

        seek(stream, 12 + namespace_type_count * 4, std::ios::cur);

        int64_t name_offsets_offset = tell(stream);
        std::vector<int64_t> name_offsets = std::vector<int64_t>(namespace_class_count);

        for (int64_t i = 0; i < namespace_class_count; ++i)
        {
            seek(stream, name_offsets_offset + i * 36 + 8, std::ios_base::beg);
            name_offsets[i] = read<int32_t>(stream);
        }

        seek(stream, name_offsets_offset + 36 * namespace_class_count + namespace_class_data_member_count * 24, std::ios_base::beg);

        int64_t names_base = (int32_t)tell(stream);

        std::vector<std::string> names = std::vector<std::string>(namespace_class_count);
        for (int64_t i = 0; i < namespace_class_count; i++)
        {
            seek(stream, names_base + name_offsets[i], std::ios_base::beg);
            names[i] = read<std::string>(stream);
        }

        seek(stream, header.header_size + header.namespace_size, std::ios_base::beg);

        int64_t object_blocks_data_offset = tell(stream) + 36 * (int64_t)header.object_block_count;
        object_blocks.clear();
        object_blocks.reserve(header.object_block_count);
        
        for (uint32_t i = 0; i < header.object_block_count; ++i)
        {
            ObjectBlock block;

            block.name_id = read<int32_t>(stream);
            block.elem_count = read<int32_t>(stream);
            block.data_size = read<int32_t>(stream);
            block.objects_size = read<int32_t>(stream);
            block.arrays_size = read<int32_t>(stream);
            seek(stream, 4, std::ios::cur);
            block.array_links.resize(read<int32_t>(stream));
            block.object_links.resize(read<int32_t>(stream));
            block.data_offset = object_blocks_data_offset;
            block.elem_size = block.objects_size / block.elem_count;
            block.name = names[block.name_id - 1];

            //std::cout << i << " " << block.name << std::endl;

            object_blocks_data_offset += block.data_size;

            object_blocks.push_back(block);

            seek(stream, 4, std::ios::cur);
        }

        int64_t object_link_offset = object_blocks_data_offset +
            int64_t(header.shared_data_count) * 12 +
            int64_t(header.shared_data_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.object_array_link_size);

        int64_t array_link_offset = object_link_offset + int64_t(header.object_link_size);

        if (!Linking::load_link_group<ObjectLink>(stream, object_link_offset, object_blocks))
        {
            return false;
        }
        if (!Linking::load_link_group<ArrayLink>(stream, array_link_offset, object_blocks))
        {
            return false;
        }

        seek(stream, prev_pos, std::ios_base::beg);
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool ModifyBlockData(std::ostream& ostream, std::istream& istream, PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs, const std::vector<std::vector<char>>& modified_block_data)
{
    CONVASSERT(modified_block_IDs.size() == modified_block_data.size());

    try
    {
        std::vector<char> data_transfer_buffer;
        uint32_t modified_block_index = 0;
        int64_t old_data_begin = object_blocks.front().data_offset;
        int64_t old_data_end = object_blocks.back().data_offset + object_blocks.back().data_size;
        int64_t old_suffix_end = old_data_end + 
            int64_t(header.shared_data_count) * 12 +
            int64_t(header.shared_data_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.object_array_link_size);

        header.block_data_size = 0;

        seek(ostream, old_data_begin, std::ios::beg);

        for (uint32_t i = 0; i < header.object_block_count; ++i)
        {
            ObjectBlock& block = object_blocks[i];

            int64_t prev_offset = block.data_offset;
            block.data_offset = tell(ostream);
            if (!is_modified(modified_block_IDs, i))
            {
                data_transfer_buffer.resize(block.data_size);
                seek(istream, prev_offset, std::ios::beg);
                read(istream, data_transfer_buffer.data(), data_transfer_buffer.size());
                write(ostream, data_transfer_buffer.data(), data_transfer_buffer.size());
            }
            else
            {
                write(ostream, modified_block_data[modified_block_index].data(), modified_block_data[modified_block_index].size());
                block.data_size = modified_block_data[modified_block_index].size();
                ++modified_block_index;
            }
           
            header.block_data_size += block.data_size;
        }

        data_transfer_buffer.resize(old_suffix_end - old_data_end);
        seek(istream, old_data_end, std::ios::beg);
        read(istream, data_transfer_buffer.data(), data_transfer_buffer.size());
        write(ostream, data_transfer_buffer.data(), data_transfer_buffer.size());
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool ModifyBlockLinks(std::ostream& ostream, std::istream& istream, PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs)
{
    try
    {
        std::vector<char> data_transfer_buffer;

        header.array_link_count = 0;
        header.array_link_size = 0;
        header.object_link_size = 0;
        header.object_link_count = 0;

        int64_t object_link_start_offset =
            object_blocks.back().data_offset + object_blocks.back().data_size +
            int64_t(header.shared_data_count) * 12 +
            int64_t(header.shared_data_size) +
            int64_t(header.header_class_object_block_count) * 4 +
            int64_t(header.header_class_child_count) * 16 +
            int64_t(header.object_array_link_size);

        seek(ostream, object_link_start_offset, std::ios::beg);

        for (uint32_t i = 0; i < header.object_block_count; ++i)
        {
            ObjectBlock& block = object_blocks[i];

            int64_t prev_offset = block.object_links.offset;
            block.object_links.offset = tell(ostream);

            if (block.object_links.size() == 0)
            {
                continue;
            }
            if (!is_modified(modified_block_IDs, i))
            {
                data_transfer_buffer.resize(block.object_links.storage_size);
                seek(istream, prev_offset, std::ios::beg);
                read(istream, data_transfer_buffer.data(), data_transfer_buffer.size());
                write(ostream, data_transfer_buffer.data(), data_transfer_buffer.size());
            }

            else
            {
                for (size_t j = 0; j < block.object_links.size(); ++j)
                {
                    write<uint8_t>(ostream, 5);

                    var_write<uint32_t>(ostream,
                        (block.object_links[j].parent_offset_flag) |
                        (block.object_links[j].parent_field_offset << 1) |
                        (block.object_links[j].parent_obj_offset << 1));

                    var_write<uint32_t>(ostream, 1);
                    if (block.elem_count > 1)
                    {
                        var_write<uint32_t>(ostream, block.object_links[j].parent_obj_ID);
                    }
                    var_write<uint32_t>(ostream, block.object_links[j].shared_data_ID + 1);
                    if (block.object_links[j].shared_data_ID + 1 == 0)
                    {
                        var_write<uint32_t>(ostream, block.object_links[j].obj_ID);
                        var_write<uint32_t>(ostream, block.object_links[j].obj_block_ID);
                        var_write<uint32_t>(ostream, block.object_links[j].obj_offset);
                    }
                    var_write<uint32_t>(ostream, block.object_links[j].obj_array_count);
                }
                size_t size = tell(ostream) - block.object_links.offset;
                block.object_links.storage_size = tell(ostream) - block.object_links.offset;
            }
            header.object_link_count += block.object_links.size();
            header.object_link_size += block.object_links.storage_size;
        }

        for (uint32_t i = 0; i < header.object_block_count; ++i)
        {
            ObjectBlock& block = object_blocks[i];

            int64_t prev_offset = block.array_links.offset;
            block.array_links.offset = tell(ostream);

            if (block.array_links.size() == 0)
            {
                continue;
            }
            if (!is_modified(modified_block_IDs, i))
            {
                data_transfer_buffer.resize(block.array_links.storage_size);
                seek(istream, prev_offset, std::ios::beg);
                read(istream, data_transfer_buffer.data(), data_transfer_buffer.size());
                write(ostream, data_transfer_buffer.data(), data_transfer_buffer.size());
            }
            else
            {
                for (size_t j = 0; j < block.array_links.size(); ++j)
                {
                    write<uint8_t>(ostream, 5);
                    var_write<uint32_t>(ostream, 
                        (block.object_links[j].parent_offset_flag) |
                        (block.object_links[j].parent_field_offset << 1) |
                        (block.object_links[j].parent_obj_offset << 1));

                    var_write<uint32_t>(ostream, 1);
                    if (block.elem_count > 1)
                    {
                        var_write<uint32_t>(ostream, block.array_links[j].parent_obj_ID);
                    }
                    var_write<uint32_t>(ostream, block.array_links[j].count);
                    var_write<uint32_t>(ostream, block.array_links[j].offset);
                }

                block.array_links.storage_size = ostream.tellp() - block.array_links.offset;
            }
            header.array_link_count += block.array_links.size();
            header.array_link_size += block.array_links.storage_size;
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool ModifyBlockHeaders(std::ostream& ostream, const PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs)
{
    try
    {
        for (size_t i = 0; i < object_blocks.size(); ++i)
        {
            ObjectBlock& block = object_blocks[i];

            if (!is_modified(modified_block_IDs, i))
            {
                seek(ostream, 36, std::ios::cur);
                continue;
            }

            seek(ostream, header.header_size + header.namespace_size + i * 36 + 4, std::ios::beg);
            write<uint32_t>(ostream, object_blocks[i].elem_count);
            write<uint32_t>(ostream, object_blocks[i].data_size);
            write<uint32_t>(ostream, object_blocks[i].objects_size);
            write<uint32_t>(ostream, object_blocks[i].arrays_size);
            seek(ostream, 4, std::ios::cur);
            write<uint32_t>(ostream, object_blocks[i].array_links.size());
            write<uint32_t>(ostream, object_blocks[i].object_links.size());
        }
    }
    catch (const std::exception& e)
    {
        const auto ec = std::error_code{ errno, std::system_category() };
        gbl_err << "ERROR: " << e.what() << " " << ec.message() << std::endl;
        return false;
    }
    return true;
}

void ObjectBlock::save_data_bytes(std::istream& stream, const std::string& file_path)
{
    try 
    {
        int64_t prev_pos = tell(stream);

        std::vector<char> data;
        data.resize(data_size);

        seek(stream, data_offset, std::ios::beg);
        read(stream, data.data(), data_size);

        std::ofstream ostream(file_path);
        write(ostream, data.data(), data_size);

        seek(stream, prev_pos, std::ios::beg);
    }
    catch (const std::exception& e)
    {
        gbl_err << "Failed to save_data_bytes on " << name << " - " << e.what(); 
    }
}

void ObjectBlock::save_elem_bytes(std::istream& stream, const std::string& file_path)
{
    try
    {
        int64_t prev_pos = tell(stream);

        std::vector<char> data;
        data.resize(elem_size);

        for (int i = 0; i < elem_count; ++i)
        {
            seek(stream, data_offset + i * elem_size, std::ios::beg);
            read(stream, data.data(), elem_size);

            std::ofstream ostream(file_path + std::to_string(i));
            write(ostream, data.data(), elem_size);
        }

        seek(stream, prev_pos, std::ios::beg);
    }
    catch (const std::exception& e)
    {
        gbl_err << "Failed to save_data_bytes on " << name << " - " << e.what();
    }
}

ObjectBlock* FindObjectBlock(const std::vector<ObjectBlock>& object_blocks, const std::string& name)
{
    for (const ObjectBlock& object_block : object_blocks)
    {
        if (object_block.name == name)
        {
            return (ObjectBlock*)&object_block;
        }
    }
    return nullptr;
}

int32_t ObjectBlockID(const std::vector<ObjectBlock>& object_blocks, const std::string& oblock_name)
{
    for (size_t i = 0; i < object_blocks.size(); ++i)
    {
        if (object_blocks[i].name == oblock_name)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

std::vector<ObjectBlock*> FindObjectBlocks(const std::vector<ObjectBlock>& object_blocks, const std::string& name)
{
    std::vector<ObjectBlock*> lists;
    lists.reserve(1024);

    for (const ObjectBlock& object_block : object_blocks)
    {
        if (object_block.name == name)
        {
            lists.push_back((ObjectBlock*)&object_block);
        }
    }
    return lists;
}


std::vector<ObjectLink*> FindMemberObjectLinks(const std::vector<ObjectBlock>& object_blocks, const ObjectBlock& object_block, const std::string& member_oblock_name)
{
    std::vector<ObjectLink*> member_links;
    member_links.reserve(object_block.object_links.size());

    for (ObjectBlock* oblock : FindObjectBlocks(object_blocks, member_oblock_name))
    {
        uint32_t oblock_ID = uint32_t(oblock - object_blocks.data());
        for (uint32_t i = 0; i < object_block.object_links.size(); ++i)
        {
            if (object_block.object_links[i].obj_block_ID == oblock_ID)
            {
                member_links.push_back((ObjectLink*)&object_block.object_links[i]);
            }
        }
    }

    return member_links;
}

std::vector<ObjectLink*> FindParentsObjectLinks(const std::vector<ObjectLink*>& object_links, const uint32_t parent_obj_ID)
{
    std::vector<ObjectLink*> parents_links;
    parents_links.reserve(object_links.size());
    for (ObjectLink* olink : object_links)
    {
        if (olink->parent_obj_ID == parent_obj_ID)
        {
            parents_links.push_back(olink);
        }
    }   
    return parents_links;
}

std::vector<ObjectLink*> FindParentsObjectLinks(const std::vector<ObjectLink>& object_links, const uint32_t parent_obj_ID)
{
    std::vector<ObjectLink*> parents_links;
    parents_links.reserve(object_links.size());
    for (const ObjectLink& olink : object_links)
    {
        if (olink.parent_obj_ID == parent_obj_ID)
        {
            parents_links.push_back((ObjectLink*)&olink);
        }
    }
    return parents_links;
}

std::vector<ArrayLink*> FindParentsArrayLinks(const std::vector<ArrayLink*>& array_links, const uint32_t parent_obj_ID)
{
    std::vector<ArrayLink*> parents_links;
    parents_links.reserve(array_links.size());
    for (ArrayLink* alink : array_links)
    {
        if (alink->parent_obj_ID == parent_obj_ID)
        {
            parents_links.push_back(alink);
        }
    }
    return parents_links;
}

std::vector<ArrayLink*> FindParentsArrayLinks(const std::vector<ArrayLink>& array_links, const uint32_t parent_obj_ID)
{
    std::vector<ArrayLink*> parents_links;
    parents_links.reserve(array_links.size());
    for (const ArrayLink& alink : array_links)
    {
        if (alink.parent_obj_ID == parent_obj_ID)
        {
            parents_links.push_back((ArrayLink*)&alink);
        }
    }
    return parents_links;
}