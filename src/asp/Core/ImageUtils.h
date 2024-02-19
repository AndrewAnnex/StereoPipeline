// __BEGIN_LICENSE__
//  Copyright (c) 2009-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NGT platform is licensed under the Apache License, Version 2.0 (the
//  "License"); you may not use this file except in compliance with the
//  License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__

/// \file ImageUtils.h
/// Utility functions for handling images

#ifndef __ASP_CORE_IMAGE_UTILS_H__
#define __ASP_CORE_IMAGE_UTILS_H__

#include <vw/Image/ImageViewRef.h>

namespace vw {
  namespace cartography {
    class GeoReference;
  }
}

namespace asp {

/// Load an input image, georef, and nodata value
void load_image(std::string const& image_file,
                vw::ImageViewRef<double> & image, double & nodata,
                bool & has_georef, vw::cartography::GeoReference & georef);

  /// Create a DEM ready to use for interpolation
  void create_interp_dem(std::string const& dem_file,
                         vw::cartography::GeoReference & dem_georef,
                         vw::ImageViewRef<vw::PixelMask<double>> & interp_dem);

} // end namespace asp

#endif//__ASP_CORE_IMAGE_UTILS_H__