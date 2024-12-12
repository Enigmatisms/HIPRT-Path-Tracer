/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef HOST_DEVICE_COMMON_BSDFS_DATA_H
#define HOST_DEVICE_COMMON_BSDFS_DATA_H

 /**
  * What masking-shadowing term to use with the GGX NDF.
  *
  * 'HeightCorrelated' is a little be more precise and
  * corect than 'HeightUncorrelated' so it should basically
  * always be preferred.
  * 
  * This is basically only for experimentation purposes
  */
enum GGXMaskingShadowingFlavor
{
	HeightCorrelated,
	HeightUncorrelated
};

struct BRDFsData
{
	bool white_furnace_mode = false;
	bool white_furnace_mode_turn_off_emissives = true;

	// 32x32 texture containing the precomputed parameters of the LTC
	// fitted to approximate the SSGX sheen volumetric layer.
	// See SheenLTCFittedParameters.h
	void* sheen_ltc_parameters_texture = nullptr;

	// 2D texture for the precomputed directional albedo
	// for the GGX BRDFs used in the principled BSDF for energy conservation
	// of conductors
	void* GGX_Ess = nullptr;

	// 3D texture for the precomputed directional albedo of the base layer
	// of the principled BSDF (specular GGX layer + diffuse below)
	void* glossy_dielectric_Ess = nullptr;

	// 3D texture (cos_theta_o, roughness, relative_eta) for the precomputed
	// directional albedo used for energy conservation of glass objects when
	// entering a medium
	void* GGX_Ess_glass = nullptr;
	// Table when leaving a medium
	void* GGX_Ess_glass_inverse = nullptr;

	// Whether or not to use the texture unit's hardware texel interpolation
	// when fetching the LUTs. It's faster but less precise.
	bool use_hardware_tex_interpolation = false;

	// Whether or not to approximate energy compensation for the clearcoat layer
	bool clearcoat_compensation_approximation = true;

	GGXMaskingShadowingFlavor GGX_masking_shadowing = GGXMaskingShadowingFlavor::HeightCorrelated;
};

#endif