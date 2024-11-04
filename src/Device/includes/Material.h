/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_MATERIAL_H
#define DEVICE_MATERIAL_H

#include "Device/includes/Texture.h"

#include "HostDeviceCommon/HitInfo.h"
#include "HostDeviceCommon/RenderData.h"

#ifndef __KERNELCC__
#include "Image/Image.h"
#endif

template <typename T>
HIPRT_HOST_DEVICE HIPRT_INLINE void get_material_property(const HIPRTRenderData& render_data, T& output_data, bool is_srgb, const float2& texcoords, int texture_index);
HIPRT_HOST_DEVICE HIPRT_INLINE void get_metallic_roughness(const HIPRTRenderData& render_data, float& metallic, float& roughness, const float2& texcoords, int metallic_texture_index, int roughness_texture_index, int metallic_roughness_texture_index);
HIPRT_HOST_DEVICE HIPRT_INLINE void get_base_color(const HIPRTRenderData& render_data, ColorRGB32F& base_color, float& out_alpha, const float2& texcoords, int base_color_texture_index);

HIPRT_HOST_DEVICE HIPRT_INLINE float get_hit_base_color_alpha(const HIPRTRenderData& render_data, const RendererMaterial& material, hiprtHit hit)
{
    if (material.base_color_texture_index == -1)
        // Quick exit if no texture
        return 1.0f;

    float2 texcoords = uv_interpolate(render_data.buffers.triangles_indices, hit.primID, render_data.buffers.texcoords, hit.uv);

    // Getting the alpha for transparency check to see if we need to pass the ray through or not
    float alpha;
    ColorRGB32F base_color;
    get_base_color(render_data, base_color, alpha, texcoords, material.base_color_texture_index);

    return alpha;
}

HIPRT_HOST_DEVICE HIPRT_INLINE float get_hit_base_color_alpha(const HIPRTRenderData& render_data, hiprtHit hit)
{
    int material_index = render_data.buffers.material_indices[hit.primID];
    RendererMaterial material = render_data.buffers.materials_buffer[material_index];

    return get_hit_base_color_alpha(render_data, material, hit);
}

HIPRT_HOST_DEVICE HIPRT_INLINE SimplifiedRendererMaterial get_intersection_material(const HIPRTRenderData& render_data, int material_index, float2 texcoords)
{
	RendererMaterial material = render_data.buffers.materials_buffer[material_index];

    ColorRGB32F emission = material.get_emission() / material.emission_strength;
    get_material_property(render_data, emission, false, texcoords, material.emission_texture_index);
    material.set_emission(emission);

    float trash_alpha;
    get_base_color(render_data, material.base_color, trash_alpha, texcoords, material.base_color_texture_index);

    get_metallic_roughness(render_data, material.metallic, material.roughness, texcoords, material.metallic_texture_index, material.roughness_texture_index, material.roughness_metallic_texture_index);
    get_material_property(render_data, material.oren_nayar_sigma, false, texcoords, material.oren_sigma_texture_index);
    
    get_material_property(render_data, material.specular, false, texcoords, material.specular_texture_index);
    get_material_property(render_data, material.specular_tint, false, texcoords, material.specular_tint_texture_index);
    get_material_property(render_data, material.specular_color, false, texcoords, material.specular_color_texture_index);
    
    get_material_property(render_data, material.anisotropic, false, texcoords, material.anisotropic_texture_index);
    get_material_property(render_data, material.anisotropic_rotation, false, texcoords, material.anisotropic_rotation_texture_index);
    
    get_material_property(render_data, material.coat, false, texcoords, material.coat_texture_index);
    get_material_property(render_data, material.coat_roughness, false, texcoords, material.coat_roughness_texture_index);
    get_material_property(render_data, material.coat_ior, false, texcoords, material.coat_ior_texture_index);
    
    get_material_property(render_data, material.sheen, false, texcoords, material.sheen_texture_index);
    get_material_property(render_data, material.sheen_roughness, false, texcoords, material.sheen_roughness_texture_index);
    get_material_property(render_data, material.sheen_color, false, texcoords, material.sheen_color_texture_index);
    
    get_material_property(render_data, material.specular_transmission, false, texcoords, material.specular_transmission_texture_index);

    SimplifiedRendererMaterial simplified_material(material);
    simplified_material.emissive_texture_used = material.emission_texture_index > 0;

    return simplified_material;
}

HIPRT_HOST_DEVICE HIPRT_INLINE void get_metallic_roughness(const HIPRTRenderData& render_data, float& metallic, float& roughness, const float2& texcoords, int metallic_texture_index, int roughness_texture_index, int metallic_roughness_texture_index)
{
    if (metallic_roughness_texture_index != RendererMaterial::NO_TEXTURE)
    {
        ColorRGB32F rgb = sample_texture_rgb_8bits(render_data.buffers.material_textures, metallic_roughness_texture_index, render_data.buffers.textures_dims[metallic_roughness_texture_index], false, texcoords);

        // Not converting to linear here because material properties (roughness and metallic) here are assumed to be linear already
        roughness = rgb.g;
        metallic = rgb.b;
    }
    else
    {
        get_material_property(render_data, metallic, false, texcoords, metallic_texture_index);
        get_material_property(render_data, roughness, false, texcoords, roughness_texture_index);
    }
}

HIPRT_HOST_DEVICE HIPRT_INLINE void get_base_color(const HIPRTRenderData& render_data, ColorRGB32F& base_color, float& out_alpha, const float2& texcoords, int base_color_texture_index)
{
    ColorRGBA32F rgba;

    out_alpha = 1.0;
    get_material_property(render_data, rgba, true, texcoords, base_color_texture_index);
    if (base_color_texture_index != RendererMaterial::NO_TEXTURE)
    {
        base_color = ColorRGB32F(rgba.r, rgba.g, rgba.b);
        out_alpha = rgba.a;
    }
}

template <typename T>
HIPRT_HOST_DEVICE HIPRT_INLINE void read_data(const ColorRGBA32F& rgba, T& data) {}

template<>
HIPRT_HOST_DEVICE HIPRT_INLINE void read_data(const ColorRGBA32F& rgba, ColorRGBA32F& data)
{
    data = rgba;
}

template<>
HIPRT_HOST_DEVICE HIPRT_INLINE void read_data(const ColorRGBA32F& rgba, ColorRGB32F& data)
{
    data.r = rgba.r;
    data.g = rgba.g;
    data.b = rgba.b;
}

template<>
HIPRT_HOST_DEVICE HIPRT_INLINE void read_data(const ColorRGBA32F& rgba, float& data)
{
    data = rgba.r;
}

template <typename T>
HIPRT_HOST_DEVICE HIPRT_INLINE void get_material_property(const HIPRTRenderData& render_data, T& output_data, bool is_srgb, const float2& texcoords, int texture_index)
{
    if (texture_index == RendererMaterial::NO_TEXTURE || texture_index == RendererMaterial::CONSTANT_EMISSIVE_TEXTURE)
        return;

    ColorRGBA32F rgba = sample_texture_rgba(render_data.buffers.material_textures, texture_index, render_data.buffers.textures_dims[texture_index], is_srgb, texcoords);
    read_data(rgba, output_data);
}

#endif
