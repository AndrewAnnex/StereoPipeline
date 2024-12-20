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

// \file SfsUtils.h
// Basic utilities for SfS

#ifndef __ASP_SFS_SFS_UTILS_H__
#define __ASP_SFS_SFS_UTILS_H__

#include <vw/Cartography/GeoReference.h>
#include <vw/Image/ImageView.h>

namespace asp {

// Find the Sun azimuth and elevation at the lon-lat position of the
// center of the DEM. The result can change depending on the DEM.
void sunAngles(vw::ImageView<double> const& dem, 
               double nodata_val, 
               vw::cartography::GeoReference const& georef,
               vw::Vector3 const& sun_pos,
               double & azimuth, double & elevation);

// Read sun positions from a file
void readSunPositions(std::string const& sun_positions_list,
                      std::vector<std::string> const& input_images,
                      vw::ImageView<double> const& dem, 
                      double nodata_val, 
                      vw::cartography::GeoReference const& georef,
                      std::vector<vw::Vector3> & sun_positions);

// Read the sun angles (azimuth and elevation) and convert them to sun positions.
void readSunAngles(std::string const& sun_positions_list,
                   std::vector<std::string> const& input_images,
                   vw::ImageView<double> const& dem, 
                   double nodata_val, 
                   vw::cartography::GeoReference const& georef,
                   std::vector<vw::Vector3> & sun_positions);

} // end namespace asp

#endif // __ASP_SFS_SFS_UTILS_H__
