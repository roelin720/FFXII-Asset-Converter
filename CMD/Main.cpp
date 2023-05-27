
#include "PhyreInterface.h"
#include <iostream>

int main(int argc, char* argv[])
{
    std::clog << "Executing commands..." << std::endl << std::endl;
    int err_code = -1;
    if (PhyreInterface::Initialise())
    {
        err_code = !PhyreInterface::Run(argc, (const char**)argv);
        PhyreInterface::Free();
    }
    
    if (err_code == 0)
    {
        std::clog << std::endl << "Execution complete" << std::endl << std::endl;
    }
    return err_code;
}
