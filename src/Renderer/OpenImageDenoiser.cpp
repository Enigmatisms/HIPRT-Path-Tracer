/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#include "Renderer/OpenImageDenoiser.h"

#include <iostream>

OpenImageDenoiser::OpenImageDenoiser() : m_width(0), m_height(0)
{
    // Create an Open Image Denoise device
    m_device = oidn::newDevice(); // CPU or GPU if available
    if (m_device.getHandle() == nullptr)
    {
        std::cerr << "There was an error getting the device for denoising with OIDN. Perhaps some missing DLLs for your hardware?" << std::endl;

        return;
    }

    m_device.commit();
}

void OpenImageDenoiser::set_buffers(ColorRGB* color_buffer, int width, int height)
{
    m_color_buffer = color_buffer;
    m_denoised_color_buffer = m_device.newBuffer(width * height * 3 * sizeof(float));
    m_width = width;
    m_height = height;

    m_use_albedo = false;
    m_use_normals = false;

    create_beauty_filter();
}

void OpenImageDenoiser::set_buffers(ColorRGB* color_buffer, int width, int height, bool override_use_normals, bool override_use_albedo)
{
    m_color_buffer = color_buffer;
    m_denoised_color_buffer = m_device.newBuffer(width * height * 3 * sizeof(float));
    m_width = width;
    m_height = height;

    m_use_normals = override_use_normals;
    m_use_albedo = override_use_albedo;

    create_beauty_filter();
}

void OpenImageDenoiser::set_buffers(ColorRGB* color_buffer, hiprtFloat3* normals_buffer, int width, int height)
{
    m_normals_buffer = normals_buffer;
    m_denoised_normals_buffer = m_device.newBuffer(width * height * sizeof(hiprtFloat3));

    set_buffers(color_buffer, width, height, true, false);
    create_AOV_filters();
}

void OpenImageDenoiser::set_buffers(ColorRGB* color_buffer, ColorRGB* albedo_buffer, int width, int height)
{
    m_albedo_buffer = albedo_buffer;
    m_denoised_albedo_buffer = m_device.newBuffer(width * height * sizeof(ColorRGB));

    set_buffers(color_buffer, width, height, false, true);
    create_AOV_filters();
}

void OpenImageDenoiser::set_buffers(ColorRGB* color_buffer, hiprtFloat3* normals_buffer, ColorRGB* albedo_buffer, int width, int height)
{
    m_albedo_buffer = albedo_buffer;
    m_normals_buffer = normals_buffer;

    m_denoised_albedo_buffer = m_device.newBuffer(width * height * sizeof(ColorRGB));
    m_denoised_normals_buffer = m_device.newBuffer(width * height * sizeof(hiprtFloat3));

    set_buffers(color_buffer, width, height, true, true);
    create_AOV_filters();
}

bool* OpenImageDenoiser::get_denoise_albedo_var()
{
    return &m_denoise_albedo;
}

bool* OpenImageDenoiser::get_denoise_normals_var()
{
    return &m_denoise_normals;
}

void OpenImageDenoiser::create_beauty_filter()
{
    m_beauty_filter = m_device.newFilter("RT"); // generic ray tracing filter
    m_beauty_filter.setImage("color", m_color_buffer, oidn::Format::Float3, m_width, m_height); // beauty

    if (m_use_albedo)
    {
        if (m_denoise_albedo)
            m_beauty_filter.setImage("albedo", m_denoised_albedo_buffer, oidn::Format::Float3, m_width, m_height);
        else
            m_beauty_filter.setImage("albedo", m_albedo_buffer, oidn::Format::Float3, m_width, m_height);
    }

    if (m_use_normals)
    {
        if (m_denoise_normals)
            m_beauty_filter.setImage("normal", m_denoised_normals_buffer, oidn::Format::Float3, m_width, m_height);
        else
            m_beauty_filter.setImage("normal", m_normals_buffer, oidn::Format::Float3, m_width, m_height);

    }

    m_beauty_filter.setImage("output", m_denoised_color_buffer, oidn::Format::Float3, m_width, m_height); // denoised beauty
    m_beauty_filter.set("cleanAux", true); // Normals and albedo are not noisy
    m_beauty_filter.set("hdr", true); // beauty image is HDR
    m_beauty_filter.commit();

    const char* errorMessage;
    if (m_device.getError(errorMessage) != oidn::Error::None)
    {
        std::cout << "Filter configuration error: " << errorMessage << std::endl;
        return;
    }
}

void OpenImageDenoiser::create_AOV_filters()
{
    if (m_use_albedo)
    {
        m_albedo_filter = m_device.newFilter("RT");
        m_albedo_filter.setImage("albedo", m_albedo_buffer, oidn::Format::Float3, m_width, m_height);
        m_albedo_filter.setImage("output", m_denoised_albedo_buffer, oidn::Format::Float3, m_width, m_height);
        m_albedo_filter.commit();
    }

    if (m_use_normals)
    {
        m_normals_filter = m_device.newFilter("RT");
        m_normals_filter.setImage("normal", m_normals_buffer, oidn::Format::Float3, m_width, m_height);
        m_normals_filter.setImage("output", m_denoised_normals_buffer, oidn::Format::Float3, m_width, m_height);
        m_normals_filter.commit();
    }

    const char* errorMessage;
    if (m_device.getError(errorMessage) != oidn::Error::None)
    {
        std::cout << "Filter configuration error: " << errorMessage << std::endl;
        return;
    }
}

std::vector<ColorRGB> OpenImageDenoiser::get_denoised_data()
{
    ColorRGB* denoised_ptr = (ColorRGB*)m_denoised_color_buffer.getData();

    std::vector<ColorRGB> denoised_output;
    denoised_output.insert(denoised_output.end(), &denoised_ptr[0], &denoised_ptr[m_width * m_height]);

    return denoised_output;
}

void* OpenImageDenoiser::get_denoised_data_pointer()
{
    return m_denoised_color_buffer.getData();
}

void* OpenImageDenoiser::get_denoised_albedo_pointer()
{
    return m_denoised_albedo_buffer.getData();
}

void* OpenImageDenoiser::get_denoised_normals_pointer()
{
    return m_denoised_normals_buffer.getData();
}

void OpenImageDenoiser::denoise()
{
    // Fill the input image buffers
    if (m_use_albedo && m_denoise_albedo)
        m_albedo_filter.execute();
    if (m_use_normals && m_denoise_normals)
        m_normals_filter.execute();

    m_beauty_filter.execute();

    const char* errorMessage;
    if (m_device.getError(errorMessage) != oidn::Error::None)
        std::cout << "Error: " << errorMessage << std::endl;
}

void OpenImageDenoiser::denoise_normals()
{
    if (m_use_normals && m_denoise_normals)
        // This means that everything is setup for denoising
        m_normals_filter.execute();
}

void OpenImageDenoiser::denoise_albedo()
{
    if (m_use_albedo && m_denoise_albedo)
        // This means that everything is setup for denoising
        m_albedo_filter.execute();
}
