/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef GPU_BAKER_CONSTANTS_H
#define GPU_BAKER_CONSTANTS_H

#ifndef __KERNELCC__
#include <string>
#endif

struct GPUBakerConstants
{
	static const int GGX_ESS_TEXTURE_SIZE_COS_THETA_O = 96;
	static const int GGX_ESS_TEXTURE_SIZE_ROUGHNESS = 96;

	static const int GGX_GLASS_ESS_TEXTURE_SIZE_COS_THETA_O = 256;
	static const int GGX_GLASS_ESS_TEXTURE_SIZE_ROUGHNESS = 16;
	static const int GGX_GLASS_ESS_TEXTURE_SIZE_IOR = 128;

	static const int GLOSSY_DIELECTRIC_TEXTURE_SIZE_COS_THETA_O = 128;
	static const int GLOSSY_DIELECTRIC_TEXTURE_SIZE_ROUGHNESS = 64;
	static const int GLOSSY_DIELECTRIC_TEXTURE_SIZE_IOR = 128;

#ifndef __KERNELCC__
	// Not using these on the GPU since they are std::string types: unavailable on the GPU
	// and besides, we don't these paths on the GPU, only the texture sizes
	static constexpr std::string get_GGX_Ess_filename(int texture_size_cos_theta = GPUBakerConstants::GGX_ESS_TEXTURE_SIZE_COS_THETA_O, 
													  int texture_size_roughness = GPUBakerConstants::GGX_ESS_TEXTURE_SIZE_ROUGHNESS)
	{
		return "GGX_Ess_" + std::to_string(texture_size_cos_theta) + "x" + std::to_string(texture_size_roughness) + ".hdr";
	}

	static constexpr std::string get_glossy_dielectric_Ess_filename(int texture_size_cos_theta = GPUBakerConstants::GLOSSY_DIELECTRIC_TEXTURE_SIZE_COS_THETA_O,
																	int texture_size_roughness = GPUBakerConstants::GLOSSY_DIELECTRIC_TEXTURE_SIZE_ROUGHNESS,
																	int texture_size_ior = GPUBakerConstants::GLOSSY_DIELECTRIC_TEXTURE_SIZE_IOR)
	{
		return "Glossy_Ess_" + std::to_string(texture_size_cos_theta) + "x" + std::to_string(texture_size_roughness) + "x" + std::to_string(texture_size_ior) + ".hdr";
	}

	static constexpr std::string get_GGX_glass_Ess_filename(int texture_size_cos_theta = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_COS_THETA_O, 
															int texture_size_roughness = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_ROUGHNESS, 
															int texture_size_ior = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_IOR)
	{
		return "GGX_Glass_Ess_" + std::to_string(texture_size_cos_theta) + "x" + std::to_string(texture_size_roughness) + "x" + std::to_string(texture_size_ior) + ".hdr";
	}

	static constexpr std::string get_GGX_glass_inv_Ess_filename(int texture_size_cos_theta = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_COS_THETA_O, 
																int texture_size_roughness = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_ROUGHNESS, 
																int texture_size_ior = GPUBakerConstants::GGX_GLASS_ESS_TEXTURE_SIZE_IOR)
	{
		return "inv_GGX_Glass_Ess_" + std::to_string(texture_size_cos_theta) + "x" + std::to_string(texture_size_roughness) + "x" + std::to_string(texture_size_ior) + ".hdr";
	}
#endif
};

#endif
