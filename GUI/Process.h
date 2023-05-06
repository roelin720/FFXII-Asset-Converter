#include <windows.h> 
#include <string>

struct Process
{
	int Execute(const std::string& cmd, std::string& cout, std::string& cerr, const std::string& id);
};