#pragma once

namespace PhyreInterface
{
	bool Initialise();
	bool Run(int argc, const char** argv);
	void Free();
	const char* Usage();
	const char* UsagePrompt();
}
