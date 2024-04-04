#pragma once
#include "3rdParty/md5/md5.h"
#include <filesystem>
#include <ostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <map>

class VBFArchive
{
public:
    struct md5hash
    {
        uint64_t data[2];

        md5hash();
        md5hash(MD5& md5hasher);
        md5hash(MD5& md5hasher, const std::string& input);

        auto operator<=>(const md5hash&) const = default;
    };
    static_assert(sizeof(md5hash) == 16, "sizeof(md5hash) == 16");

    struct Block
    {
        uint64_t offset = 0;
        uint64_t size = 0;

        auto operator<=>(const Block&) const = default;
    };

    struct Mapping
    {
        uint64_t src_offset;
        uint64_t dst_offset;
        uint64_t size;

        constexpr Block sri_block() const { return Block{ .offset = src_offset, .size = size }; }
        constexpr Block dst_block() const { return Block{ .offset = dst_offset, .size = size }; }

        auto operator<=>(const Mapping&) const = default;
    };

    struct TreeNode;
    struct File
    {
        TreeNode* node = nullptr;
        md5hash name_hash;
        std::string name;
        uint64_t block_list_offset = 0;
        uint64_t data_offset = 0;
        uint64_t uncomp_size = 0;
        uint64_t data_size = 0;
        std::vector<Block> blocks;
    };

    struct TreeNode
    {
        std::string name_segment;
        File* file = nullptr;

        TreeNode* parent = nullptr;
        std::vector<TreeNode*> children;

        std::string full_name() const;
    };
    std::vector<TreeNode> tree;

    std::string arc_path;
    std::filesystem::file_time_type arc_timestamp;  

    std::vector<File> files;
    int64_t name_block_size = 0;

    std::vector<Block> unused_data;
    std::vector<Block> unused_block_lists;

    bool load(const std::string& path);
    bool load_tree();
    bool extract(const File& file, const std::filesystem::path& out_path) const;
    bool inject(File& file, const std::filesystem::path& in_path);
    bool update_header();

    bool extract_all(const VBFArchive::TreeNode& node, const std::string& folder, bool* stop_early = nullptr) const;
    bool inject_all(VBFArchive::TreeNode& node, const std::string& folder, bool* stop_early = nullptr);

    const File* find_file(const std::string& file_name) const;
    File* find_file(const std::string& file_name);

    const TreeNode* find_node(const std::string& file_name) const;
    TreeNode* find_node(const std::string& file_name);

    bool unmodified() const;
    bool signal_modified();
};

namespace VBFUtils
{
    bool Separate(const std::string& arc_file_path, std::string& arc_path, std::string& file_name);
}

inline std::ostream& operator<<(std::ostream& out, const VBFArchive::File& val)
{
    out << "name: " << val.name << std::endl;
    out << "name_hash: " << 
        std::setfill('0') << std::setw(8) << std::right << std::hex <<  val.name_hash.data[0] <<
        std::setfill('0') << std::setw(8) << std::right << std::hex << val.name_hash.data[1] << std::endl;
    out << "block_list_offset: " << std::dec << std::setw(0) << val.block_list_offset << std::endl;
    out << "data_offset: " << val.data_offset << std::endl;
    out << "uncomp_size: " << (
        (val.uncomp_size >> 30) ? std::to_string(val.uncomp_size >> 30) + "GB" :
        (val.uncomp_size >> 20) ? std::to_string(val.uncomp_size >> 20) + "MB" :
        (val.uncomp_size >> 10) ? std::to_string(val.uncomp_size >> 10) + "KB" :
                                         std::to_string(val.uncomp_size)       + "B") 
        << std::endl;
    out << "block_count: " << val.blocks.size() << std::endl;
    return out;
}