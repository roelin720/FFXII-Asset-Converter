#pragma once
#include <Windows.h>
#include <string>
#include <vector>

class Pipe
{
protected:
	Pipe() = default;
	std::string make_ID();
public:
	HANDLE handle = INVALID_HANDLE_VALUE;
	std::string name;

	bool opened() const;
	size_t peek() const;

	bool write(const char* data, size_t size);
	bool read(char* data, size_t size);
	bool read_all(std::vector<char>& vec);
	bool read_all(std::string& str);

	template<typename T> bool write(const T& val)
	{
		return write((const char*)&val, sizeof(T));
	}
	template<typename T> bool read(T& val)
	{
		return read((char*)&val, sizeof(T));
	}

	virtual void close();
	virtual ~Pipe();
};

class PipeServer : public Pipe
{
public:
	uint32_t buffer_size = 0;

	bool create(const std::string& name, uint32_t buffer_size = 1 << 12);
	bool connect();
	void disconnect();

	void close() override;
};

class PipeClient : public Pipe
{
public:
	bool open(const std::string& name);
};
