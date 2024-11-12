/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#include "Compiler/GPUKernel.h"
#include "Compiler/GPUKernelCompilerOptions.h"
#include "HostDeviceCommon/KernelOptions.h"

#include <cassert>

/**
 * Defining the strings that go with the option so that they can be passed to the shader compiler
 * with the -D<string>=<value> option.
 * 
 * The strings used here must match the ones used in KernelOptions.h
 */
const std::string GPUKernelCompilerOptions::USE_SHARED_STACK_BVH_TRAVERSAL = "UseSharedStackBVHTraversal";
const std::string GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_SIZE = "SharedStackBVHTraversalSize";
const std::string GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_BLOCK_SIZE = "SharedStackBVHTraversalBlockSize";

const std::string GPUKernelCompilerOptions::BSDF_OVERRIDE = "BSDFOverride";
const std::string GPUKernelCompilerOptions::PRINCIPLED_BSDF_DIFFUSE_LOBE = "PrincipledBSDFDiffuseLobe";
const std::string GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING = "PrincipledBSDFGGXUseMultipleScattering";
const std::string GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING_DO_FRESNEL = "PrincipledBSDFGGXUseMultipleScatteringDoFresnel";
const std::string GPUKernelCompilerOptions::GGX_SAMPLE_FUNCTION = "PrincipledBSDFAnisotropicGGXSampleFunction";
const std::string GPUKernelCompilerOptions::INTERIOR_STACK_STRATEGY = "InteriorStackStrategy";
const std::string GPUKernelCompilerOptions::NESTED_DIELETRCICS_STACK_SIZE_OPTION = "NestedDielectricsStackSize";

const std::string GPUKernelCompilerOptions::DIRECT_LIGHT_SAMPLING_STRATEGY = "DirectLightSamplingStrategy";
const std::string GPUKernelCompilerOptions::RIS_USE_VISIBILITY_TARGET_FUNCTION = "RISUseVisiblityTargetFunction";
const std::string GPUKernelCompilerOptions::ENVMAP_SAMPLING_STRATEGY = "EnvmapSamplingStrategy";
const std::string GPUKernelCompilerOptions::ENVMAP_SAMPLING_DO_BSDF_MIS = "EnvmapSamplingDoBSDFMIS";

const std::string GPUKernelCompilerOptions::RESTIR_DI_INITIAL_TARGET_FUNCTION_VISIBILITY = "ReSTIR_DI_InitialTargetFunctionVisibility";
const std::string GPUKernelCompilerOptions::RESTIR_DI_SPATIAL_TARGET_FUNCTION_VISIBILITY = "ReSTIR_DI_SpatialTargetFunctionVisibility";
const std::string GPUKernelCompilerOptions::RESTIR_DI_DO_VISIBILITY_REUSE = "ReSTIR_DI_DoVisibilityReuse";
const std::string GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_USE_VISIBILITY = "ReSTIR_DI_BiasCorrectionUseVisibility";
const std::string GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_WEIGHTS = "ReSTIR_DI_BiasCorrectionWeights";
const std::string GPUKernelCompilerOptions::RESTIR_DI_LATER_BOUNCES_SAMPLING_STRATEGY = "ReSTIR_DI_LaterBouncesSamplingStrategy";
const std::string GPUKernelCompilerOptions::RESTIR_DI_DO_LIGHTS_PRESAMPLING = "ReSTIR_DI_DoLightsPresampling";

const std::unordered_set<std::string> GPUKernelCompilerOptions::ALL_MACROS_NAMES = {
	GPUKernelCompilerOptions::USE_SHARED_STACK_BVH_TRAVERSAL,
	GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_SIZE,
	GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_BLOCK_SIZE,

	GPUKernelCompilerOptions::BSDF_OVERRIDE,
	GPUKernelCompilerOptions::PRINCIPLED_BSDF_DIFFUSE_LOBE,
	GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING,
	GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING_DO_FRESNEL,
	GPUKernelCompilerOptions::GGX_SAMPLE_FUNCTION,
	GPUKernelCompilerOptions::INTERIOR_STACK_STRATEGY,
	GPUKernelCompilerOptions::NESTED_DIELETRCICS_STACK_SIZE_OPTION,

	GPUKernelCompilerOptions::DIRECT_LIGHT_SAMPLING_STRATEGY,
	GPUKernelCompilerOptions::RIS_USE_VISIBILITY_TARGET_FUNCTION,
	GPUKernelCompilerOptions::ENVMAP_SAMPLING_STRATEGY,
	GPUKernelCompilerOptions::ENVMAP_SAMPLING_DO_BSDF_MIS,

	GPUKernelCompilerOptions::RESTIR_DI_INITIAL_TARGET_FUNCTION_VISIBILITY,
	GPUKernelCompilerOptions::RESTIR_DI_SPATIAL_TARGET_FUNCTION_VISIBILITY,
	GPUKernelCompilerOptions::RESTIR_DI_DO_VISIBILITY_REUSE,
	GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_USE_VISIBILITY,
	GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_WEIGHTS,
	GPUKernelCompilerOptions::RESTIR_DI_LATER_BOUNCES_SAMPLING_STRATEGY,
	GPUKernelCompilerOptions::RESTIR_DI_DO_LIGHTS_PRESAMPLING,
};

GPUKernelCompilerOptions::GPUKernelCompilerOptions()
{
	// Mandatory options that every kernel must have so we're
	// adding them here with their default values
	m_options_macro_map[GPUKernelCompilerOptions::USE_SHARED_STACK_BVH_TRAVERSAL] = std::make_shared<int>(UseSharedStackBVHTraversal);
	m_options_macro_map[GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_SIZE] = std::make_shared<int>(SharedStackBVHTraversalSize);
	m_options_macro_map[GPUKernelCompilerOptions::SHARED_STACK_BVH_TRAVERSAL_BLOCK_SIZE] = std::make_shared<int>(SharedStackBVHTraversalBlockSize);

	m_options_macro_map[GPUKernelCompilerOptions::BSDF_OVERRIDE] = std::make_shared<int>(BSDFOverride);
	m_options_macro_map[GPUKernelCompilerOptions::PRINCIPLED_BSDF_DIFFUSE_LOBE] = std::make_shared<int>(PrincipledBSDFDiffuseLobe);
	m_options_macro_map[GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING] = std::make_shared<int>(PrincipledBSDFGGXUseMultipleScattering);
	m_options_macro_map[GPUKernelCompilerOptions::PRINCIPLED_BSDF_GGX_MULTIPLE_SCATTERING_DO_FRESNEL] = std::make_shared<int>(PrincipledBSDFGGXUseMultipleScatteringDoFresnel);
	m_options_macro_map[GPUKernelCompilerOptions::GGX_SAMPLE_FUNCTION] = std::make_shared<int>(PrincipledBSDFAnisotropicGGXSampleFunction);
	m_options_macro_map[GPUKernelCompilerOptions::INTERIOR_STACK_STRATEGY] = std::make_shared<int>(InteriorStackStrategy);
	m_options_macro_map[GPUKernelCompilerOptions::NESTED_DIELETRCICS_STACK_SIZE_OPTION] = std::make_shared<int>(NestedDielectricsStackSize);

	m_options_macro_map[GPUKernelCompilerOptions::DIRECT_LIGHT_SAMPLING_STRATEGY] = std::make_shared<int>(DirectLightSamplingStrategy);
	m_options_macro_map[GPUKernelCompilerOptions::RIS_USE_VISIBILITY_TARGET_FUNCTION] = std::make_shared<int>(RISUseVisiblityTargetFunction);
	m_options_macro_map[GPUKernelCompilerOptions::ENVMAP_SAMPLING_STRATEGY] = std::make_shared<int>(EnvmapSamplingStrategy);
	m_options_macro_map[GPUKernelCompilerOptions::ENVMAP_SAMPLING_DO_BSDF_MIS] = std::make_shared<int>(EnvmapSamplingDoBSDFMIS);

	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_INITIAL_TARGET_FUNCTION_VISIBILITY] = std::make_shared<int>(ReSTIR_DI_InitialTargetFunctionVisibility);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_SPATIAL_TARGET_FUNCTION_VISIBILITY] = std::make_shared<int>(ReSTIR_DI_SpatialTargetFunctionVisibility);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_DO_VISIBILITY_REUSE] = std::make_shared<int>(ReSTIR_DI_DoVisibilityReuse);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_USE_VISIBILITY] = std::make_shared<int>(ReSTIR_DI_BiasCorrectionUseVisibility);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_BIAS_CORRECTION_WEIGHTS] = std::make_shared<int>(ReSTIR_DI_BiasCorrectionWeights);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_LATER_BOUNCES_SAMPLING_STRATEGY] = std::make_shared<int>(ReSTIR_DI_LaterBouncesSamplingStrategy);
	m_options_macro_map[GPUKernelCompilerOptions::RESTIR_DI_DO_LIGHTS_PRESAMPLING] = std::make_shared<int>(ReSTIR_DI_DoLightsPresampling);

	// Making sure we didn't forget to fill the ALL_MACROS_NAMES vector with all the options that exist
	assert(GPUKernelCompilerOptions::ALL_MACROS_NAMES.size() == m_options_macro_map.size());
}

GPUKernelCompilerOptions::GPUKernelCompilerOptions(const GPUKernelCompilerOptions& other)
{
	*this = other;
}

GPUKernelCompilerOptions GPUKernelCompilerOptions::operator=(const GPUKernelCompilerOptions& other)
{
	GPUKernelCompilerOptions copy;
	copy.clear();

	copy.m_additional_include_directories = other.m_additional_include_directories;
	for (auto& pair : other.m_options_macro_map)
		// Creating new shared ptr for the copy
		copy.m_options_macro_map[pair.first] = std::make_shared<int>(*pair.second);

	for (auto& pair : other.m_custom_macro_map)
		// Creating new shared ptr for the copy
		copy.m_custom_macro_map[pair.first] = std::make_shared<int>(*pair.second);

	m_additional_include_directories = copy.m_additional_include_directories;
	m_options_macro_map = copy.m_options_macro_map;
	m_custom_macro_map = copy.m_custom_macro_map;

	return copy;
}

std::vector<std::string> GPUKernelCompilerOptions::get_all_macros_as_std_vector_string()
{
	std::vector<std::string> macros;

	for (auto macro_key_value : m_options_macro_map)
		macros.push_back("-D " + macro_key_value.first + "=" + std::to_string(*macro_key_value.second));

	for (auto macro_key_value : m_custom_macro_map)
		macros.push_back("-D " + macro_key_value.first + "=" + std::to_string(*macro_key_value.second));

	return macros;
}

std::vector<std::string> GPUKernelCompilerOptions::get_relevant_macros_as_std_vector_string(const GPUKernel* kernel) const
{
	std::vector<std::string> macros;

	// Looping on all the options macros and checking if the kernel uses that option macro,
	// only adding the macro to the returned vector if the kernel uses that option macro
	for (auto macro_key_value : m_options_macro_map)
		if (kernel->uses_macro(macro_key_value.first))
			macros.push_back("-D " + macro_key_value.first + "=" + std::to_string(*macro_key_value.second));

	// Adding all the custom macros without conditions
	for (auto macro_key_value : m_custom_macro_map)
		macros.push_back("-D " + macro_key_value.first + "=" + std::to_string(*macro_key_value.second));

	std::vector<std::string> additional_macros = kernel->get_additional_compiler_macros();
	for (const std::string& additional_macro : additional_macros)
		macros.push_back(additional_macro);

	return macros;
}

void GPUKernelCompilerOptions::set_macro_value(const std::string& name, int value)
{
	if (ALL_MACROS_NAMES.find(name) != ALL_MACROS_NAMES.end())
	{
		if (m_options_macro_map.find(name) != m_options_macro_map.end())
			// If you could find the name in the options-macro, settings its value
			*m_options_macro_map[name] = value;
		else
			// Otherwise, creating it
			m_options_macro_map[name] = std::make_shared<int>(value);
	}
	else
	{
		// Otherwise, this is a user defined macro, putting it in the custom macro map
		if (m_custom_macro_map.find(name) != m_custom_macro_map.end())
			// Updating the macro's alue if it already exists
			*m_custom_macro_map[name] = value;
		else
			// Creating it otherwise
			m_custom_macro_map[name] = std::make_shared<int>(value);
	}
}

void GPUKernelCompilerOptions::remove_macro(const std::string& name)
{
	// Only removing from the custom macro map because we cannot remove the options-macro
	m_custom_macro_map.erase(name);
}

bool GPUKernelCompilerOptions::has_macro(const std::string& name)
{
	// Only checking the custom macro map because we cannot remove the options-macro so it makes
	// no sense to check whether this instance has the macro "InteriorStackStrategy"
	// for example, it will always be yes
	return m_custom_macro_map.find(name) != m_custom_macro_map.end();
}

int GPUKernelCompilerOptions::get_macro_value(const std::string& name) const
{
	auto find = m_options_macro_map.find(name);

	if (find == m_options_macro_map.end())
	{
		// Wasn't found in the options-macro, trying in the custom macros
		auto find_custom = m_custom_macro_map.find(name);
		if (find_custom == m_custom_macro_map.end())
			return std::numeric_limits<int>::min();
		else
			return *find_custom->second;
	}
	else
		return *find->second;
}

const std::shared_ptr<int> GPUKernelCompilerOptions::get_pointer_to_macro_value(const std::string& name) const
{
	auto find = m_options_macro_map.find(name);

	if (find == m_options_macro_map.end())
	{
		// Wasn't found in the options-macro, trying in the custom macros
		auto find_custom = m_custom_macro_map.find(name);
		if (find_custom == m_custom_macro_map.end())
			return nullptr;
		else
			return find_custom->second;
	}
	else
		return find->second;
}

int* GPUKernelCompilerOptions::get_raw_pointer_to_macro_value(const std::string& name)
{
	std::shared_ptr<int> pointer = get_pointer_to_macro_value(name);
	if (pointer != nullptr)
		return pointer.get();

	return nullptr;
}

void GPUKernelCompilerOptions::set_pointer_to_macro(const std::string& name, std::shared_ptr<int> pointer_to_value)
{
	auto find = m_options_macro_map.find(name);

	if (find == m_options_macro_map.end())
		// Wasn't found in the options-macro, adding/setting it in the custom macro map
		m_custom_macro_map[name] = pointer_to_value;
	else
		m_options_macro_map[name] = pointer_to_value;
}

const std::unordered_map<std::string, std::shared_ptr<int>>& GPUKernelCompilerOptions::get_options_macro_map() const
{
	return m_options_macro_map;
}

const std::unordered_map<std::string, std::shared_ptr<int>>& GPUKernelCompilerOptions::get_custom_macro_map() const
{
	return m_custom_macro_map;
}

void GPUKernelCompilerOptions::clear()
{
	m_custom_macro_map.clear();
	m_options_macro_map.clear();
	m_additional_include_directories.clear();
}

void GPUKernelCompilerOptions::apply_onto(GPUKernelCompilerOptions& other)
{
	for (auto& pair : m_options_macro_map)
	{
		if (other.m_options_macro_map.find(pair.first) == other.m_options_macro_map.end())
			// The option doesn't exist, we need to create the shared ptr
			other.m_options_macro_map[pair.first] = std::make_shared<int>(*pair.second);
		else
			// No need to create a shared ptr, we can just copy the value
			*other.m_options_macro_map[pair.first] = *pair.second;
	}

	for (auto& pair : m_custom_macro_map)
	{
		if (other.m_custom_macro_map.find(pair.first) == other.m_custom_macro_map.end())
			// The option doesn't exist, we need to create the shared ptr
			other.m_custom_macro_map[pair.first] = std::make_shared<int>(*pair.second);
		else
			// No need to create a shared ptr, we can just copy the value
			*other.m_custom_macro_map[pair.first] = *pair.second;
	}
}
