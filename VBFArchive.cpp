#include "VBFArchive.h"
#include "ConverterInterface.h"
#include "FileIOUtils.h"
#include "assimp/../../contrib/zlib/zlib.h"
#include "assimp/../../contrib/zlib/deflate.h"
#include <filesystem>
#include <execution>
#include <algorithm>

using namespace IO;
namespace IO
{
	static MD5 md5hasher;

	template <> inline VBFArchive::md5hash read<VBFArchive::md5hash>(std::istream& stream)
	{
		VBFArchive::md5hash val;
		stream.read((char*)&val, 16);
		return val;
	}

	template <> inline void write<VBFArchive::md5hash>(std::ostream& stream, const VBFArchive::md5hash& val)
	{
		stream.write((char*)&val, 16);
	}
}

namespace
{
	constexpr uint32_t uint16_max = std::numeric_limits<uint16_t>::max();
	constexpr uint32_t uint16_range = uint16_max + 1;
	constexpr uint64_t chunk_size_max = uint16_range;
	static_assert(chunk_size_max >= uint16_range, "chunk_size_max >= uint16_range");

	std::vector<char> chunk_buffer = std::vector<char>(chunk_size_max, 0);

	constexpr auto order_by_size = [](const VBFArchive::Block& a, const VBFArchive::Block& b)
	{
		return a.size < b.size;
	};

	VBFArchive::Block realloc(std::vector<VBFArchive::Block>& unused_blocks, VBFArchive::Block block, uint64_t new_size)
	{
		VBFArchive::Block new_block{ .offset = block.offset, .size = new_size };
		VBFArchive::Block freed_block{};

		if (new_size > block.size)
		{
			auto b_it = std::upper_bound(unused_blocks.begin(), unused_blocks.end(), block);
			if (b_it != unused_blocks.end() && block.offset + block.size == b_it->offset)
			{
				int64_t increase = new_size - block.size;
				b_it->size -= increase;
				b_it->offset += increase;
				if (b_it->size == 0)
				{
					unused_blocks.erase(b_it);
				}
				return new_block;
			}

			std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end(), order_by_size);
			b_it = std::upper_bound(unused_blocks.begin(), unused_blocks.end(), new_block, order_by_size);
			if (b_it == unused_blocks.end())
			{
				std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end());
				return VBFArchive::Block();
			}
			new_block.offset = b_it->offset;
			b_it->size -= new_size;
			b_it->offset += new_size;
			freed_block = block;
		}
		else if (new_size < block.size)
		{
			new_block.offset = block.offset;
			freed_block.size = block.size - new_size;
			freed_block.offset = block.offset + new_size;
		}

		if (freed_block.size || freed_block.offset)
		{
			unused_blocks.push_back(freed_block);
			std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end());

			auto it = std::lower_bound(unused_blocks.begin(), unused_blocks.end(), freed_block);
			if (it + 1 < unused_blocks.end() && it->offset + it->size == (it + 1)->offset)
			{
				it->size += (it + 1)->size;
				unused_blocks.erase(it + 1);
			}
			if (it > unused_blocks.begin() && (it - 1)->offset + (it - 1)->size == it->offset)
			{
				(it - 1)->size += it->size;
				unused_blocks.erase(it);
			}
		}

		return new_block;
	}

	VBFArchive::Block alloc(std::vector<VBFArchive::Block>& unused_blocks, uint64_t size)
	{
		VBFArchive::Block new_block{ .offset = 0, .size = size };

		std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end(), order_by_size);
		auto b_it = std::upper_bound(unused_blocks.begin(), unused_blocks.end(), new_block, order_by_size);
		if (b_it == unused_blocks.end())
		{
			std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end());
			return VBFArchive::Block();
		}

		new_block.offset = b_it->offset;
		b_it->size -= size;
		b_it->offset += size;
		if (b_it->size == 0)
		{
			unused_blocks.erase(b_it);
		}
		std::sort(std::execution::par_unseq, unused_blocks.begin(), unused_blocks.end());

		return new_block;
	}

	void get_unused(std::vector<VBFArchive::Block>& used, std::vector<VBFArchive::Block>& unused, uint64_t start, uint64_t end)
	{
		std::sort(std::execution::par_unseq, used.begin(), used.end());
		unused.reserve(1024);

		uint64_t block_pos = start;
		for (uint64_t i = 0; i < used.size(); ++i)
		{
			uint64_t diff = used[i].offset - block_pos;
			if (diff > 0)
			{
				unused.push_back(VBFArchive::Block{
					.offset = block_pos,
					.size = diff
				});
			}
			block_pos = used[i].offset + used[i].size;
		}
		if (end > block_pos)
		{
			unused.push_back(VBFArchive::Block{
				.offset = block_pos,
				.size = end - block_pos
			});
		}
	}

	void transfer_data_chunked(std::istream& istream, std::ostream& ostream, const VBFArchive::Mapping& mapping)
	{
		for (uint64_t chunk_offset = 0; chunk_offset < mapping.size;)
		{
			uint64_t chunk_size = std::min(mapping.size - chunk_offset, chunk_size_max);
		
			seekg(istream, mapping.src_offset + chunk_offset, std::ios::beg);
			read(istream, chunk_buffer.data(), chunk_size);
			seekp(ostream, mapping.dst_offset + chunk_offset, std::ios::beg);
			write(ostream, chunk_buffer.data(), chunk_size);
		
			chunk_offset += chunk_size;
		}
	}

	VBFArchive::md5hash hash_data_chunked(std::istream& istream, const VBFArchive::Block& block)
	{
		md5hasher.reset();

		for (uint64_t chunk_offset = 0; chunk_offset < block.size;)
		{
			uint64_t chunk_size = std::min(block.size - chunk_offset, chunk_size_max);

			seekg(istream, block.offset + chunk_offset, std::ios::beg);
			read(istream, chunk_buffer.data(), chunk_size);
			md5hasher.add(chunk_buffer.data(), chunk_size);

			chunk_offset += chunk_size;
		}
		return VBFArchive::md5hash(md5hasher);
	}

	bool compare(const std::string& a, const std::string& b)
	{
		if (strlen(a.c_str()) != strlen(b.c_str()))
		{
			return false;
		}
		for (int i = 0; i < a.size(); ++i)
		{
			if (std::tolower(a[i]) != std::tolower(b[i]))
			{
				return false;
			}
		}

		return true;
	}
}

VBFArchive::md5hash::md5hash() : data{ 0, 0 } {}

VBFArchive::md5hash::md5hash(MD5& md5hasher) : data{0, 0}
{
	md5hasher.getHash((unsigned char*)this);
}

VBFArchive::md5hash::md5hash(MD5& md5hasher, const std::string& input) : data{ 0, 0 }
{
	md5hasher(input);
	md5hasher.getHash((unsigned char*)this);
}

bool VBFArchive::load(const std::string& arc_path)
{
	try
	{
		files.clear();

		this->arc_path = arc_path;
		int64_t file_size = std::filesystem::file_size(arc_path);

		std::ifstream stream(arc_path, std::ios::binary);
		if (file_size == 0 || !stream.good())
		{
			LOG(ERR) << "VBF: Failed to open file " + arc_path << std::endl;
			return false;
		}
		constexpr char magic_bytes[] = "SRYK";
		if (read<int32_t>(stream) != *((int32_t*)magic_bytes))
		{
			LOG(ERR) << "VBF: magic bytes are incorrect" << std::endl;
			return false;
		}
		int64_t header_len = read<int32_t>(stream);
		int64_t file_count = read<int64_t>(stream);
		seek(stream, 16 + file_count * 48, std::ios::beg);
		name_block_size = read<int32_t>(stream);

		//md5hash header_hash = hash_data_chunked(stream, Block{ .offset = 0, .size = (uint64_t)header_len });
		//seek(stream, -16, std::ios::end);
		//md5hash read_header_hash = read<md5hash>(stream);
		//if (read_header_hash != header_hash)
		//{
		//	__debugbreak();
		//}

		files.resize(file_count);

		std::vector<Block> used_blocks;
		std::vector<Block> used_block_indices;
		used_blocks.reserve((header_len - (16 + files.size() * 48 + name_block_size)) / 2);
		used_block_indices.resize(files.size());

		bool reorder_files = false;

		for (size_t i = 0; i < files.size(); ++i)
		{
			seek(stream, 16 + i * 16, std::ios::beg);
			files[i].name_hash = read<VBFArchive::md5hash>(stream);

			seek(stream, 16 + files.size() * 16 + i * 32, std::ios::beg);

			files[i].block_list_offset = read<uint32_t>(stream) * 2;
			seek(stream, 4, std::ios::cur);
			files[i].uncomp_size = read<uint64_t>(stream);
			files[i].data_offset = read<uint64_t>(stream);
			uint64_t name_offset = read<uint64_t>(stream);

			seek(stream, 16 + files.size() * 48 + 4 + name_offset, std::ios::beg);
			files[i].name = read<std::string>(stream);

			files[i].blocks.resize((files[i].uncomp_size / uint16_range) + !!(files[i].uncomp_size % uint16_range));
			seek(stream, 16 + files.size() * 48 + name_block_size + files[i].block_list_offset, std::ios::beg);

			if (!files[i].blocks.empty())
			{
				files[i].blocks[0].size = (((uint16_range + read<uint16_t>(stream)) - 1) % uint16_range) + 1;
				files[i].blocks[0].offset = files[i].data_offset;
				files[i].data_size += files[i].blocks[0].size;
				used_blocks.push_back(files[i].blocks[0]);
			}

			for (uint64_t j = 1; j < files[i].blocks.size(); ++j)
			{
				files[i].blocks[j].size = (((uint16_range + read<uint16_t>(stream)) - 1) % uint16_range) + 1;
				files[i].blocks[j].offset = files[i].blocks[j - 1].offset + files[i].blocks[j - 1].size;
				files[i].data_size += files[i].blocks[j].size;
				used_blocks.push_back(files[i].blocks[j]);
			}

			used_block_indices[i] = Block{
				.offset = files[i].block_list_offset,
				.size = files[i].blocks.size() * 2
			};

			reorder_files |= i != 0 && files[i].block_list_offset < files[i - 1].block_list_offset;
		}

		if (reorder_files)
		{
			std::sort(std::execution::par_unseq, files.begin(), files.end(), [](const File& a, const File& b)
				{
					return a.block_list_offset < b.block_list_offset;
				});
		}

		get_unused(used_blocks, unused_data, header_len, file_size - 16);
		get_unused(used_block_indices, unused_block_lists, 0, files.back().block_list_offset + files.back().blocks.size() * 2);

		assert(!used_blocks.empty());
		unused_data.push_back(Block{
			.offset = used_blocks.back().offset + used_blocks.back().size,
			.size = UINT64_MAX >> 2 //represents infinity
		});

		std::sort(std::execution::par_unseq, unused_data.begin(), unused_data.end());
		std::sort(std::execution::par_unseq, unused_block_lists.begin(), unused_block_lists.end());

		signal_modified();

		return load_tree();
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF: ") + e.what() + " " + ec.message() << std::endl;
		return false;
	}

	return true;
}

bool VBFArchive::update_header()
{
	LOG(INFO) << "Updating vbf header for " << arc_path << std::endl;

	if (arc_path.empty() || files.empty())
	{
		LOG(ERR) << "VBF: No vbf has been loaded" << std::endl;
		return false;
	}
	if (!unmodified())
	{
		LOG(ERR) << "VBF: vbf has been externally modified. Please reload" << std::endl;
		return false;
	}

	uint64_t header_len = 16 + files.size() * 48 + name_block_size + files.back().block_list_offset + files.back().blocks.size() * 2;

	try
	{
		std::fstream stream(arc_path, std::ios::binary | std::ios::in | std::ios::out);
		if (!stream.good())
		{
			LOG(ERR) << "VBF: Failed to open vbf - " + arc_path << std::endl;
			return false;
		}
		assert(name_block_size > 0);

		uint64_t name_offset = 0;

		for (size_t i = 0; i < files.size(); ++i)
		{
			seekp(stream, 16 + i * 16, std::ios::beg);
			write<VBFArchive::md5hash>(stream, files[i].name_hash);

			seekp(stream, 16 + files.size() * 16 + i * 32, std::ios::beg);

			write<uint32_t>(stream, files[i].block_list_offset / 2);
			write<uint32_t>(stream, 0);
			write<uint64_t>(stream, files[i].uncomp_size);
			write<uint64_t>(stream, files[i].data_offset);
			write<uint64_t>(stream, name_offset);

			uint64_t name_size = std::strlen(files[i].name.c_str()) + 1;
			seekp(stream, 16 + files.size() * 48 + 4 + name_offset, std::ios::beg);
			write(stream, files[i].name.data(), name_size);
			name_offset += name_size;

			seekp(stream, 16 + files.size() * 48 + name_block_size + files[i].block_list_offset, std::ios::beg);

			for (uint64_t j = 0; j < files[i].blocks.size(); ++j)
			{
				write<uint16_t>(stream, files[i].blocks[j].size % uint16_range);
			}
		}

		seekp(stream, 4, std::ios::beg);
		write<int32_t>(stream, header_len);

		stream.flush();
		md5hash header_hash = hash_data_chunked(stream, Block{ .offset = 0, .size = header_len });

		seekp(stream, -16, std::ios::end);
		write<md5hash>(stream, header_hash);

		stream.close();
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF: ") + e.what() + " " + ec.message() + "VBF IS LIKELY CORRUPT. PLEASE RESTORE FROM BACKUP" << std::endl;
		return false;
	}
	signal_modified();

	return true;
}

bool VBFArchive::extract(const File& file, const std::filesystem::path& out_path) const
{
	LOG(INFO) << "Extracting " << file.name << " " << out_path << std::endl;

	if (arc_path.empty() || files.empty())
	{
		LOG(ERR) << "VBF: No vbf has been loaded" << std::endl;
		return false;
	}
	if (!unmodified())
	{
		LOG(ERR) << "VBF: vbf has been externally modified. Please reload" << std::endl;
		return false;
	}

	try
	{
		std::ifstream istream(arc_path, std::ios::binary);
		if (!istream.good())
		{
			LOG(ERR) << "VBF: Failed to open vbf file - " << arc_path << std::endl;
			return false;
		}
		std::ofstream ostream(out_path, std::ios::binary);
		if (!ostream.good())
		{
			LOG(ERR) << "VBF: Failed to open output file - " << out_path << std::endl;
			return false;
		}

		uLongf decomp_size = 0;
		std::vector<char> decomp_block_data;
		decomp_block_data.resize(file.uncomp_size);

		std::vector<char> raw_block_data;
		raw_block_data.reserve(uint16_range);

		seek(istream, file.data_offset, std::ios::beg);

		for (int32_t i = 0; i < file.blocks.size(); ++i)
		{
			raw_block_data.resize(file.blocks[i].size);

			read(istream, raw_block_data.data(), raw_block_data.size());

			if (raw_block_data.size() == uint16_range || (i == file.blocks.size() - 1 && raw_block_data.size() == file.uncomp_size % uint16_range))
			{
				write(ostream, raw_block_data.data(), raw_block_data.size());
				continue;
			}

			decomp_size = (uLongf)decomp_block_data.size();
			int result = uncompress((Bytef*)decomp_block_data.data(), &decomp_size, (Bytef*)raw_block_data.data(), (uLongf)raw_block_data.size());

			switch (result)
			{
			case Z_MEM_ERROR:
				LOG(ERR) << "Failed to decompress block " + std::to_string(i) + " of file (out of memory) " + file.name << std::endl;
				return false;
			case Z_BUF_ERROR:
				LOG(ERR) << "Failed to decompress block " + std::to_string(i) + " of file (comp_buffer is too small) " + file.name << std::endl;
				return false;
			case Z_DATA_ERROR:
				LOG(ERR) << "Failed to decompress block " + std::to_string(i) + " of file (data is corrupt) " + file.name << std::endl;
				return false;
			}
			if (result != Z_OK)
			{
				LOG(ERR) << "Failed to decompress block " + std::to_string(i) + " of file (unknown error) " + file.name << std::endl;
				return false;
			}
			write(ostream, decomp_block_data.data(), decomp_size);
		}
		return true;
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF: ") + e.what() + " " + ec.message() << std::endl;
		return false;
	}
	return true;
}

bool VBFArchive::inject(File& file, const std::filesystem::path& src_path)
{
	LOG(INFO) << "Injecting into " << file.name << " from " << src_path << std::endl;

	if (arc_path.empty() || files.empty())
	{
		LOG(ERR) << "VBF: No vbf has been loaded" << std::endl;
		return false;
	}
	if (!unmodified())
	{
		LOG(ERR) << "VBF: vbf has been externally modified. Please reload" << std::endl;
		return false;
	}

	std::string	stage_path = IO::TmpPath() + "/vbf_stage.tmp";
	std::vector<Mapping> stage_map;
	stage_map.reserve(256);

	try
	{
		std::ofstream stage(stage_path, std::ios::binary);
		if (!stage.good())
		{
			IO::CancelWrite(stage_path);
			LOG(ERR) << "VBF: Failed to open vbf stage file - " + stage_path << std::endl;
			return false;
		}

		uint64_t src_size = (uint64_t)std::filesystem::file_size(src_path);
		if (src_size == 0)
		{
			IO::CancelWrite(stage_path, &stage);
			LOG(ERR) << "VBF: Failed to get file size/file is empty - " << src_path << std::endl;
			return false;
		}

		std::ifstream src_stream(src_path, std::ios::binary);
		if (!src_stream.good())
		{
			IO::CancelWrite(stage_path, &stage);
			LOG(ERR) << "VBF: Failed to open file - " << src_path << std::endl;
			return false;
		}

		std::vector<char> uncomp_buffer;
		std::vector<char> comp_buffer;
		uint64_t total_comp_size = 0;

		uncomp_buffer.resize(uint16_range);
		comp_buffer.resize(compressBound(uint16_range));

		uint64_t prev_block_count = file.blocks.size();
		file.blocks.clear();
		file.blocks.reserve(src_size / uint16_max);

		LOG(INFO) << "Compressing files" << std::endl;

		for (uint64_t uncomp_offset = 0; uncomp_offset < src_size;)
		{
			uint64_t uncomp_size = std::min(src_size - uncomp_offset, uint64_t(uint16_range));
			uLongf comp_size = (uLongf)comp_buffer.size();

			seek(src_stream, uncomp_offset, std::ios::beg);
			read(src_stream, uncomp_buffer.data(), uncomp_size);

			int result = compress((Bytef*)comp_buffer.data(), &comp_size, (Bytef*)uncomp_buffer.data(), (uLongf)uncomp_size);
			if (result != Z_OK || comp_size >= uint16_range || (total_comp_size + comp_size >= src_size && comp_size == (src_size % uint16_range)))
			{
				total_comp_size = 0;
				file.blocks.clear();
				seekp(stage, 0, std::ios::beg);
				break;
			}

			write(stage, comp_buffer.data(), comp_size);

			file.blocks.push_back(Block{
				.offset = total_comp_size,
				.size = comp_size
			});

			uncomp_offset += uncomp_size;
			total_comp_size += comp_size;
		}

		if (total_comp_size == 0)
		{
			LOG(INFO) << "Storing data uncompressed" << std::endl;

			file.blocks.reserve((src_size / uint16_range) + 1);
			for (uint64_t chunk_offset = 0; chunk_offset < src_size;)
			{
				uint64_t chunk_size = std::min(uint64_t(src_size) - chunk_offset, uint64_t(uint16_range));

				write(stage, comp_buffer.data(), chunk_size);
				file.blocks.push_back(Block{
					.offset = chunk_offset,
					.size = chunk_size
				});

				chunk_offset += chunk_size;
			}
		}

		uint64_t src_storage_size = total_comp_size ? total_comp_size : src_size;

		std::ifstream arc_stream(arc_path, std::ios::binary);
		if (!arc_stream.good())
		{
			IO::CancelWrite(stage_path, &stage);
			LOG(ERR) << "VBF: Failed to open vbf - " + arc_path << std::endl;
			return false;
		}

		bool reorder_files = false;

		assert(!file.blocks.empty());
		if (prev_block_count != file.blocks.size())
		{
			uint64_t header_len = 16 + files.size() * 48 + name_block_size + files.back().block_list_offset + files.back().blocks.size() * 2;
			Block block_list = realloc(unused_block_lists, Block{ .offset = file.block_list_offset, .size = prev_block_count * 2 }, file.blocks.size() * 2);

			if (block_list.size != 0)
			{
				reorder_files = file.block_list_offset != block_list.offset;

				file.block_list_offset = block_list.offset;
			}
			else if (unused_data.front().offset == header_len && unused_data.front().size >= file.blocks.size() * 2)
			{
				reorder_files = file.block_list_offset != unused_data.front().offset;
				file.block_list_offset = files.back().block_list_offset + files.back().blocks.size() * 2;
				unused_data.front().offset += file.blocks.size() * 2;
				unused_data.front().size -= file.blocks.size() * 2;
				if (unused_data.front().size == 0)
				{
					unused_data.erase(unused_data.begin());
				}
			}
			else
			{
				assert(block_list.offset == 0);
				block_list.offset = -1;
				//block_list.offset = files.back().block_list_offset + files.back().blocks.size() * 2;

				while (block_list.size < file.blocks.size() * 2)
				{
					File* front_file = &files.front();
					for (File& file : files)
					{
						if (file.data_offset < front_file->data_offset) [[unlikely]]
						{
							front_file = &file;
						}
					}
					LOG(INFO) << "Shifting file data to end for block-list space " << front_file->name << std::endl;

					Block new_data_block = alloc(unused_data, front_file->data_size);
					assert(new_data_block.offset > front_file->data_offset && new_data_block.size == front_file->data_size);

					block_list.offset = std::min(block_list.offset, front_file->data_offset - (16 + files.size() * 48 + name_block_size));
					block_list.size = (front_file->data_offset + front_file->data_size) - header_len;

					uint64_t shift = (int64_t)new_data_block.offset - (int64_t)front_file->data_offset;

					for (int i = 0; i < front_file->blocks.size(); ++i)
					{
						front_file->blocks[i].offset += shift;
					}

					stage_map.push_back(Mapping{
						.src_offset = (uint64_t)stage.tellp(),
						.dst_offset = new_data_block.offset,
						.size = front_file->data_size
					});

					transfer_data_chunked(arc_stream, stage, Mapping{
						.src_offset = front_file->data_offset,
						.dst_offset = (uint64_t)stage.tellp(),
						.size = front_file->data_size
					});

					front_file->data_offset = new_data_block.offset;
				}

				reorder_files = file.block_list_offset != block_list.offset;
				file.block_list_offset = block_list.offset;
				block_list.size -= file.blocks.size() * 2;
				block_list.offset += file.blocks.size() * 2;
				if (block_list.size != 0)
				{
					unused_block_lists.push_back(block_list);
					std::sort(std::execution::par_unseq, unused_block_lists.begin(), unused_block_lists.end());
				}
			}
		}

		if (src_storage_size != file.data_size)
		{
			Block data_block = realloc(unused_data, Block{ .offset = file.data_offset, .size = (uint64_t)file.data_size }, src_storage_size);
			assert(data_block.size == src_storage_size && data_block.offset != 0);
			file.data_size = src_storage_size;
			file.data_offset = data_block.offset;
		}

		file.uncomp_size = src_size;

		for (auto& block : file.blocks)
		{
			block.offset += file.data_offset;
		}

		stage_map.push_back(Mapping{
			.src_offset = 0,
			.dst_offset = file.data_offset,
			.size = file.data_size
		});

		if (reorder_files)
		{
			std::sort(std::execution::par_unseq, files.begin(), files.end(), [](const File& a, const File& b)
			{
				return a.block_list_offset < b.block_list_offset;
			});
		}
		stage.close();
		arc_stream.close();
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF: ") + e.what() + " " + ec.message() << std::endl;
		return false;
	}

	try
	{
		LOG(INFO) << "Comitting vbf changes" << std::endl;

		std::ifstream istream(stage_path, std::ios::binary);
		if (!istream.good())
		{
			std::remove(stage_path.c_str());
			LOG(ERR) << std::string("VBF: Failed to open vbf stage file - ") + stage_path << std::endl;
			return false;
		}
		std::fstream ostream(arc_path, std::ios::binary | std::ios::in | std::ios::out);
		if (!ostream.good())
		{
			std::remove(stage_path.c_str());
			LOG(ERR) << std::string("VBF: Failed to open vbf - ") + arc_path << std::endl;
			return false;
		}

		for (uint64_t i = 0; i < stage_map.size(); ++i)
		{
			transfer_data_chunked(istream, ostream, stage_map[i]);
		}

		istream.close();
		ostream.close();
		std::remove(stage_path.c_str());

		uint64_t new_arc_size = unused_data.back().offset + 16;
		std::filesystem::resize_file(arc_path, new_arc_size);
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF: ") + e.what() + " " + ec.message() + "VBF IS LIKELY CORRUPT. PLEASE RESTORE FROM BACKUP" << std::endl;
		return false;
	}

	signal_modified();

	return true;
}

bool VBFArchive::load_tree()
{
	std::string cache_path = "cache/vbftree3" + md5hasher(arc_path) + ".bin";

	try
	{
		if (files.empty())
		{
			LOG(ERR) << "VBF: No vbf has been loaded" << std::endl;
			return false;
		}

		std::ifstream stream(cache_path, std::ios::binary);

		if constexpr (false)
		if (std::filesystem::exists(cache_path) && stream.good() && std::filesystem::file_size(cache_path) > 8)
		{
			tree.resize(read<uint32_t>(stream));

			for (auto& node : tree)
			{
				node.name_segment.resize(read<uint32_t>(stream));
				read(stream, node.name_segment.data(), node.name_segment.size());
				int32_t parentID = read<int32_t>(stream);
				int32_t fileID = read<int32_t>(stream);
				node.parent = parentID >= 0 ? tree.data() + parentID : nullptr;
				node.file = fileID >= 0 ? files.data() + fileID : nullptr;
				size_t child_count = read<uint32_t>(stream);
				node.children.resize(child_count);
				for (uint32_t i = 0; i < child_count; ++i)
				{
					node.children[i] = tree.data() + read<uint32_t>(stream);
				}
				if (node.file)
				{
					node.file->node = &node;
				}
			}
			return true;
		}

		struct TrieNode
		{
			std::string name;
			std::list<TrieNode*> children;
			File* file = nullptr;

			~TrieNode()
			{
				for (TrieNode* child : children)
				{
					delete child;
				}
			}

			TreeNode* store(std::vector<TreeNode>& vbf_tree, TreeNode* parent) const
			{
				vbf_tree.push_back(TreeNode{ .name_segment = name, .parent = parent });

				TreeNode* vbf_node = &vbf_tree.back();

				if (file)
				{
					vbf_node->file = file;
					file->node = vbf_node;
				}
				vbf_node->children.reserve(children.size());
				for (auto* child : children)
				{
					vbf_node->children.push_back(child->store(vbf_tree, vbf_node));

				}
				return vbf_node;
			}
		};
		TrieNode trie;
		uint64_t trie_size = 1;

		for (size_t i = 0; i < files.size(); ++i)
		{
			TrieNode* trie_node = &trie;

			for (const std::string& name_seg : IO::Segment(files[i].name))
			{
				auto it = std::find_if(trie_node->children.begin(), trie_node->children.end(), [&name_seg](const TrieNode* child) { return child->name == name_seg; });
				if (it == trie_node->children.end())
				{
					trie_node->children.push_back(new TrieNode{ .name = name_seg });
					it = std::prev(trie_node->children.end());
					++trie_size;
				}
				trie_node = *it;
			}

			trie_node->file = &files[i];
		}

		tree.reserve(trie_size);
		trie.store(tree, nullptr);

		if (std::filesystem::is_directory("cache") || std::filesystem::create_directory("cache"))
		{
			std::ofstream stream(cache_path, std::ios::binary);
			write<uint32_t>(stream, tree.size());

			for (auto& node : tree)
			{
				write<uint32_t>(stream, node.name_segment.size());
				write(stream, node.name_segment.data(), node.name_segment.size());
				write<int32_t>(stream, node.parent ? node.parent - tree.data() : -1);
				write<int32_t>(stream, node.file ? node.file - files.data() : -1);
				write<uint32_t>(stream, node.children.size());
				for (size_t i = 0; i < node.children.size(); ++i)
				{
					write<uint32_t>(stream, node.children[i] - tree.data());
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << std::string("VBF LOAD FALURE: ") + e.what() + " " + ec.message() << std::endl;
		std::remove(cache_path.c_str());
		return false;
	}
	return true;
}

const VBFArchive::File* VBFArchive::find_file(const std::string& file_name) const
{
	md5hash name_hash = md5hash(md5hasher, IO::Normalise(file_name));

	for (const File& file : files)
	{
		if (file.name_hash == name_hash)
		{
			return &file;
		}
	}
	return nullptr;
}

VBFArchive::File* VBFArchive::find_file(const std::string& file_name)
{
	return (VBFArchive::File*)(((const VBFArchive*)this)->find_file(file_name));
}

const VBFArchive::TreeNode* VBFArchive::find_node(const std::string& file_name) const
{
	if (tree.empty())
	{
		return nullptr;
	}
	const VBFArchive::TreeNode* node = tree.data();
	std::vector<std::string> segments = IO::Segment(file_name);

	for (int i = 0; i < segments.size(); ++i)
	{
		bool seg_match = false;
		for (TreeNode* child : node->children)
		{
			if (compare(child->name_segment, segments[i]))
			{
				node = child;
				seg_match = true;
				break;
			}
		}
		if (!seg_match)
		{
			return nullptr;
		}
	}
	return node;
}

VBFArchive::TreeNode* VBFArchive::find_node(const std::string& file_name)
{
	return (VBFArchive::TreeNode*)(((const VBFArchive*)this)->find_node(file_name));
}

bool VBFArchive::unmodified() const
{
	if (arc_path.empty())
	{
		LOG(ERR) << "vbf not loaded" << std::endl;
		return false;
	}
	try
	{
		if (arc_timestamp == std::filesystem::last_write_time(arc_path))
		{
			return true;
		}
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
		return false;
	}
	return false;
}

bool VBFArchive::signal_modified()
{
	if (arc_path.empty())
	{
		LOG(ERR) << "vbf not loaded" << std::endl;
		return false;
	}
	try
	{
		arc_timestamp = std::filesystem::last_write_time(arc_path);
	}
	catch (const std::exception& e)
	{
		const auto ec = std::error_code{ errno, std::system_category() };
		LOG(ERR) << "IO ERROR: " << e.what() << " " << ec.message() << std::endl;
		return false;
	}
	return true;
}

bool VBFUtils::Separate(const std::string& arc_file_path, std::string& arc_path, std::string& file_name)
{
	try
	{
		std::string n_arc_file_path = Normalise(arc_file_path);
		size_t arc_end = n_arc_file_path.find(".vbf");
		if (arc_end == std::string::npos)
		{
			LOG(ERR) << ".vbf not found in " << n_arc_file_path << std::endl;
			return false;
		}
		arc_path = n_arc_file_path.substr(0, arc_end + 4);
		if (arc_end + 4 == n_arc_file_path.size())
		{
			return true;
		}
		file_name = n_arc_file_path.substr(arc_end + 5);
		std::replace(file_name.begin(), file_name.end(), '\\', '/');
	}
	catch (const std::exception& e)
	{
		LOG(ERR) << "ERROR: " << e.what() << std::endl;
		return false;
	}
	return true;
}

bool VBFArchive::extract_all(const VBFArchive::TreeNode& node, const std::string& folder, bool* stop_early) const
{
	std::string path = folder + "/" + node.name_segment;

	if (node.file && !extract(*node.file, path))
	{
		return false;
	}

	if (!node.children.empty())
	{
		if (!(std::filesystem::is_directory(path) || std::filesystem::create_directory(path)))
		{
			LOG(ERR) << "Failed to create directory " << path;
			return false;
		}
		for (TreeNode* child : node.children)
		{
			if (stop_early && *stop_early)
			{
				return false;
			}
			if (!extract_all(*child, path, stop_early))
			{
				return false;
			}
		}
	}
	return true;
}

bool VBFArchive::inject_all(VBFArchive::TreeNode& node, const std::string& folder, bool* stop_early)
{
	for (const auto& dir_entry : std::filesystem::directory_iterator(folder))
	{
		auto path_entry = dir_entry.path();
		if (path_entry.has_filename())
		{
			std::string filename = path_entry.filename().string();

			if (node.name_segment == filename)
			{
				if (node.file && !inject(*node.file, path_entry.string()))
				{
					return false;
				}

				if (!node.children.empty())
				{
					std::string path = folder + "/" + node.name_segment;
					for (TreeNode* child : node.children)
					{
						if (stop_early && *stop_early)
						{
							return false;
						}
						if (!inject_all(*child, path, stop_early))
						{
							return false;
						}
					}
				}
				break;
			}
		}
	}
	return true;
}

std::string VBFArchive::TreeNode::full_name() const
{
	return parent && parent->parent ? parent->full_name() + "/" + name_segment : name_segment;
}
