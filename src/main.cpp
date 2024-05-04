/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#include <iostream>
#include <cmath>
#include <stb_image_write.h>

#include "Device/kernels/PathTracerKernel.h"
#include "Image/Envmap.h"
#include "Image/Image.h"
#include "Renderer/BVH.h"
#include "Renderer/CPURenderer.h"
#include "Renderer/GPURenderer.h"
#include "Renderer/Triangle.h"
#include "Scene/Camera.h"
#include "Scene/SceneParser.h"
#include "UI/RenderWindow.h"
#include "Utils/CommandlineArguments.h"
#include "Utils/Utils.h"

#define GPU_RENDER 0

int main(int argc, char* argv[])
{
    CommandLineArguments cmd_arguments = CommandLineArguments::process_command_line_args(argc, argv);

    const int width = cmd_arguments.render_width;
    const int height = cmd_arguments.render_height;

    std::cout << std::endl << "Reading scene file " << cmd_arguments.scene_file_path << " ..." << std::endl;
    Scene parsed_scene = SceneParser::parse_scene_file(cmd_arguments.scene_file_path, (float)width / height);
    std::cout << std::endl;

#if GPU_RENDER

    RenderWindow render_window(width, height);

    GPURenderer& renderer = render_window.get_renderer();
    renderer.set_scene(parsed_scene);
    renderer.set_camera(parsed_scene.camera);
    render_window.run();

    return 0;

#else

    std::cout << "[" << width << "x" << height << "]: " << cmd_arguments.render_samples << " samples ; " << cmd_arguments.bounces << " bounces" << std::endl << std::endl;

    CPURenderer cpu_renderer(width, height);
    cpu_renderer.set_scene(parsed_scene);
    cpu_renderer.set_camera(parsed_scene.camera);
    cpu_renderer.get_render_settings().nb_bounces = cmd_arguments.bounces;
    cpu_renderer.get_render_settings().samples_per_frame = cmd_arguments.render_samples;
    cpu_renderer.render();
    cpu_renderer.tonemap(2.2f, 1.0f);

    Image image_denoised_1 = Utils::OIDN_denoise(cpu_renderer.get_framebuffer(), width, height, 1.0f);
    Image image_denoised_075 = Utils::OIDN_denoise(cpu_renderer.get_framebuffer(), width, height, 0.75f);
    Image image_denoised_05 = Utils::OIDN_denoise(cpu_renderer.get_framebuffer(), width, height, 0.5f);

    cpu_renderer.get_framebuffer().write_image_png("RT_output.png");
    image_denoised_1.write_image_png("RT_output_denoised_1.png");
    image_denoised_075.write_image_png("RT_output_denoised_075.png");
    image_denoised_05.write_image_png("RT_output_denoised_05.png");

    return 0;
#endif
}
