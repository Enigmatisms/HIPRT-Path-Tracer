/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_RESTIR_DI_SAMPLE_FLAGS_H
#define DEVICE_RESTIR_DI_SAMPLE_FLAGS_H

enum ReSTIRDISampleFlags
{
    RESTIR_DI_FLAGS_NONE = 0,
    // The sample is an evmap sample and 'point_on_light_source'
    // should be interpreted as a direction, not a point on a light source
    RESTIR_DI_FLAGS_ENVMAP_SAMPLE = 1 << 0,
    // The sample is a BSDF sample and it's a refraction BSDF sample
    // This means that we should not check for the cosine term to be > 0.0f
    // because it IS going to be < 0.0f
    RESTIR_DI_FLAGS_BSDF_REFRACTION = 1 << 1,
    // This sample *AT ITS OWN PIXEL* is unoccluded. This can be used to avoid tracing
    // rays for visibility since we know it's unoccluded already
    RESTIR_DI_FLAGS_UNOCCLUDED = 1 << 2
};

#endif
