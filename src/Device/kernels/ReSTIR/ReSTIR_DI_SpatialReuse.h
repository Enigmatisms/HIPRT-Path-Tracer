/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_RESTIR_DI_SPATIAL_REUSE_H
#define DEVICE_RESTIR_DI_SPATIAL_REUSE_H 

#include "Device/includes/Dispatcher.h"
#include "Device/includes/FixIntellisense.h"
#include "Device/includes/Hash.h"
#include "Device/includes/Intersect.h"
#include "Device/includes/LightUtils.h"
#include "Device/includes/Sampling.h"

#include "HostDeviceCommon/HIPRTCamera.h"
#include "HostDeviceCommon/Color.h"
#include "HostDeviceCommon/HitInfo.h"
#include "HostDeviceCommon/RenderData.h"

 /** References:
 *
 * [1] [Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting] https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/
 * [2] [A Gentle Introduction to ReSTIR: Path Reuse in Real-time] https://intro-to-restir.cwyman.org/
 * [3] [A Gentle Introduction to ReSTIR: Path Reuse in Real-time - SIGGRAPH 2023 Presentation Video] https://dl.acm.org/doi/10.1145/3587423.3595511#sec-supp
 * [4] [NVIDIA RTX DI SDK - Github] https://github.com/NVIDIAGameWorks/RTXDI
 * [5] [Generalized Resampled Importance Sampling Foundations of ReSTIR] https://research.nvidia.com/publication/2022-07_generalized-resampled-importance-sampling-foundations-restir
 * [6] [Uniform disk sampling] https://rh8liuqy.github.io/Uniform_Disk.html
 */

#define USE_BALANCE_HEURISTICS 0
#define MIS_LIKE_WEIGHTS 1

template <bool withVisiblity>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const RendererMaterial& material, const RayVolumeState& volume_state, float3 view_direction, float3 shading_point, float3 shading_normal)
{}

template <>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function<0>(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const RendererMaterial& material, const RayVolumeState& volume_state, float3 view_direction, float3 shading_point, float3 shading_normal)
{
	float bsdf_pdf;
	float distance_to_light;
	float3 sample_direction;
	sample_direction = sample.point_on_light_source - shading_point;
	sample_direction = sample_direction / (distance_to_light = hippt::length(sample_direction));

	RayVolumeState trash_volume_state = volume_state;
	ColorRGB32F bsdf_color = bsdf_dispatcher_eval(render_data.buffers.materials_buffer, material, trash_volume_state, view_direction, shading_normal, sample_direction, bsdf_pdf);
	float cosine_term = hippt::max(0.0f, hippt::dot(shading_normal, sample_direction));

	float geometry_term = 1.0f;
	if (render_data.render_settings.restir_di_settings.target_function.geometry_term_in_target_function)
	{
		float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, sample.emissive_triangle_index));
		float cosine_at_light_source = hippt::abs(hippt::dot(sample_direction, light_source_normal));

		geometry_term = cosine_at_light_source / (distance_to_light * distance_to_light);
	}

	int material_index = render_data.buffers.material_indices[sample.emissive_triangle_index];
	ColorRGB32F sample_emission = render_data.buffers.materials_buffer[material_index].emission;

	float target_function = (bsdf_color * sample_emission * cosine_term * geometry_term).luminance();
	if (target_function == 0.0f)
		// Quick exit because computing the visiblity that follows isn't going
		// to change anything to the fact that we have 0.0f target function here
		return 0.0f;

	return target_function;
}

template <>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function<1>(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const RendererMaterial& material, const RayVolumeState& volume_state, float3 view_direction, float3 shading_point, float3 shading_normal)
{
	float bsdf_pdf;
	float distance_to_light;
	float3 sample_direction;
	sample_direction = sample.point_on_light_source - shading_point;
	sample_direction = sample_direction / (distance_to_light = hippt::length(sample_direction));

	RayVolumeState trash_volume_state = volume_state;
	ColorRGB32F bsdf_color = bsdf_dispatcher_eval(render_data.buffers.materials_buffer, material, trash_volume_state, view_direction, shading_normal, sample_direction, bsdf_pdf);
	float cosine_term = hippt::max(0.0f, hippt::dot(shading_normal, sample_direction));

	float geometry_term = 1.0f;
	if (render_data.render_settings.restir_di_settings.target_function.geometry_term_in_target_function)
	{
		float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, sample.emissive_triangle_index));
		float cosine_at_light_source = hippt::abs(hippt::dot(sample_direction, light_source_normal));

		geometry_term = cosine_at_light_source / (distance_to_light * distance_to_light);
	}

	int material_index = render_data.buffers.material_indices[sample.emissive_triangle_index];
	ColorRGB32F sample_emission = render_data.buffers.materials_buffer[material_index].emission;

	float target_function = (bsdf_color * sample_emission * cosine_term * geometry_term).luminance();
	if (target_function == 0.0f)
		// Quick exit because computing the visiblity that follows isn't going
		// to change anything to the fact that we have 0.0f target function here
		return 0.0f;

	hiprtRay shadow_ray;
	shadow_ray.origin = shading_point;
	shadow_ray.direction = sample_direction;

	bool visible = !evaluate_shadow_ray(render_data, shadow_ray, distance_to_light);

	target_function *= visible;

	return target_function;
}

/**
 * Returns the linear index that can be used directly to index a buffer
 * of render_data of the 'neighbor_number'th neighbor that we're going
 * to spatially reuse from
 * 
 * 'neighbor_number' is in [0, neighbor_reuse_count]
 * 'neighbor_reuse_count' is in [1, ReSTIR_DI_Settings.spatial_reuse_neighbor_count]
 * 'neighbor_reuse_radius' is the radius of the disk within which the neighbors are sampled
 * 'center_pixel_coords' is the coordinates of the center pixel that is currently 
 *	 doing the resampling of its neighbors
 * 'res' is the resolution of the viewport. This is used to check whether the generated
 *	 neighbor location is outside of the viewport or not
 * 'cos_sin_theta_rotation' is a pair of float [x, y] with x = cos(random_rotation) and
 *	 y = sin(random_rotation). This is used to rotate the points generated by the Hammersley
 *	 sampler so that not each pixel on the image resample the exact same neighbors (and so
 *	 that a given pixel P resamples different neighbors accros different frame, otherwise
 *	 the Hammersley sampler would always generate the exact same points
 */
HIPRT_HOST_DEVICE HIPRT_INLINE int get_neighbor_pixel_index(int neighbor_number, int neighbor_reuse_count, int neighbor_reuse_radius, int2 center_pixel_coords, int2 res, float2 cos_sin_theta_rotation, Xorshift32Generator& random_number_generator)
{
	int neighbor_pixel_index;

	if (neighbor_number == neighbor_reuse_count)
	{
		// If this is the last neighbor, we set it to ourselves
		// This is why our loop on the neighbors goes up to 'i < NEIGHBOR_REUSE_COUNT + 1'
		// It's so that when i == NEIGHBOR_REUSE_COUNT, we resample ourselves
		neighbor_pixel_index = center_pixel_coords.x + center_pixel_coords.y * res.x;
	}
	else
	{
		// +1 and +1 here because we want to skip the frist point as it is always (0, 0)
		// which means that we would be resampling ourselves (the center pixel) --> 
		// pointless because we already resample ourselves "manually" (that's why there's that
		// "if (neighbor_number == neighbor_reuse_count)" above)
		float2 uv = sample_hammersley_2D(neighbor_reuse_count + 1, neighbor_number + 1);
		// TODO first sample of hammersley is always in the center of the disk so we're reusing ourselves?
		float2 neighbor_offset_in_disk = sample_in_disk_uv(neighbor_reuse_radius, uv);

		// 2D rotation matrix: https://en.wikipedia.org/wiki/Rotation_matrix
		float cos_theta = cos_sin_theta_rotation.x;
		float sin_theta = cos_sin_theta_rotation.y;
		float2 neighbor_offset_rotated = make_float2(neighbor_offset_in_disk.x * cos_theta - neighbor_offset_in_disk.y * sin_theta, neighbor_offset_in_disk.x * sin_theta + neighbor_offset_in_disk.y * cos_theta);
		int2 neighbor_offset_int = make_int2(static_cast<int>(neighbor_offset_rotated.x), static_cast<int>(neighbor_offset_rotated.y));

		int2 neighbor_pixel_coords = center_pixel_coords + neighbor_offset_int;
		if (neighbor_pixel_coords.x < 0 || neighbor_pixel_coords.x >= res.x || neighbor_pixel_coords.y < 0 || neighbor_pixel_coords.y >= res.y)
			// Rejecting the sample if it's outside of the viewport
			return -1;

		neighbor_pixel_index = neighbor_pixel_coords.x + neighbor_pixel_coords.y * res.x;
	}

	return neighbor_pixel_index;
}


#ifdef __KERNELCC__
GLOBAL_KERNEL_SIGNATURE(void) ReSTIR_DI_SpatialReuse(HIPRTRenderData render_data, int2 res, HIPRTCamera camera)
#else
GLOBAL_KERNEL_SIGNATURE(void) inline ReSTIR_DI_SpatialReuse(HIPRTRenderData render_data, int2 res, HIPRTCamera camera, int x, int y)
#endif
{
#ifdef __KERNELCC__
	const uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;
#endif
	uint32_t pixel_index = (x + y * res.x);
	if (pixel_index >= res.x * res.y)
		return;

	// Initializing the random generator
	unsigned int seed;
	if (render_data.render_settings.freeze_random)
		seed = wang_hash(pixel_index + 1);
	else
		seed = wang_hash((pixel_index + 1) * (render_data.render_settings.sample_number + 1) * render_data.random_seed);
	
	Xorshift32Generator random_number_generator(seed);

	ReSTIRDIReservoir* input_reservoir_buffer = render_data.render_settings.restir_di_settings.spatial_pass.input_reservoirs;

	ReSTIRDIReservoir new_reservoir;
	// Center pixel coordinates
	int2 center_pixel_coords = make_int2(x, y);

	// Surface data of the center pixel
	RendererMaterial center_pixel_material = render_data.g_buffer.materials[pixel_index];
	RayVolumeState center_volume_state = render_data.g_buffer.ray_volume_states[pixel_index];
	float3 center_pixel_view_direction = render_data.g_buffer.view_directions[pixel_index];
	float3 center_pixel_shading_normal = render_data.g_buffer.shading_normals[pixel_index];
	float3 center_pixel_shading_point = render_data.g_buffer.first_hits[pixel_index] + center_pixel_shading_normal * 1.0e-4f;

	// Rotation that is going to be used to rotate the points generated by the Hammersley sampler
	// for generating the neighbors location to resample
	float rotation_theta = 2.0f * M_PI * random_number_generator();
	float2 cos_sin_theta_rotation = make_float2(cos(rotation_theta), sin(rotation_theta));

	int selected_neighbor = 0;
	int reused_neighbors_count = render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_neighbor_count;
	// Resampling the neighbors. Using neighbors + 1 here so that
	// we can use the last iteration of the loop to resample ourselves (the center pixel)
	// 
	// See the implementation of get_neighbor_pixel_index() earlier in this file
	for (int neighbor = 0; neighbor < reused_neighbors_count + 1; neighbor++)
	{
		int neighbor_pixel_index = get_neighbor_pixel_index(neighbor, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, random_number_generator);
		if (neighbor_pixel_index == -1)
			// Neighbor out of the viewport
			continue;

		ReSTIRDIReservoir neighbor_reservoir = input_reservoir_buffer[neighbor_pixel_index];
		if (neighbor_reservoir.UCW == 0.0f)
		{
			new_reservoir.M += neighbor_reservoir.M;
			continue;
		}

		float target_function_at_center = 0.0f;
		if (neighbor == reused_neighbors_count)
			// No need to evaluate the center sample at the center pixel, that's exactly
			// the target function of the center reservoir
			target_function_at_center = neighbor_reservoir.sample.target_function;
		else
			target_function_at_center = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_TargetFunctionVisibility>(render_data, neighbor_reservoir.sample, center_pixel_material, center_volume_state, center_pixel_view_direction, center_pixel_shading_point, center_pixel_shading_normal);

		float jacobian_determinant = 1.0f;
		// If the neighbor reservoir is invalid, do not compute the jacobian
		// Also, if this is the last neighbor resample (meaning that it is the sample pixel), 
		// the jacobian is going to be 1.0f so no need to compute
		if (neighbor_reservoir.UCW != 0.0f && neighbor != reused_neighbors_count)
		{
			float distance_to_light_at_center;
			float distance_to_light_at_neighbor;
			float3 to_light_direction_at_center = neighbor_reservoir.sample.point_on_light_source - center_pixel_shading_point;
			float3 to_light_direction_at_neighbor = neighbor_reservoir.sample.point_on_light_source - render_data.g_buffer.first_hits[neighbor_pixel_index];
			to_light_direction_at_center /= (distance_to_light_at_center = hippt::length(to_light_direction_at_center));
			to_light_direction_at_neighbor /= (distance_to_light_at_neighbor = hippt::length(to_light_direction_at_neighbor));

			float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, neighbor_reservoir.sample.emissive_triangle_index));
			float cosine_ratio = hippt::abs(hippt::dot(-to_light_direction_at_center, light_source_normal)) / hippt::abs(hippt::dot(-to_light_direction_at_neighbor, light_source_normal));
			float distance_squared_ratio = (distance_to_light_at_neighbor * distance_to_light_at_neighbor) / (distance_to_light_at_center * distance_to_light_at_center);

			jacobian_determinant = cosine_ratio * distance_squared_ratio;
		}

		float mis_weight = 1.0f;
#if USE_BALANCE_HEURISTICS
		float nume = 0.0f;
		// We already have the target function at the center pixel, adding it to the denom
		float denom = 0.0f;

		for (int j = 0; j < reused_neighbors_count + 1; j++)
		{
			int neighbor_index_j = get_neighbor_pixel_index(j, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, random_number_generator);
			if (neighbor_index_j == -1)
				continue;

			float3 neighbor_shading_normal = render_data.g_buffer.shading_normals[neighbor_index_j];
			float3 neighbor_shading_point = render_data.g_buffer.first_hits[neighbor_index_j] + neighbor_shading_normal * 1.0e-4f;
			float3 neighbor_view_direction = render_data.g_buffer.view_directions[neighbor_index_j];
			SimplifiedRendererMaterial neighbor_material = render_data.g_buffer.materials[neighbor_index_j];
			RayVolumeState neighbor_ray_volume_state = render_data.g_buffer.ray_volume_states[neighbor_index_j];

			float target_function_at_j = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_SpatialReuseBiasUseVisiblity>(render_data, neighbor_reservoir.sample, neighbor_material, neighbor_ray_volume_state, neighbor_view_direction, neighbor_shading_point, neighbor_shading_normal);
			denom += target_function_at_j;
			if (j == neighbor)
				nume = target_function_at_j;
		}

		if (denom == 0.0f)
			mis_weight = 0.0f;
		else
			mis_weight = nume / denom;
#elif MIS_LIKE_WEIGHTS
		mis_weight = 1.0f;
#else
		mis_weight = neighbor_reservoir.M;
#endif

		// Combining as in Alg. 6 of the paper
		if (new_reservoir.combine_with(neighbor_reservoir, mis_weight, target_function_at_center, jacobian_determinant, random_number_generator))
			selected_neighbor = neighbor;
	}


	// Unbiased normalization term as in ReSTIR 2019 Alg. 6
	float Z = 0.0f;
	float mis_like_denom = 0.0f;
	float mis_like_nume = 0.0f;

	// Now checking how many of our neighbors could have produced the sample that we just picked
	if (new_reservoir.weight_sum > 0.0f)
	{
		for (int neighbor = 0; neighbor < reused_neighbors_count + 1; neighbor++)
		{
			int neighbor_pixel_index = get_neighbor_pixel_index(neighbor, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, random_number_generator);
			if (neighbor_pixel_index == -1)
				// Neighbor out of the viewport
				continue;

			float target_function_at_neighbor = 0.0f;
			if (neighbor == reused_neighbors_count)
			{
				// The last neighbor is the center pixel.
				// We want to evaluate the target function of the picked sample at the center pixel.
				// We already have the value of the target function in the reservoir sample
				target_function_at_neighbor = new_reservoir.sample.target_function;
			}
			else
			{
				// Getting the surface data at the neighbor
				RendererMaterial neighbor_material = render_data.g_buffer.materials[neighbor_pixel_index];
				RayVolumeState neighbor_ray_volume_state = render_data.g_buffer.ray_volume_states[neighbor_pixel_index];
				float3 neighbor_view_direction = render_data.g_buffer.view_directions[neighbor_pixel_index];
				float3 neighbor_shading_normal = render_data.g_buffer.shading_normals[neighbor_pixel_index];
				float3 neighbor_shading_point = render_data.g_buffer.first_hits[neighbor_pixel_index] + neighbor_shading_normal * 1.0e-4f;

				target_function_at_neighbor = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_SpatialReuseBiasUseVisiblity>(render_data, new_reservoir.sample, neighbor_material, neighbor_ray_volume_state, neighbor_view_direction, neighbor_shading_point, neighbor_shading_normal);
			}

			if (target_function_at_neighbor > 0.0f)
			{
				// If the neighbor could have produced this sample...
				ReSTIRDIReservoir neighbor_reservoir = input_reservoir_buffer[neighbor_pixel_index];

				// ... adding M to the Z normalization term
				// TODO add the possibility through ImGui to choose whether we're using confidence
				// weights MIS weights (adding M to the numerator and Z denom) or just 1 weight for
				// each sample (+ 1 to numerator each time, still Z denom but only +1 for each valid
				// sample. Basically the exact same as confidence weights but forcing M=1 for every
				// reused reservoir)
				Z += neighbor_reservoir.M;

				if (neighbor == selected_neighbor)
					mis_like_nume += target_function_at_neighbor * neighbor_reservoir.M;
				mis_like_denom += target_function_at_neighbor * neighbor_reservoir.M;
			}
		}
	}


	// Compute the unbiased contribution weight using 1/Z normalization weight as in ReSTIR 2019 Alg. 6
#if USE_BALANCE_HEURISTICS
	new_reservoir.end();
#elif MIS_LIKE_WEIGHTS
	new_reservoir.end_normalized(mis_like_nume, mis_like_denom);
#else
	new_reservoir.end_normalized(1.0f, Z);
#endif

	render_data.render_settings.restir_di_settings.spatial_pass.output_reservoirs[pixel_index] = new_reservoir;
}

#endif
