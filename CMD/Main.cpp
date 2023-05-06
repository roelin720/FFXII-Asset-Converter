
#include "PhyreInterface.h"
#include <iostream>

int main(int argc, char* argv[])
{
    //if constexpr (true)
    //{
    //    std::clog << "Executing commands..." << std::endl;
    //
    //    if (PhyreInterface::Initialise()) 
    //    {
    //        int _argc = 5;
    //        const char* _argv[] = { argv[0], "--pack",
    //            "O:/Noesis/VBF/Output/gamedata/d3d11/artdata/chr/chara/c10/c1005/tex/tex_0_clut_0_c1005_clut_1_n.dds.phyre",
    //            "O:/Noesis/VBF/Mods/Stage/c1005/tex/tex_0_clut_0_c1005_clut_1_n.png",
    //            "O:/Noesis/VBF/Mods/Final/c1005/tex/tex_0_clut_0_c1005_clut_1_n.dds.phyre"
    //        };
    //        return PhyreInterface::Run(_argc, (const char**)_argv);
    //    }
    //
    //    return 0;
    //}

    std::clog << "Executing commands..." << std::endl << std::endl;
    int res = -1;
    if (PhyreInterface::Initialise())
    {
        res = PhyreInterface::Run(argc, (const char**)argv);
    }
    std::clog << std::endl << "Execution complete" << std::endl << std::endl;
    return res;
}
