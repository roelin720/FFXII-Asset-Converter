
#include "ConverterInterface.h"
#include <iostream>

int main(int argc, char* argv[])
{
    std::clog << "Executing commands..." << std::endl << std::endl;
    int err_code = -1;
    if (ConverterInterface::Initialise())
    {
        err_code = !ConverterInterface::Run(argc, (const char**)argv);
        ConverterInterface::Free();
    }
    
    if (err_code == 0)
    {
        std::clog << std::endl << "Execution complete" << std::endl << std::endl;
    }
    return err_code;
}
