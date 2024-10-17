/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_PRINCIPLED_H
#define DEVICE_PRINCIPLED_H

#include "Device/includes/FixIntellisense.h"
#include "Device/includes/ONB.h"
#include "Device/includes/BSDFs/OrenNayar.h"
#include "Device/includes/RayPayload.h"
#include "Device/includes/Sampling.h"
#include "Device/includes/BSDFs/SheenLTC.h"

#include "HostDeviceCommon/Material.h"
#include "HostDeviceCommon/Xorshift.h"

 /** References:
  *
  * [1] [CSE 272 University of California San Diego - Disney BSDF Homework] https://cseweb.ucsd.edu/~tzli/cse272/wi2024/homework1.pdf
  * [2] [GLSL Path Tracer implementation by knightcrawler25] https://github.com/knightcrawler25/GLSL-PathTracer
  * [3] [SIGGRAPH 2012 Course] https://blog.selfshadow.com/publications/s2012-shading-course/#course_content
  * [4] [SIGGRAPH 2015 Course] https://blog.selfshadow.com/publications/s2015-shading-course/#course_content
  * [5] [Burley 2015 Course Notes - Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering] https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
  * [6] [PBRT v3 Source Code] https://github.com/mmp/pbrt-v3
  * [7] [PBRT v4 Source Code] https://github.com/mmp/pbrt-v4
  * [8] [Blender's Cycles Source Code] https://github.com/blender/cycles
  * [9] [Autodesk Standard Surface] https://autodesk.github.io/standard-surface/
  * [10] [Blender Principled BSDF] https://docs.blender.org/manual/fr/dev/render/shader_nodes/shader/principled.html
  *
  * Important note: none of the lobes of this implementation includes the cosine term.
  * The cosine term NoL needs to be taken into account outside of the BSDF
  */

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_coat_eval(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3& local_to_light_direction, const float3& local_halfway_vector, float incident_ior, float& out_pdf)
{
    if (local_view_direction.z * local_to_light_direction.z < 0)
        // View direction and to light direction not in the same hemisphere
        // This lobe only reflects so this is an invalid configuration
        return ColorRGB32F(0.0f);

    // The coat lobe is just a microfacet lobe
    return microfacet_GTR2_eval(material.coat_roughness, material.coat_ior, incident_ior, local_view_direction, local_to_light_direction, local_halfway_vector, out_pdf);
}

/**
 * The sampled direction is returned in the local shading frame of the basis used for 'local_view_direction'
 */
HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_coat_sample(const SimplifiedRendererMaterial& material, const float3& local_view_direction, Xorshift32Generator& random_number_generator)
{
    return microfacet_GTR2_sample(material.coat_roughness, material.coat_anisotropy, local_view_direction, random_number_generator);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_sheen_eval(const HIPRTRenderData& render_data, const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3& local_to_light_direction, float& pdf)
{
    return sheen_ltc_eval(render_data, material, local_to_light_direction, local_view_direction, pdf);
}

HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_sheen_sample(const HIPRTRenderData& render_data, const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3& shading_normal, Xorshift32Generator& random_number_generator)
{
    return sheen_ltc_sample(render_data, material, local_view_direction, shading_normal, random_number_generator);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_metallic_eval(const SimplifiedRendererMaterial& material, float incident_ior, const float3& local_view_direction, const float3& local_to_light_direction, const float3& local_half_vector, float& pdf)
{
    float HoL = hippt::clamp(1.0e-8f, 1.0f, hippt::dot(local_half_vector, local_to_light_direction));

    ColorRGB32F F;
    if (material.advanced_metallic_fresnel)
        // Computing the complex fresnel response from conductor material using
        // the intuitive parameters
        F = gulbrandsen_metallic_fresnel(material.metallic_reflectivity, material.metallic_edge_tint, HoL);
    else
        // The reflectance of the metal is here assumed to be the same color as
        // the base color of the material which can be easier to manipulate
        // than the metallic specialization above
        F = fresnel_schlick(material.base_color, HoL);

    return microfacet_GTR2_eval(material.roughness, F, local_view_direction, local_to_light_direction, local_half_vector, pdf);
}

/**
 * The sampled direction is returned in the local shading frame of the basis used for 'local_view_direction'
 */
HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_metallic_sample(const SimplifiedRendererMaterial& material, const float3& local_view_direction, Xorshift32Generator& random_number_generator)
{
    return microfacet_GTR2_sample(material.roughness, material.anisotropy, local_view_direction, random_number_generator);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_diffuse_eval(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3& local_to_light_direction, float& pdf)
{
    // The diffuse lobe is a simple Oren Nayar lobe
    return oren_nayar_brdf_eval(material, local_view_direction, local_to_light_direction, pdf);
}

/**
 * The sampled direction is returned in world space
 */
HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_diffuse_sample(const float3& surface_normal, Xorshift32Generator& random_number_generator)
{
    // Our Oren-Nayar diffuse lobe is sampled by a cosine weighted distribution
    return cosine_weighted_sample(surface_normal, random_number_generator);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_specular_eval(const SimplifiedRendererMaterial& material, float incident_ior, const float3& local_view_direction, const float3& local_to_light_direction, const float3& local_half_vector, float& pdf)
{
    // The specular lobe is just another GGX (GTR2) lobe
    return microfacet_GTR2_eval(material.roughness, material.ior, incident_ior, local_view_direction, local_to_light_direction, local_half_vector, pdf);
}

HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_specular_sample(const SimplifiedRendererMaterial& material, const float3& local_view_direction, Xorshift32Generator& random_number_generator)
{
    return microfacet_GTR2_sample(material.roughness, material.anisotropy, local_view_direction, random_number_generator);
}

// TODO have materials_buffer as a global variable to avoid having to pass it around like that?
HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_glass_eval(const RendererMaterial* materials_buffer, const SimplifiedRendererMaterial& material, RayVolumeState& ray_volume_state, const float3& local_view_direction, const float3& local_to_light_direction, float& pdf)
{
    float NoV = local_view_direction.z;
    float NoL = local_to_light_direction.z;

    if (hippt::abs(NoL) < 1.0e-8f)
        // Check to avoid dividing by 0 later on
        return ColorRGB32F(0.0f);

    // We're in the case of reflection if the view direction and the bounced ray (light direction) are in the same hemisphere
    bool reflecting = NoL * NoV > 0;

    // Relative eta = eta_t / eta_i
    float eta_t = ray_volume_state.outgoing_mat_index == InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX ? 1.0 : materials_buffer[ray_volume_state.outgoing_mat_index].ior;
    float eta_i = ray_volume_state.incident_mat_index == InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX ? 1.0 : materials_buffer[ray_volume_state.incident_mat_index].ior;
    float relative_eta = eta_t / eta_i;

    // relative_eta can be 1 when refracting from a volume into another volume of the same IOR.
    // This in conjunction with the view direction and the light direction being the negative of
    // one another will lead the microfacet normal to be the null vector which then causes
    // NaNs.
    // 
    // Example:
    // The view and light direction can be the negative of one another when looking straight at a
    // flat window for example. The view direction is aligned with the normal of the window
    // in this configuration whereas the refracting light direction (and it is very likely to refract
    // in this configuration) is going to point exactly away from the view direction and the normal.
    // 
    // We then have
    // 
    // half_vector  = light_dir + relative_eta * view_dir
    //              = light_dir + 1.0f * view_dir
    //              = light_dir + view_dir = (0, 0, 0)
    //
    // Normalizing this null vector then leads to a NaNs because of the zero-length.
    //
    // We're settings relative_eta to 1.00001f to avoid this issue
    if (hippt::abs(relative_eta - 1.0f) < 1.0e-5f)
        relative_eta = 1.0f + 1.0e-5f;

    // Computing the generalized (that takes refraction into account) half vector
    float3 local_half_vector;
    if (reflecting)
        local_half_vector = local_to_light_direction + local_view_direction;
    else
        // We need to take the relative_eta into account when refracting to compute
        // the half vector (this is the "generalized" part of the half vector computation)
        local_half_vector = local_to_light_direction * relative_eta + local_view_direction;

    local_half_vector = hippt::normalize(local_half_vector);
    if (local_half_vector.z < 0.0f)
        // Because the rest of the function we're going to compute here assume
        // that the microfacet normal is in the same hemisphere as the surface
        // normal, we're going to flip it if needed
        local_half_vector = -local_half_vector;

    float HoL = hippt::dot(local_to_light_direction, local_half_vector);
    float HoV = hippt::dot(local_view_direction, local_half_vector);

    if (HoL * NoL < 0.0f || HoV * NoV < 0.0f)
        // Backfacing microfacets when the microfacet normal isn't in the same
        // hemisphere as the view dir or light dir
        return ColorRGB32F(0.0f);

    ColorRGB32F color;
    float F = fresnel_dielectric(hippt::dot(local_view_direction, local_half_vector), relative_eta);
    if (reflecting)
    {
        color = microfacet_GTR2_eval(material.roughness, ColorRGB32F(F), local_view_direction, local_to_light_direction, local_half_vector, pdf);

        // Scaling the PDF by the probability of being here (reflection of the ray and not transmission)
        pdf *= F;
    }
    else
    {
        float dot_prod = HoL + HoV / relative_eta;
        float dot_prod2 = dot_prod * dot_prod;
        float denom = dot_prod2 * NoL * NoV;

        float alpha_x;
        float alpha_y;
        SimplifiedRendererMaterial::get_alphas(material.roughness, material.anisotropy, alpha_x, alpha_y);

        float D = GTR2_anisotropic(alpha_x, alpha_y, local_half_vector);
        float G1_V = G1(alpha_x, alpha_y, local_view_direction);
        float G = G1_V * G1(alpha_x, alpha_y, local_to_light_direction);

        float dwm_dwi = hippt::abs(HoL) / dot_prod2;
        float D_pdf = G1_V / hippt::abs(NoV) * D * hippt::abs(HoV);
        pdf = dwm_dwi * D_pdf * (1.0f - F);

        // We added a check a few lines above to "avoid dividing by 0 later on". This is where.
        // When NoL is 0, denom is 0 too and we're dividing by 0. 
        // The PDF of this case is as low as 1.0e-9 (light direction sampled perpendicularly to the normal)
        // so this is an extremely rare case.
        // The PDF being non-zero, we could actualy compute it, it's valid but not with floats :D
        color = sqrt(material.base_color) * D * (1 - F) * G * hippt::abs(HoL * HoV / denom);

        if (ray_volume_state.incident_mat_index != InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX)
        {
            // If we're not coming from the air, this means that we were in a volume and we're currently
            // refracting out of the volume or into another volume.
            // This is where we take the absorption of our travel into account using Beer-Lambert's law.
            // Note that we want to use the absorption of the material we finished traveling in.
            // The BSDF we're evaluating right now is using the new material we're refracting in, this is not
            // by this material that the ray has been absorbed. The ray has been absorded by the volume
            // it was in before refracting here, so it's the incident mat index

            const SimplifiedRendererMaterial& incident_material = materials_buffer[ray_volume_state.incident_mat_index];
            // Remapping the absorption coefficient so that it is more intuitive to manipulate
            // according to Burley, 2015 [5].
            // This effectively gives us a "at distance" absorption coefficient.
            ColorRGB32F absorption_coefficient = log(incident_material.absorption_color) / incident_material.absorption_at_distance;
            color = color * exp(absorption_coefficient * ray_volume_state.distance_in_volume);

            // We changed volume so we're resetting the distance
            ray_volume_state.distance_in_volume = 0.0f;
            if (ray_volume_state.leaving_mat)
                // We refracting out of a volume so we're poping the stack
                ray_volume_state.interior_stack.pop(ray_volume_state.leaving_mat);
        }
    }

    return color;
}

/**
 * The sampled direction is returned in the local shading frame of the basis used for 'local_view_direction'
 */
HIPRT_HOST_DEVICE HIPRT_INLINE float3 principled_glass_sample(const RendererMaterial* materials_buffer, const SimplifiedRendererMaterial& material, RayVolumeState& ray_volume_state, const float3& local_view_direction, Xorshift32Generator& random_number_generator)
{
    // Relative eta = eta_t / eta_i
    float eta_t = ray_volume_state.outgoing_mat_index == InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX ? 1.0 : materials_buffer[ray_volume_state.outgoing_mat_index].ior;
    float eta_i = ray_volume_state.incident_mat_index == InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX ? 1.0 : materials_buffer[ray_volume_state.incident_mat_index].ior;
    float relative_eta = eta_t / eta_i;
    // To avoid sampling directions that would lead to a null half_vector. 
    // Explained in more details in glass_eval.
    if (hippt::abs(relative_eta - 1.0f) < 1.0e-5f)
        relative_eta = 1.0f + 1.0e-5f;

    float alpha_x;
    float alpha_y;
    SimplifiedRendererMaterial::get_alphas(material.roughness, material.anisotropy, alpha_x, alpha_y);
    float3 microfacet_normal = GGX_aniso_sample(local_view_direction, alpha_x, alpha_y, random_number_generator);

    float F = fresnel_dielectric(hippt::dot(local_view_direction, microfacet_normal), relative_eta);
    float rand_1 = random_number_generator();

    float3 sampled_direction;
    if (rand_1 < F)
    {
        // Reflection
        sampled_direction = reflect_ray(local_view_direction, microfacet_normal);

        // This is a reflection, we're poping the stack
        ray_volume_state.interior_stack.pop(false);
    }
    else
    {
        // Refraction

        if (hippt::dot(microfacet_normal, local_view_direction) < 0.0f)
            // For the refraction operation that follows, we want the direction to refract (the view
            // direction here) to be in the same hemisphere as the normal (the microfacet normal here)
            // so we're flipping the microfacet normal in case it wasn't in the same hemisphere as
            // the view direction
            // Relative_eta as already been flipped above in the code
            microfacet_normal = -microfacet_normal;

        refract_ray(local_view_direction, microfacet_normal, sampled_direction, relative_eta);
    }

    return sampled_direction;
}

/**
 * 'internal' functions are just so that 'principled_bsdf_eval' looks nicer
 */
HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F internal_eval_coat_layer(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3 local_to_light_direction, const float3& local_half_vector, float incident_ior, float coat_weight, float coat_proba, ColorRGB32F& layers_throughput, float& out_cumulative_pdf)
{
    if (coat_weight > 0.0f)
    {
        float coat_pdf;
        ColorRGB32F contribution = principled_coat_eval(material, local_view_direction, local_to_light_direction, local_half_vector, incident_ior, coat_pdf);
        contribution *= coat_weight;
        contribution *= layers_throughput;

        out_cumulative_pdf += coat_pdf * coat_proba;

        // When light hits the coat layer, only the refracted part gets transmitted to the layer below.
        // Because the coat layer is a dielectric layer, that refracted part is given by the Fresnel laws
        // 
        // Also, the coat layer color absorbs the light so we're taking that color into account as well
        ColorRGB32F transmitted_light = ColorRGB32F(1.0f) - fresnel_reflectance_from_ior(incident_ior, material.coat_ior, local_half_vector, local_to_light_direction);
        // Taking the color of the absorbing coat medium into account when the light that got transmitted
        // travels through it
        transmitted_light *= material.coat_color;
        // Blending the coat layer between completely transparent (no effect on the transmitted light
        // throughput when material.coat is 0.0f) and full absorption contribution.
        transmitted_light = (1.0f - material.coat) * ColorRGB32F(1.0f) + material.coat * transmitted_light;
        layers_throughput *= transmitted_light;

        return contribution;
    }

    return ColorRGB32F(0.0f);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F internal_eval_sheen_layer(const HIPRTRenderData& render_data, const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3& local_to_light_direction, const float3& world_space_to_light_direction, const float3& shading_normal, float incident_ior, float sheen_weight, float sheen_proba, ColorRGB32F& layers_throughput, float& out_cumulative_pdf)
{
    if (sheen_weight > 0.0f)
    {
        float sheen_pdf;
        ColorRGB32F contribution = principled_sheen_eval(render_data, material, local_view_direction, local_to_light_direction, sheen_pdf);
        contribution *= sheen_weight;
        contribution *= layers_throughput;

        out_cumulative_pdf += sheen_pdf * sheen_proba;

        // Same as the coat layer for the sheen: only the refracted light goes into the layer below
        // and the sheen layer doesn't absorb light so we're not taking color into account as
        // in the coat layer here
        //
        // Also, we're using the world space shading normal here and not half-vector because
        // the sheen lobe isn't a microfacet lobe so it's normal isn't the halfway-vector
        // (contrary to the coat / specular lobes for example)
        layers_throughput *= ColorRGB32F(1.0f) - material.sheen * fresnel_reflectance_from_ior(incident_ior, material.ior, shading_normal, world_space_to_light_direction);

        return contribution;
    }

    return ColorRGB32F(0.0f);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F internal_eval_metal_layer(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3 local_to_light_direction, const float3& local_half_vector, float incident_ior, float metal_weight, float metal_proba, ColorRGB32F& layers_throughput, float& out_cumulative_pdf)
{
    if (metal_weight > 0.0f)
    {
        float metal_pdf;
        ColorRGB32F contribution = principled_metallic_eval(material, incident_ior, local_view_direction, local_to_light_direction, local_half_vector, metal_pdf);
        contribution *= metal_weight;
        contribution *= layers_throughput;

        out_cumulative_pdf += metal_pdf * metal_proba;

        // There is nothing below the metal layer so we don't have a layer_throughput absorption here
        // ...

        return contribution;
    }

    return ColorRGB32F(0.0f);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F internal_eval_specular_layer(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3 local_to_light_direction, const float3& local_half_vector, float incident_ior, float specular_weight, float specular_proba, ColorRGB32F& layers_throughput, float& out_cumulative_pdf)
{
    if (specular_weight > 0.0f)
    {
        float specular_pdf;
        ColorRGB32F contribution = principled_specular_eval(material, incident_ior, local_view_direction, local_to_light_direction, local_half_vector, specular_pdf);
        contribution *= (1.0f - material.specular_tint) * ColorRGB32F(1.0f) + material.specular_tint * material.specular_color;
        contribution *= specular_weight;
        contribution *= layers_throughput;

        // Only the transmitted portion of the light goes to the layer below
        layers_throughput *= ColorRGB32F(1.0f) - material.specular * fresnel_reflectance_from_ior(incident_ior, material.ior, local_half_vector, local_to_light_direction);

        out_cumulative_pdf += specular_pdf * specular_proba;

        return contribution;
    }

    return ColorRGB32F(0.0f);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F internal_eval_diffuse_layer(const SimplifiedRendererMaterial& material, const float3& local_view_direction, const float3 local_to_light_direction, float diffuse_weight, float diffuse_proba, ColorRGB32F& layers_throughput, float& out_cumulative_pdf)
{
    if (diffuse_weight > 0.0f)
    {
        float diffuse_pdf;
        ColorRGB32F contribution = principled_diffuse_eval(material, local_view_direction, local_to_light_direction, diffuse_pdf);
        contribution *= diffuse_weight;
        contribution *= layers_throughput;

        out_cumulative_pdf += diffuse_pdf * diffuse_proba;

        return contribution;
    }

    return ColorRGB32F(0.0f);
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_bsdf_eval(const HIPRTRenderData& render_data, const SimplifiedRendererMaterial& material, RayVolumeState& ray_volume_state, const float3& view_direction, float3 shading_normal, const float3& to_light_direction, float& pdf)
{
    pdf = 0.0f;

    // Only the glass lobe is considered when evaluating
    // the BSDF from inside the object so we're going to use that
    // 'outside_object' flag to negate the other lobes if we're
    // inside the object
    bool outside_object = hippt::dot(view_direction, shading_normal) > 0;
    if (!outside_object)
        // For the rest of the computations to be correct, we want the normal
        // in the same hemisphere as the view direction
        shading_normal = -shading_normal;

    float3 T, B;
    build_ONB(shading_normal, T, B);
    float3 local_view_direction = world_to_local_frame(T, B, shading_normal, view_direction);
    float3 local_to_light_direction = world_to_local_frame(T, B, shading_normal, to_light_direction);
    float3 local_half_vector = hippt::normalize(local_view_direction + local_to_light_direction);

    // Rotated ONB for the anisotropic GTR2 evaluation (metallic/glass lobes for example)
    float3 TR, BR;
    build_rotated_ONB(shading_normal, TR, BR, material.anisotropy_rotation * M_PI);
    float3 local_view_direction_rotated = world_to_local_frame(TR, BR, shading_normal, view_direction);
    float3 local_to_light_direction_rotated = world_to_local_frame(TR, BR, shading_normal, to_light_direction);
    float3 local_half_vector_rotated = hippt::normalize(local_view_direction_rotated + local_to_light_direction_rotated);


    // Linear blending weights for the lobes
    // 
    // Everytime we multiply by "outside_object" is because we want to disable
    // the lobe if we're inside the object
    //
    // The layering follows the one of the principled BSDF of blender:
    // [10] https://docs.blender.org/manual/fr/dev/render/shader_nodes/shader/principled.html
    float coat_weight = material.coat * outside_object;
    float sheen_weight = material.sheen * outside_object;
    float metal_weight = material.metallic * outside_object;
    float specular_weight = (1.0f - material.metallic) * (1.0f - material.specular_transmission) * material.specular * outside_object;
    // Diffuse weight is 1.0f because:
    // The diffuse lobe is below the specular lobe. This means that the diffuse lobe
    // will only get the energy that got through the specular lobe and the specular
    // lobe already accounts for the metallic weight and transmission weight
    float diffuse_weight = (1.0f - material.metallic) * (1.0f - material.specular_transmission) * outside_object;
    //float glass_weight = (1.0f - material.metallic) * material.specular_transmission;

    float coat_sample_proba = coat_weight;
    float sheen_sample_proba = sheen_weight;
    float metal_sample_proba = metal_weight;
    float specular_sample_proba = specular_weight;
    float diffuse_sample_proba = diffuse_weight;
    float probability_normalize_factor = coat_sample_proba + sheen_sample_proba + metal_sample_proba + specular_sample_proba + diffuse_sample_proba;

    // For the given to_light_direction, normal, view_direction etc..., what's the probability
    // that the 'principled_bsdf_sample()' function would have sampled the lobe?
    float coat_proba_norm = coat_sample_proba / probability_normalize_factor;
    float sheen_proba_norm = sheen_sample_proba / probability_normalize_factor;
    float metal_proba_norm = metal_sample_proba / probability_normalize_factor;
    float specular_proba_norm = specular_sample_proba / probability_normalize_factor;
    float diffuse_proba_norm = diffuse_sample_proba / probability_normalize_factor;
    //float glass_proba = glass_weight;

    // Keeps track of the remaining light's energy as we traverse layers
    ColorRGB32F layers_throughput = ColorRGB32F(1.0f);
    ColorRGB32F final_color = ColorRGB32F(0.0f);

    float incident_ior = ray_volume_state.incident_mat_index == InteriorStackImpl<InteriorStackStrategy>::MAX_MATERIAL_INDEX ? 1.0 : render_data.buffers.materials_buffer[ray_volume_state.incident_mat_index].ior;
    final_color += internal_eval_coat_layer(material, local_view_direction, local_to_light_direction, local_half_vector, incident_ior, coat_weight, coat_proba_norm, layers_throughput, pdf);
    final_color += internal_eval_sheen_layer(render_data, material, local_view_direction, local_to_light_direction, to_light_direction, shading_normal, incident_ior, sheen_weight, sheen_proba_norm, layers_throughput, pdf);
    final_color += internal_eval_metal_layer(material, local_view_direction_rotated, local_to_light_direction_rotated, local_half_vector_rotated, incident_ior, metal_weight, metal_proba_norm, layers_throughput, pdf);
    final_color += internal_eval_specular_layer(material, local_view_direction_rotated, local_to_light_direction_rotated, local_half_vector_rotated, incident_ior, specular_weight, specular_proba_norm, layers_throughput, pdf);
    final_color += internal_eval_diffuse_layer(material, local_view_direction, local_to_light_direction, diffuse_weight, diffuse_proba_norm, layers_throughput, pdf);

    //// Glass
    //final_color += glass_weight > 0 ? glass_weight * principled_glass_eval(render_data.buffers.materials_buffer, material, ray_volume_state, local_view_direction_rotated, local_to_light_direction_rotated, tmp_pdf) : ColorRGB32F(0.0f);
    //pdf += tmp_pdf * glass_proba;
    //tmp_pdf = 0.0f;

    return final_color;
}

HIPRT_HOST_DEVICE HIPRT_INLINE ColorRGB32F principled_bsdf_sample(const HIPRTRenderData& render_data, const SimplifiedRendererMaterial& material, RayVolumeState& ray_volume_state, const float3& view_direction, const float3& shading_normal, const float3& geometric_normal, float3& output_direction, float& pdf, Xorshift32Generator& random_number_generator)
{
    pdf = 0.0f;

    float3 normal = shading_normal;

    float glass_weight = (1.0f - material.metallic) * material.specular_transmission;
    bool outside_object = hippt::dot(view_direction, normal) > 0;
    if (glass_weight == 0.0f && !outside_object)
    {
        // If we're not sampling the glass lobe so we're checking
        // whether the view direction is below the upper hemisphere around the shading
        // normal or not. This may be the case mainly due to normal mapping / smooth vertex normals. 
        // 
        // See Microfacet-based Normal Mapping for Robust Monte Carlo Path Tracing, Eric Heitz, 2017
        // for some illustrations of the problem and a solution (not implemented here because
        // it requires quite a bit of code and overhead). 
        // 
        // We're flipping the normal instead which is a quick dirty fix solution mentioned
        // in the above mentioned paper.
        // 
        // The Position-free Multiple-bounce Computations for Smith Microfacet BSDFs by 
        // Wang et al. 2022 proposes an alternative position-free solution that even solves
        // the multi-scattering issue of microfacet BRDFs on top of the dark fringes issue we're
        // having here

        normal = reflect_ray(shading_normal, geometric_normal);
        outside_object = true;
    }

    // Computing the weights for sampling the lobes
    float coat_weight = material.coat * outside_object;
    float sheen_weight = material.sheen * outside_object;
    float metal_weight = material.metallic * outside_object;
    float specular_weight = (1.0f - material.metallic) * (1.0f - material.specular_transmission) * material.specular * outside_object;
    // The diffuse lobe is below the specular lobe so it has the same probability of being sampled
    float diffuse_weight = (1.0f - material.metallic) * (1.0f - material.specular_transmission) * outside_object;
    //float metal_weight = (1.0f - material.specular_transmission * (1.0f - material.metallic)) * outside_object;

    float normalize_factor = 1.0f / (coat_weight + sheen_weight + metal_weight + specular_weight + diffuse_weight);
    coat_weight *= normalize_factor;
    sheen_weight *= normalize_factor;
    metal_weight *= normalize_factor;
    specular_weight *= normalize_factor;
    diffuse_weight *= normalize_factor;
    //glass_weight *= normalize_factor;

    float cdf[5];
    cdf[0] = coat_weight;
    cdf[1] = cdf[0] + sheen_weight;
    cdf[2] = cdf[1] + metal_weight;
    cdf[3] = cdf[2] + specular_weight;
    cdf[4] = cdf[3] + diffuse_weight;
    //cdf[4] = cdf[3] + glass_weight;

    float rand_1 = random_number_generator();
    bool sampling_glass_lobe = false;// rand_1 > cdf[3];
    if (sampling_glass_lobe)
    {
        // We're going to sample the glass lobe

        float dot_shading = hippt::dot(view_direction, shading_normal);
        float dot_geometric = hippt::dot(view_direction, geometric_normal);
        if (dot_shading * dot_geometric < 0)
        {
            // The view direction is below the surface normal (probably because of normal mapping / smooth normals).
            // 
            // We're going to flip the normal for the same reason as explained above to avoid black fringes
            // the reason we're also checking for the dot product with the geometric normal here
            // is because in the case of the glass lobe of the BRDF, we could be legitimately having
            // the dot product between the shading normal and the view direction be negative when we're
            // currently travelling inside the surface. To make sure that we're in the case of the black fringes
            // caused by normal mapping and microfacet BRDFs, we're also checking with the geometric normal.
            // 
            // If the view direction isn't below the geometric normal but is below the shading normal, this
            // indicates that we're in the case of the black fringes and we can flip the normal
            // 
            // If both dot products are negative, this means that we're travelling inside the surface
            // and we shouldn't flip the normal
            normal = reflect_ray(shading_normal, geometric_normal);
        }
    }

    if (!sampling_glass_lobe)
        // We're going to sample a reflective lobe so we're poping the stack
        ray_volume_state.interior_stack.pop(false);

    if (hippt::dot(view_direction, normal) < 0)
        // We want the normal in the same hemisphere as the view direction
        // for the rest of the calculations
        normal = -normal;
        
    if (rand_1 < cdf[0])
    {
        float3 TR_coat, BR_coat;
        build_rotated_ONB(normal, TR_coat, BR_coat, material.coat_anisotropy_rotation * M_PI);
        float3 local_view_direction_rotated_coat = world_to_local_frame(TR_coat, BR_coat, normal, view_direction);

        output_direction = local_to_world_frame(TR_coat, BR_coat, normal, principled_coat_sample(material, local_view_direction_rotated_coat, random_number_generator));
    }
    else if (rand_1 < cdf[1])
    {
        float3 T, B;
        build_ONB(normal, T, B);
        float3 local_view_direction = world_to_local_frame(T, B, normal, view_direction);

        output_direction = local_to_world_frame(T, B, normal, principled_sheen_sample(render_data, material, local_view_direction, normal, random_number_generator));
    }
    else if (rand_1 < cdf[2])
    {
        // Rotated ONB for the anisotropic GTR2 evaluation
        float3 TR, BR;
        build_rotated_ONB(normal, TR, BR, material.anisotropy_rotation* M_PI);
        float3 local_view_direction_rotated = world_to_local_frame(TR, BR, normal, view_direction);

        output_direction = local_to_world_frame(TR, BR, normal, principled_metallic_sample(material, local_view_direction_rotated, random_number_generator));
    }
    else if (rand_1 < cdf[3])
    {
        // Rotated ONB for the anisotropic GTR2 evaluation
        float3 TR, BR;
        build_rotated_ONB(normal, TR, BR, material.anisotropy_rotation* M_PI);
        float3 local_view_direction_rotated = world_to_local_frame(TR, BR, normal, view_direction);

        output_direction = local_to_world_frame(TR, BR, normal, principled_specular_sample(material, local_view_direction_rotated, random_number_generator));
    }
    else if (rand_1 < cdf[4])
        // No call to local_to_world_frame() since the sample diffuse functions
        // already returns in world space around the given normal
        output_direction = principled_diffuse_sample(normal, random_number_generator);
    //else
    //    // When sampling the glass lobe, if we're reflecting off the glass, we're going to have to pop the stack.
    //    // This is handled inside glass_sample because we cannot know from here if we refracted or reflected
    //    output_direction = local_to_world_frame(TR, BR, normal, principled_glass_sample(render_data.buffers.materials_buffer, material, ray_volume_state, local_view_direction_rotated, random_number_generator));

    if (hippt::dot(output_direction, shading_normal) < 0 && !sampling_glass_lobe)
        // It can happen that the light direction sampled is below the surface. 
        // We return 0.0 in this case if we didn't sample the glass lobe
        // because no lobe other than the glass lobe allows refractions
        return ColorRGB32F(0.0f);

    // Not using 'normal' here because eval() needs to know whether or not we're inside the surface.
    // This is because is we're inside the surface, we're only going to evaluate the glass lobe.
    // If we were using 'normal', we would always be outside the surface because 'normal' is flipped
    // (a few lines above in the code) so that it is in the same hemisphere as the view direction and
    // eval() will then think that we're always outside the surface even though that's not the case
    return principled_bsdf_eval(render_data, material, ray_volume_state, view_direction, shading_normal, output_direction, pdf);
}

#endif
