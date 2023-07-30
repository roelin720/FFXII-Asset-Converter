#pragma once
#include <iostream>

struct GlobalStream
{
	std::ostream* ostream = nullptr;

	GlobalStream(std::ostream* ostream) : ostream(ostream) {}

	template<typename T>
	GlobalStream& operator<<(const T& val)
	{
		if (ostream) *ostream << val;
		return *this;
	}

	typedef std::basic_ostream<char, std::char_traits<char> > manipT;
	typedef manipT& (*manipFunc)(manipT&);
	GlobalStream& operator<<(manipFunc manip)
	{
		if (ostream) manip(*ostream); ;
		return *this;
	}
};

extern GlobalStream gbl_log;
extern GlobalStream gbl_warn;
extern GlobalStream gbl_err;

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


