#include "ConverterInterface.h"
#include <filesystem>
#include "Viewer.h"
#include "args-parser/all.hpp"
#include "glm/ext/scalar_constants.hpp"

namespace
{
    bool run(int argc, const char** argv)
    {
        try
        {
            Args::CmdLine cmd(argc, argv, Args::CmdLine::CmdLineOpt::CommandIsRequired);

            Args::Command show_cmd(SL("show"), Args::ValueOptions::OneValue, false);
            show_cmd.setDescription(SL("Displays the asset in a window."));

            Args::Command snapshot_cmd(SL("snapshot"), Args::ValueOptions::OneValue, false);
            snapshot_cmd.setDescription(SL("Captures (several) images of the asset."));

            Args::Arg width_arg(SL('x'), SL("size_x"), true, false);
            width_arg.setDescription(SL("The view/image width."));

            Args::Arg height_arg(SL('y'), SL("size_y"), true, false);
            height_arg.setDescription(SL("The view/image height."));

            Args::Arg out_arg(('o'), SL("out"), true, true);
            out_arg.setDescription(SL("The filename to save the snapshots with."));

            Args::Arg count_arg(SL('c'), SL("count"), true, false);
            count_arg.setDescription(SL("The number of snapshots to make."));

            Args::Help help;
            help.setExecutable(argv[0]);
            help.setAppDescription("Opens a view of a given asset, or takes one or multiple snapshots of it.");

            show_cmd.addArg(width_arg);
            show_cmd.addArg(height_arg);
            snapshot_cmd.addArg(width_arg);
            snapshot_cmd.addArg(height_arg);
            snapshot_cmd.addArg(out_arg);
            snapshot_cmd.addArg(count_arg);
            cmd.addArg(show_cmd);
            cmd.addArg(snapshot_cmd);
            cmd.addArg(help);
            cmd.setPositionalDescription("asset_path");

            cmd.parse();

            size_t width = width_arg.isDefined() ? std::stoi(width_arg.value()) : 1024;
            size_t height = height_arg.isDefined() ? std::stoi(height_arg.value()) : 720;
            glm::uvec2 size = glm::uvec2(width, height);

            if (show_cmd.isDefined())
            {
                std::string asset_path = show_cmd.value();
                if (asset_path.empty() || !std::filesystem::exists(asset_path))
                {
                    LOG(ERR) << "path \"" << asset_path << "\" could not be found";
                    return false;
                }
                Viewer viewer;
                if (!viewer.init(size) || !viewer.load_scene(asset_path))
                {
                    return false;
                }
                viewer.run();
            }
            else if (snapshot_cmd.isDefined())
            {
                std::string asset_path = snapshot_cmd.value();
                if (asset_path.empty() || !std::filesystem::exists(asset_path))
                {
                    LOG(ERR) << "path \"" << asset_path << "\" could not be found";
                    return false;
                }
                Viewer viewer;
                if (!viewer.init(size) || !viewer.load_scene(asset_path))
                {
                    return false;
                }

                float samples[] = {2.0f * glm::pi<float>(), 0.5f * glm::pi<float>(), 1.0f * glm::pi<float>(), 1.5f * glm::pi<float>() };

                std::vector<glm::vec2> rotations;
                rotations.reserve(_countof(samples) * _countof(samples));

                for (int i = 0; i < _countof(samples); ++i)
                {
                    for (int j = 0; j < _countof(samples); ++j)
                    {
                        rotations.push_back(glm::vec2(samples[i], samples[j]));
                    }
                }

                size_t snapshot_count = count_arg.isDefined() ? std::clamp(std::stoi(count_arg.value()), 1, (int)rotations.size()) : 1;
                std::string out_file_base = out_arg.value();

                for (size_t i = 0; i < snapshot_count; ++i)
                {
                    std::string out_path = out_file_base;
                    if (snapshot_count > 1)
                    {
                        std::string ext = std::filesystem::path(out_file_base).extension().string();
                        out_path = std::filesystem::path(out_file_base).replace_extension(std::to_string(i) + ext).string();
                    }
                    viewer.renderer->camera.theta = rotations[i].x;
                    viewer.renderer->camera.phi = rotations[i].y;
                    viewer.renderer->render_scene_to_file(out_path, size);
                }
            }
        }
        catch (const Args::HelpHasBeenPrintedException&)
        {
        }
        catch (const Args::BaseException& e)
        {
            LOG(ERR) << e.desc() << std::endl;
            return false;
        }
        catch (const std::exception& e)
        {
            LOG(ERR) << "Exception: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
}


int main(int argc, const char* argv[])
{
    //if constexpr (false)
    //{
    //    const char* _argv[] =
    //    {
    //        argv[0],
    //        "show",
    //        "B:/VBF/dmp/gamedata/d3d11/artdata/chr/chara/c10/c1004/mws/c1004.dae.phyre"
    //    };
    //    int _argc = _countof(_argv);
    //
    //    return !run(_argc, _argv);
    //}
    //if constexpr (true)
    //{
    //    const char* _argv[] =
    //    {
    //        argv[0],
    //        "snapshot",
    //        "B:/VBF/dmp/gamedata/d3d11/artdata/chr/chara/c10/c1004/mws/c1004.dae.phyre",
    //        "-o", "B:\\FFX\\chevy\\two\\img.png",
    //        "-x", "1024",
    //        "-y", "1024",
    //        "-c", "16"
    //    };
    //    int _argc = _countof(_argv);
    //
    //    return !run(_argc, _argv);
    //}
    return !run(argc, argv);
}

