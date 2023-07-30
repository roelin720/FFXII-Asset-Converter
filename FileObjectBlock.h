#pragma once
#include "Enums.h"
#include <istream>
#include <string>
#include <vector>

struct PhyreHeader;

struct FileLink
{
    uint32_t parent_obj_ID = 0;
    uint32_t parent_obj_offset = 0;
    uint32_t parent_field_offset = 0;
    uint32_t parent_offset_flag = 0;
};

struct ArrayLink : public FileLink
{
    uint32_t offset = 0;
    uint32_t count = 0;
};

struct ObjectLink : public FileLink
{
    uint32_t obj_ID = 0;
    uint32_t obj_offset = 0;
    uint32_t obj_block_ID = 0;
    uint32_t obj_array_count = 0;
    uint32_t shared_data_ID = 0;
};

template<typename _LinkType, const char* _name>
struct LinkGroup : public std::vector<_LinkType>
{
    using LinkType = _LinkType;

    const char* name = _name;
    int64_t offset = 0;
    int64_t storage_size = 0;
};

struct ObjectBlock
{
private:
    static constexpr char a_link_name[] = "array";
    static constexpr char o_link_name[] = "object";
public:
    int64_t name_id = 0;
    int64_t elem_count = 0;
    int64_t data_size = 0;
    int64_t objects_size = 0;
    int64_t arrays_size = 0;
    int64_t data_offset = 0;
    int64_t elem_size = 0;
    std::string name;

    LinkGroup<ObjectLink, o_link_name> object_links;
    LinkGroup<ArrayLink, a_link_name> array_links;

    template<typename T>
    auto& link_group() {}
    template<> auto& link_group<ObjectLink>() { return object_links; }
    template<> auto& link_group<ArrayLink>() { return array_links; }

    void save_data_bytes(std::istream& stream, const std::string& file_path);
    void save_elem_bytes(std::istream& stream, const std::string& file_path);
};

bool LoadObjectBlocks(std::istream& stream, std::vector<ObjectBlock>& object_blocks);

//instead of remaking all the links which may be slow, complicated and might increase file sizes
//we just change the modified ones and make adjustments to offsets for the rest

bool ModifyBlockData(std::ostream& ostream, std::istream& istream, PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs, const std::vector<std::vector<char>>& modified_block_data);
bool ModifyBlockLinks(std::ostream& ostream, std::istream& istream, PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs);
bool ModifyBlockHeaders(std::ostream& ostream, const PhyreHeader& header, std::vector<ObjectBlock>& object_blocks, const std::vector<uint32_t>& modified_block_IDs);

int32_t ObjectBlockID(const std::vector<ObjectBlock>& object_blocks, const std::string& oblock_name);
ObjectBlock* FindObjectBlock(const std::vector<ObjectBlock>& object_blocks, const std::string& oblock_name);
std::vector<ObjectBlock*> FindObjectBlocks(const std::vector<ObjectBlock>& object_blocks, const std::string& oblock_name);

std::vector<ObjectLink*> FindMemberObjectLinks(const std::vector<ObjectBlock>& object_blocks, const ObjectBlock& object_block, const std::string& member_oblock_name);
std::vector<ObjectLink*> FindParentsObjectLinks(const std::vector<ObjectLink*>&object_links, const uint32_t parent_obj_ID);
std::vector<ObjectLink*> FindParentsObjectLinks(const std::vector<ObjectLink>& object_links, const uint32_t parent_obj_ID);
std::vector<ArrayLink*> FindParentsArrayLinks(const std::vector<ArrayLink*>& object_links, const uint32_t parent_obj_ID);
std::vector<ArrayLink*> FindParentsArrayLinks(const std::vector<ArrayLink>& object_links, const uint32_t parent_obj_ID);

