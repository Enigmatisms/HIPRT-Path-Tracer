/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef HOST_DEVICE_RESTIR_DI_SETTINGS_H
#define HOST_DEVICE_RESTIR_DI_SETTINGS_H

#include "Device/includes/ReSTIR/ReSTIR_DI_Reservoir.h"

struct InitialCandidatesSettings
{
	// How many light candidates to resamples during the initial candidates sampling pass
	int number_of_initial_light_candidates = 4;
	// How many BSDF candidates to resamples during the initial candidates sampling pass
	int number_of_initial_bsdf_candidates = 1;

	// Buffer that contains the reservoirs that will hold the reservoir
	// for the initial candidates generated
	ReSTIRDIReservoir* output_reservoirs = nullptr;
};

struct TemporalPassSettings
{
	bool do_temporal_reuse_pass = true;

	// How many neighbors at most to check around the temporal back-projected pixel location 
	// to find a valid neighbor
	int max_neighbor_search_count = 8;
	// Radius around the temporal reprojected location of a pixel in which to look for an
	// acceptable temporal neighbor
	int neighbor_search_radius = 8;

	// The temporal reuse pass resamples the initial candidates as well as the last frame reservoirs which
	// are accessed through this pointer
	ReSTIRDIReservoir* input_reservoirs = nullptr;
	// Buffer that holds the output of the temporal reuse pass
	ReSTIRDIReservoir* output_reservoirs = nullptr;
};

struct SpatialPassSettings
{
	bool do_spatial_reuse_pass = false;

	// How many spatial reuse pass to perform
	int number_of_passes = 1;
	// The radius within which neighbor are going to be reused spatially
	int spatial_reuse_radius = 20;
	// How many neighbors to reuse during the spatial pass
	int spatial_reuse_neighbor_count = 3;

	// Buffer that contains the input reservoirs for the spatial reuse pass
	ReSTIRDIReservoir* input_reservoirs = nullptr;
	// Buffer that contains the output reservoir of the spatial reuse pass
	ReSTIRDIReservoir* output_reservoirs = nullptr;
};

struct ReSTIRDITargetFunctionSettings
{
	// Whether or not to include the geometry term in the target function when resampling neighbors
	// Defaults to false because of numeric instability when dividing by very small distance to light
	bool geometry_term_in_target_function = false;
};

struct ReSTIRDISettings
{
	// Settings for the initial candidates generation pass
	InitialCandidatesSettings initial_candidates;
	// Settings for the temporal reuse pass
	TemporalPassSettings temporal_pass;
	// Settings for the spatial reuse pass
	SpatialPassSettings spatial_pass;
	// Settings for the target function used in all passes of ReSTIR DI
	ReSTIRDITargetFunctionSettings target_function;

	// When finalizing the reservoir in the spatial reuse pass, what value
	// to cap the reservoirs's M value to.
	//
	// The point of this parameter is to avoid too much correlation between frames if using
	// a bias correction that uses confidence weights. Without M-capping, the M value of a reservoir
	// will keep growing exponentially through temporal and spatial reuse and when that exponentially
	// grown M value is used in confidence weights, it results in new samples being very unlikely 
	// to be chosen which in turn results in non-convergence since always the same sample is evaluated
	// for a given pixel.
	//
	// A M-cap value between 5 - 30 is usually good
	int m_cap = 10;

	// User-friendly (for ImGui) normal angle. When resampling a neighbor (temporal or spatial),
	// the normal of the neighbor being re-sampled must be similar to our normal. This angle gives the
	// "similarity threshold". Normals must be within 25 degrees of each other by default
	float normal_similarity_angle_degrees = 25.0f;
	// Precomputed cosine of the angle for use in the shader
	float normal_similarity_angle_precomp = 0.906307787; // Normals must be within 25 degrees by default

	// Threshold used when determining whether a temporal neighbor is acceptable
	// for temporal reuse regarding the spatial proximity of the neighbor and the current
	// point. 
	// This is a world space distance.
	float plane_distance_threshold = 0.1f;

	// How close the roughness of the neighbor's surface must be to ours to resample that neighbor
	// If this value is 0.25f for example, then the roughnesses must be within 0.25f of each other. Simple.
	float roughness_similarity_threshold = 0.25f;

	// Pointer to the buffer that contains the output of all the passes of ReSTIR DI
	// This the buffer that should be used when evaluating direct lighting in the path tracer
	// 
	// This buffer isn't allocated but is actually just a pointer
	// to the buffer that was last used as the output of the resampling
	// passes last frame. 
	// For example if there was spatial reuse in last frame, this buffer
	// is going to be a pointer to the output of the spatial reuse pass
	// If there was only temporal reuse pass last frame, this buffer is going
	// to be a pointer to the output of the temporal reuse pass
	// 
	// This is handy to remember which buffer the temporal reuse pass is going to use
	// as input on the next frame
	ReSTIRDIReservoir* restir_output_reservoirs;
};

#endif
