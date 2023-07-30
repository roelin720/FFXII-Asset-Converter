
#include "ConverterInterface.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if constexpr (true)
    {
        int _argc = 5;
        const char* _argv[]
        {
            argv[0],
            "pack",
            "O:/Noesis/VBF/Original/FFXII_TZA.vbf/gamedata/d3d11/artdata/chr/npc/n10/n1014/tex/tex_0_clut_0_n1014_a_clut_1.dds.phyre",
            "B:/VBF/dmp/gamedata/d3d11/artdata/chr/npc/n10/n1014/tex/tex_0_clut_0_n1014_a_clut_1.png",
            "B:/VBF/tex_0_clut_0_n1014_a_clut_1.dds.phyre"
        };
        int err_code = -1;
        if (ConverterInterface::Initialise())
        {
            err_code = !ConverterInterface::Run(_argc, _argv);
            ConverterInterface::Free();
        }
        return err_code;
    }
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
