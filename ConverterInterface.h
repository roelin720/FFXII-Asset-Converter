#pragma once
#include <iostream>
#include <sstream>

struct GlobalLogger
{
	static FILE* file;
	static std::stringstream sstream;
	static bool to_file;

	uint32_t ID = 0;

	GlobalLogger(uint32_t ID);

	void write(const std::string& string);

	static void direct_to_file();
	static void direct_to_sstream();
};

struct GlobalLoggers
{
	static GlobalLogger InfoLogger;
	static GlobalLogger KeyInfoLogger;
	static GlobalLogger SuccessLogger;
	static GlobalLogger WarningLogger;
	static GlobalLogger ErrorLogger;
};

struct TemporaryLogger
{
	GlobalLogger& logger;
	std::stringstream sstream;

	TemporaryLogger(GlobalLogger& logger);
	~TemporaryLogger();

	TemporaryLogger& operator<<(const TemporaryLogger& val) = delete;

	template<typename T>
	TemporaryLogger& operator<<(const T& val)
	{
		sstream << val;
		return *this;
	}

	typedef std::ostream& (*manipFunc)(std::ostream&);
	inline TemporaryLogger& operator<<(manipFunc manip)
	{
		manip(sstream);
		return *this;
	}
};

#define INFO GlobalLoggers::InfoLogger
#define WARN GlobalLoggers::WarningLogger
#define ERR GlobalLoggers::ErrorLogger
#define KEYINFO GlobalLoggers::KeyInfoLogger
#define SUCCESS GlobalLoggers::SuccessLogger

#define LOG(logger) TemporaryLogger(logger)

namespace ConverterInterface
{
	bool Initialise();
	bool Run(int argc, const char** argv);
	void Free();

	bool UnpackDAE(const std::string& original_path, const std::string& intermediary_path);
	bool PackDAE(const std::string& original_path, const std::string& intermediary_path, const std::string& mod_output_path);

	bool UnpackDDS(const std::string& original_path, const std::string& intermediary_path);
	bool PackDDS(const std::string& original_path, const std::string& intermediary_path, const std::string& mod_output_path);
}


