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
#include "Device/includes/ReSTIR/DI/SpatialMISWeight.h"
#include "Device/includes/ReSTIR/DI/SpatialNormalizationWeight.h"
#include "Device/includes/ReSTIR/DI/Surface.h"
#include "Device/includes/ReSTIR/DI/Utils.h"
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
 * [7] [Reddit Post for the Jacobian term needed] https://www.reddit.com/r/GraphicsProgramming/comments/1eo5hqr/restir_di_light_sample_pdf_confusion/
 * [8] [Rearchitecting Spatiotemporal Resampling for Production] https://research.nvidia.com/publication/2021-07_rearchitecting-spatiotemporal-resampling-production
 */

HIPRT_HOST_DEVICE HIPRT_INLINE void get_spatial_reuse_normalization_denominator_numerator(const HIPRTRenderData& render_data, 
	const ReSTIRDIReservoir& new_reservoir, const ReSTIRDISurface& center_pixel_surface, 
	int selected_neighbor, int reused_neighbors_count, 
	int2 center_pixel_coords, int2 res, 
	float2 cos_sin_theta_rotation, 
	Xorshift32Generator& random_number_generator, float& out_normalization_nume, float& out_normalization_denom)
{
	if (new_reservoir.weight_sum <= 0)
	{
		// Invalid reservoir, returning directly
		out_normalization_nume = 1.0;
		out_normalization_denom = 1.0f;

		return;
	}

#if ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_1_OVER_M
	// 1/M MIS weights are basically confidence weights only i.e. c_i / sum(c_j) with
	// c_i = r_i.M

	out_normalization_nume = 1.0f;
	// We're simply going to divide by the sum of all the M values of all the neighbors we resampled (including the center pixel)
	// so we're only going to set the denominator to that and the numerator isn't going to change
	out_normalization_denom = 0.0f;
	for (int neighbor = 0; neighbor < reused_neighbors_count + 1; neighbor++)
	{
		int neighbor_pixel_index = get_spatial_neighbor_pixel_index(render_data, neighbor, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, Xorshift32Generator(render_data.random_seed));
		if (neighbor_pixel_index == -1)
			// Neighbor out of the viewport
			continue;

		int center_pixel_index = center_pixel_coords.x + center_pixel_coords.y * res.x;
		if (!check_neighbor_similarity_heuristics(render_data, neighbor_pixel_index, center_pixel_index, center_pixel_surface.shading_point, center_pixel_surface.shading_normal))
			continue;

		ReSTIRDIReservoir neighbor_reservoir = render_data.render_settings.restir_di_settings.spatial_pass.input_reservoirs[neighbor_pixel_index];
		out_normalization_denom += neighbor_reservoir.M;
	}
#elif ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_1_OVER_Z || ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_LIKE || ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_LIKE_CONFIDENCE_WEIGHTS
	// Checking how many of our neighbors could have produced the sample that we just picked
	// and we're going to divide by the sum of M values of those neighbors
	out_normalization_denom = 0.0f;
#if ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_1_OVER_Z
	out_normalization_nume = 1.0f;
#else
	out_normalization_nume = 0.0f;
#endif

	for (int neighbor = 0; neighbor < reused_neighbors_count + 1; neighbor++)
	{
		int neighbor_pixel_index = get_spatial_neighbor_pixel_index(render_data, neighbor, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, Xorshift32Generator(render_data.random_seed));
		if (neighbor_pixel_index == -1)
			// Neighbor out of the viewport
			continue;

		int center_pixel_index = center_pixel_coords.x + center_pixel_coords.y * res.x;
		if (!check_neighbor_similarity_heuristics(render_data, neighbor_pixel_index, center_pixel_index, center_pixel_surface.shading_point, center_pixel_surface.shading_normal))
			continue;

		// Getting the surface data at the neighbor
		ReSTIRDISurface neighbor_surface = get_pixel_surface(render_data, neighbor_pixel_index);

		float target_function_at_neighbor = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_BiasCorrectionUseVisiblity>(render_data, new_reservoir.sample, neighbor_surface);

		if (target_function_at_neighbor > 0.0f)
		{
			// If the neighbor could have produced this sample...
			ReSTIRDIReservoir neighbor_reservoir = render_data.render_settings.restir_di_settings.spatial_pass.input_reservoirs[neighbor_pixel_index];

#if ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_1_OVER_Z
			out_normalization_denom += neighbor_reservoir.M;
#elif ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_LIKE
			if (neighbor == selected_neighbor)
				out_normalization_nume += target_function_at_neighbor;
			out_normalization_denom += target_function_at_neighbor;
#elif ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_LIKE_CONFIDENCE_WEIGHTS
			if (neighbor == selected_neighbor)
				out_normalization_nume += target_function_at_neighbor;
			out_normalization_denom += target_function_at_neighbor * neighbor_reservoir.M;
#endif
		}
	}
#elif ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_GBH || ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_MIS_GBH_CONFIDENCE_WEIGHTS
	// Nothing more to normalize, everything is already handled when resampling the neighbors with balance heuristic MIS weights in the m_i terms
	out_normalization_nume = 1.0f;
	out_normalization_denom = 1.0f;
#else
#error "Unsupported bias correction mode in ReSTIR DI spatial reuse get_normalization_denominator_numerator()"
#endif
}

#ifdef __KERNELCC__
GLOBAL_KERNEL_SIGNATURE(void) ReSTIR_DI_SpatialReuse(HIPRTRenderData render_data, int2 res)
#else
GLOBAL_KERNEL_SIGNATURE(void) inline ReSTIR_DI_SpatialReuse(HIPRTRenderData render_data, int2 res, int x, int y)
#endif
{
#ifdef __KERNELCC__
	const uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;
#endif
	uint32_t center_pixel_index = (x + y * res.x);
	if (center_pixel_index >= res.x * res.y)
		return;

	if (!render_data.aux_buffers.pixel_active[center_pixel_index])
		// Pixel inactive because of adaptive sampling, returning
		return;

	// Initializing the random generator
	unsigned int seed;
	if (render_data.render_settings.freeze_random)
		seed = wang_hash(center_pixel_index + 1);
	else
		seed = wang_hash((center_pixel_index + 1) * (render_data.render_settings.sample_number + 1) * render_data.random_seed);
	Xorshift32Generator random_number_generator(seed);

	ReSTIRDIReservoir* input_reservoir_buffer = render_data.render_settings.restir_di_settings.spatial_pass.input_reservoirs;

	ReSTIRDIReservoir new_reservoir;
	// Center pixel coordinates
	int2 center_pixel_coords = make_int2(x, y);
	// Surface data of the center pixel
	ReSTIRDISurface center_pixel_surface = get_pixel_surface(render_data, center_pixel_index);


	// Rotation that is going to be used to rotate the points generated by the Hammersley sampler
	// for generating the neighbors location to resample
	float rotation_theta;
	if (render_data.render_settings.restir_di_settings.spatial_pass.do_neighbor_rotation)
		rotation_theta = 2.0f * M_PI * random_number_generator();
	else
		rotation_theta = 0.0f;

	float2 cos_sin_theta_rotation = make_float2(cos(rotation_theta), sin(rotation_theta));

	int selected_neighbor = 0;
	int reused_neighbors_count = render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_neighbor_count;
	// Resampling the neighbors. Using neighbors + 1 here so that
	// we can use the last iteration of the loop to resample ourselves (the center pixel)
	// 
	// See the implementation of get_spatial_neighbor_pixel_index() in ReSTIR/DI/Utils.h
	for (int neighbor = 0; neighbor < reused_neighbors_count + 1; neighbor++)
	{
		int neighbor_pixel_index = get_spatial_neighbor_pixel_index(render_data, neighbor, reused_neighbors_count, render_data.render_settings.restir_di_settings.spatial_pass.spatial_reuse_radius, center_pixel_coords, res, cos_sin_theta_rotation, Xorshift32Generator(render_data.random_seed));
		if (neighbor_pixel_index == -1)
			// Neighbor out of the viewport
			continue;

		ReSTIRDIReservoir neighbor_reservoir = input_reservoir_buffer[neighbor_pixel_index];
		if (neighbor_reservoir.UCW == 0.0f)
		{
			// Nothing to do here, just take the M of the resampled neighbor into account.
			// This is basically euiqvalent to combining the reservoir with the
			// new_reservoir.combine_with() function knowing that the target function will
			// be 0.0f (because there's no neighbor reservoir sample)
			new_reservoir.M += neighbor_reservoir.M;

			continue;
		}

		if (!check_neighbor_similarity_heuristics(render_data, neighbor_pixel_index, center_pixel_index, center_pixel_surface.shading_point, center_pixel_surface.shading_normal))
			continue;

		float target_function_at_center = 0.0f;
		if (neighbor == reused_neighbors_count)
			// No need to evaluate the center sample at the center pixel, that's exactly
			// the target function of the center reservoir
			target_function_at_center = neighbor_reservoir.sample.target_function;
		else
			target_function_at_center = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_TargetFunctionVisibility>(render_data, neighbor_reservoir.sample, center_pixel_surface);

		float jacobian_determinant = 1.0f;
		// If the neighbor reservoir is invalid, do not compute the jacobian
		// Also, if this is the last neighbor resample (meaning that it is the sample pixel), 
		// the jacobian is going to be 1.0f so no need to compute
		if (target_function_at_center > 0.0f && neighbor_reservoir.UCW != 0.0f && neighbor != reused_neighbors_count)
		{
			// The reconnection shift is what is implicitely used in ReSTIR DI. We need this because
			// the initial light sample candidates that we generate on the area of the lights have an
			// area measure PDF. This area measure PDF is converted to solid angle in the initial candidates
			// sampling routine by multiplying by the distance squared and dividing by the cosine
			// angle at the light source. However, a PDF in solid angle measure is only viable at a
			// given point. We say "solid angle with respect to the shading point". This means that
			// reusing a light sample with PDF (the UCW of the neighbor reservoir) in solid angle
			// from a neighbor is invalid since that PDF is only valid at the neighbor point, not
			// at the point we're resampling from (the center pixel). We thus need to convert from the
			// "solid angle PDF at the neighbor" to the solid angle at the center pixel and we do
			// that by multiplying by the jacobian determinant of the reconnection shift in solid
			// angle, Eq. 52 of 2022, "Generalized Resampled Importance Sampling".
			jacobian_determinant = get_jacobian_determinant_reconnection_shift(render_data, neighbor_reservoir, center_pixel_surface.shading_point, neighbor_pixel_index);

			if (jacobian_determinant == -1.0f)
			{
				new_reservoir.M += neighbor_reservoir.M;

				// The sample was too dissimilar and so we're rejecting it
				continue;
			}
		}

		ReSTIRDISpatialResamplingMISWeight<ReSTIR_DI_BiasCorrectionWeights> resampling_mis_weight;
		float mis_weight = 1.0f;
		if (target_function_at_center > 0.0f)
			// No need to compute the MIS weight if the target function is 0.0f because we're never going to pick
			// that sample anyway when combining the reservoir since the resampling weight will be 0.0f because of
			// the multiplication by the target function that is 0.0f
			mis_weight = resampling_mis_weight.get_resampling_MIS_weight(render_data,
				neighbor_reservoir, center_pixel_surface, 
				neighbor, reused_neighbors_count, 
				center_pixel_coords, res, 
				cos_sin_theta_rotation, 
				random_number_generator);

		// Combining as in Alg. 6 of the paper
		if (new_reservoir.combine_with(neighbor_reservoir, mis_weight, target_function_at_center, jacobian_determinant, random_number_generator))
			selected_neighbor = neighbor;
		new_reservoir.sanity_check(center_pixel_coords);
	}

	float normalization_numerator = 1.0f;
	float normalization_denominator = 1.0f;

	get_spatial_reuse_normalization_denominator_numerator(render_data, 
		new_reservoir, 
		center_pixel_surface, 
		selected_neighbor, reused_neighbors_count, 
		center_pixel_coords, res, 
		cos_sin_theta_rotation, 
		random_number_generator, normalization_numerator, normalization_denominator);

	new_reservoir.end_with_normalization(normalization_numerator, normalization_denominator);
	new_reservoir.sanity_check(center_pixel_coords);
	// M-capping
	if (render_data.render_settings.restir_di_settings.m_cap > 0)
		new_reservoir.M = hippt::min(render_data.render_settings.restir_di_settings.m_cap, new_reservoir.M);

#if ReSTIR_DI_DoVisibilityReuse && ReSTIR_DI_BiasCorrectionWeights == RESTIR_DI_BIAS_CORRECTION_1_OVER_Z
	// Why is this needed?
	//
	// Picture the case where we have visibility reuse (at the end of the initial candidates sampling pass),
	// visibility term in the bias correction target function (when counting the neighbors that could
	// have produced the picked sample) and 2 spatial reuse passes.
	//
	// The first spatial reuse pass reuses from samples that were produced with visibility in mind
	// (because of the visibility reuse pass that discards occluded samples). This means that we need
	// the visibility in the target function used when counting the neighbors that could have produced
	// the picked sample otherwise we may think that our neighbor could have produced the picked
	// sample where actually it couldn't because the sample is occluded at the neighbor. We would
	// then have a Z denominator (with 1/Z weights) that is too large and we'll end up with darkening.
	//
	// Now at the end of the first spatial reuse pass, the center pixel ends up with a sample that may
	// or may not be occluded from the center's pixel point of view. We didn't include the visibility
	// in the target function when resampling the neighbors (only when counting the "corect" neighbors
	// but that's all) so we are not giving a 0 weight to occluded resampled neighbors --> it is possible
	// that we picked an occluded sample.
	//
	// In the second spatial reuse pass, we are now going to resample from our neighbors and get some
	// samples that were not generated with occlusion in mind (because resampling target function of
	// the first spatial reuse doesn't include visibility). Yet, we are going to weight them with occlusion
	// in mind. This means that we are probably going to discard samples because of occlusion that could
	// have been generated because they are generated without occlusion test. We end up discarding too many
	// samples --> brightening bias.
	//
	// With the visibility reuse at the end of each spatial pass, we force samples at the end of each
	// spatial reuse to take visibility into account so that when we weight them with visibility testing,
	// everything goes well
	if (render_data.render_settings.restir_di_settings.spatial_pass.number_of_passes > 1)
		ReSTIR_DI_visibility_reuse(render_data, new_reservoir, center_pixel_surface.shading_point);
#endif

	render_data.render_settings.restir_di_settings.spatial_pass.output_reservoirs[center_pixel_index] = new_reservoir;
}

#endif
