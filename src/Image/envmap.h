/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef ENVMAP_H
#define ENVMAP_H

#include "Image/Image.h"

class EnvironmentMap : public Image
{
public:
    EnvironmentMap() {}
    EnvironmentMap(int width, int height); 
    EnvironmentMap(Image&& data, int width, int height);

    void compute_cdf();
    const std::vector<float>& cdf() const;

    static EnvironmentMap read_from_file(const std::string& filepath);

private:
    std::vector<float> m_cdf;
};

#endif
