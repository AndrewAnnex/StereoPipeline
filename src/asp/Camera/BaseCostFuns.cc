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

// Cost functions used in bundle adjustment. These need access to the camera
// models, so they are stored in the Camera folder.

#include <asp/Camera/BaseCostFuns.h>

namespace asp {
  
CamUncertaintyError::CamUncertaintyError(vw::Vector3 const& orig_ctr, double const* orig_adj,
                                         vw::Vector2 const& uncertainty, double weight,
                                         vw::cartography::Datum const& datum,
                                         double camera_position_uncertainty_power):
  m_orig_ctr(orig_ctr), m_uncertainty(uncertainty), m_weight(weight),
  m_camera_position_uncertainty_power(camera_position_uncertainty_power) {
    
  // m_weight must be positive
  if (m_weight <= 0)
    vw::vw_throw(vw::ArgumentErr() << "CamUncertaintyError: Invalid weight: "
              << m_weight << ". It must be positive.\n");
  
  // The first three parameters are the camera center adjustments.
  m_orig_adj = vw::Vector3(orig_adj[0], orig_adj[1], orig_adj[2]);

  // The uncertainty must be positive
  if (m_uncertainty[0] <= 0 || m_uncertainty[1] <= 0)
    vw::vw_throw(vw::ArgumentErr() << "CamUncertaintyError: Invalid uncertainty: "
              << uncertainty << ". All values must be positive.\n");    
  
  // The NED coordinate system, for separating horizontal and vertical components
  vw::Vector3 llh = datum.cartesian_to_geodetic(orig_ctr);
  vw::Matrix3x3 NedToEcef = datum.lonlat_to_ned_matrix(llh);
  m_EcefToNed = vw::math::inverse(NedToEcef);
}

// The signed power is a better-behaved version of pow that respects the sign of the input.
double signed_power(double val, double power) {
  if (val < 0)
    return -pow(-val, power);
  return pow(val, power);
}

bool CamUncertaintyError::operator()(const double* cam_adj, double* residuals) const {
  
  // The difference between the original and current camera center
  vw::Vector3 diff;
  for (size_t p = 0; p < 3; p++)
    diff[p] = cam_adj[p] - m_orig_adj[p];
  
  // Convert the difference to NED
  vw::Vector3 NedDir = m_EcefToNed * diff;
  
  // Split into horizontal and vertical components
  vw::Vector2 horiz = subvector(NedDir, 0, 2);
  double      vert  = NedDir[2];
  
  // Normalize by uncertainty
  horiz /= m_uncertainty[0];
  vert  /= m_uncertainty[1];
  
  // In the final sum of squares, each term will end up being differences raised
  // to m_camera_position_uncertainty_power power. Multiply by
  // sqrt(m_weight), to give the squared residual the correct weight.
  double p = m_camera_position_uncertainty_power / 2.0;
  residuals[0] = sqrt(m_weight) * signed_power(horiz[0], p);
  residuals[1] = sqrt(m_weight) * signed_power(horiz[1], p);
  residuals[2] = sqrt(m_weight) * signed_power(vert, p);

  return true;
}

} // end namespace asp
