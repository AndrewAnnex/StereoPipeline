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



#include <vw/Math/EulerAngles.h>
#include <asp/Camera/RPCModel.h>
#include <asp/Camera/DG_XML.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace asp {

template <class PositionFuncT, class PoseFuncT>
vw::camera::PinholeModel LinescanDGModel<PositionFuncT, PoseFuncT>::linescan_to_pinhole(double y) const {

  double t = this->m_time_func( y );
  return vw::camera::PinholeModel(this->m_position_func(t),  this->m_pose_func(t).rotation_matrix(),
				  this->m_focal_length, -this->m_focal_length,
				  -this->m_detector_origin[0], y - this->m_detector_origin[1]
				  );
}


class SecondsFrom
{
  boost::posix_time::ptime m_reference;
public:
  inline SecondsFrom( boost::posix_time::ptime const& time ) : m_reference(time) {}

  inline double operator()( boost::posix_time::ptime const& time ) const {
    return double( (time - m_reference).total_microseconds() ) / 1e6;
  }
};

inline boost::posix_time::ptime parse_time(std::string str)
{
  try{
    return boost::posix_time::time_from_string(str);
  }catch(...){
    vw::vw_throw(vw::ArgumentErr() << "Failed to parse time from string: " << str
		 << ". If you are not using Digital Globe images, you may need to specify the session as -t rpc or -t rpcmaprpc.\n");
  }
  return boost::posix_time::time_from_string(str); // Never reached!
}

boost::shared_ptr<DGCameraModel> load_dg_camera_model_from_xml(std::string const& path)
{
  //vw_out() << "DEBUG - Loading DG camera file: " << camera_file << std::endl;

  // Parse the Digital Globe XML file
  GeometricXML geo;
  AttitudeXML  att;
  EphemerisXML eph;
  ImageXML     img;
  RPCXML       rpc;
  read_xml( path, geo, att, eph, img, rpc );

  // Convert measurements in millimeters to pixels.
  geo.principal_distance /= geo.detector_pixel_pitch;
  geo.detector_origin    /= geo.detector_pixel_pitch;

  // Convert all time measurements to something that boost::date_time can read.
  boost::replace_all( eph.start_time,            "T", " " );
  boost::replace_all( img.tlc_start_time,        "T", " " );
  boost::replace_all( img.first_line_start_time, "T", " " );
  boost::replace_all( att.start_time,            "T", " " );

  // Convert UTC time measurements to line measurements. Ephemeris
  // start time will be our reference frame to calculate seconds against.
  SecondsFrom convert( parse_time( eph.start_time ) );

  // I'm going make the assumption that EPH and ATT are sampled at the same rate and time.
  VW_ASSERT( eph.position_vec.size() == att.quat_vec.size(),
	     vw::MathErr() << "Ephemeris and Attitude don't have the same number of samples." );
  VW_ASSERT( eph.start_time == att.start_time && eph.time_interval == att.time_interval,
	     vw::MathErr() << "Ephemeris and Attitude don't seem to sample with the same t0 or dt." );

  // Convert ephemeris to be position of camera. Change attitude to
  // be the rotation from camera frame to world frame. We also add an
  // additional rotation to the camera frame so X is the horizontal
  // direction to the picture and +Y points down the image (in the direction of flight).
  vw::Quat sensor_coordinate = vw::math::euler_xyz_to_quaternion(vw::Vector3(0,0,geo.detector_rotation - M_PI/2));
  for ( size_t i = 0; i < eph.position_vec.size(); i++ ) {
    eph.position_vec[i] += att.quat_vec[i].rotate( geo.perspective_center );
    att.quat_vec[i] = att.quat_vec[i] * geo.camera_attitude * sensor_coordinate;
  }

  //vw::vw_out() << "DG model load - sensor_coordinate = " << sensor_coordinate << std::endl;
  //geo.printDebugInfo(); // DEBUG INFO

  // Load up the time interpolation class. If the TLCList only has
  // one entry ... then we have to manually drop in the slope and offset.
  if ( img.tlc_vec.size() == 1 ) {
    double direction = 1;
    if ( boost::to_lower_copy( img.scan_direction ) != "forward" ) {
      direction = -1;
    }
    img.tlc_vec.push_back( std::make_pair(img.tlc_vec.front().first +
					  img.avg_line_rate, direction) );
  }

  // Build the TLCTimeInterpolation object and do a quick sanity check.
  vw::camera::TLCTimeInterpolation tlc_time_interpolation( img.tlc_vec,
							       convert( parse_time( img.tlc_start_time ) ) );
  VW_ASSERT( fabs( convert( parse_time( img.first_line_start_time ) ) -
		   tlc_time_interpolation( 0 ) ) < fabs( 1.0 / (10.0 * img.avg_line_rate ) ),
	     vw::MathErr() << "First Line Time and output from TLC lookup table do not agree of the ephemeris time for the first line of the image." );

  vw::Vector2 final_detector_origin
    = subvector(inverse(sensor_coordinate).rotate(vw::Vector3(geo.detector_origin[0],
							      geo.detector_origin[1],
							      0)), 0, 2);

  double et0 = convert( parse_time( eph.start_time ) );
  double at0 = convert( parse_time( att.start_time ) );
  double edt = eph.time_interval;
  double adt = att.time_interval;

  typedef boost::shared_ptr<DGCameraModel> CameraModelPtr;
  return CameraModelPtr(new DGCameraModel(vw::camera::PiecewiseAPositionInterpolation(eph.position_vec, eph.velocity_vec, et0, edt ),
					                                vw::camera::LinearPiecewisePositionInterpolation(eph.velocity_vec, et0, edt),
					                                vw::camera::SLERPPoseInterpolation(att.quat_vec, at0, adt),
					                                tlc_time_interpolation, img.image_size,
					                                final_detector_origin,
					                                geo.principal_distance)
		    );
} // End function load_dg_camera_model()


} // end namespace asp

