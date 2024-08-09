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

// \file DEMDisparity.cc

// Estimate the low-resolution disparity based on cameras and a DEM.

// TODO(oalexan1): Decrease the tolerance for the DEM intersection.

// TODO(oalexan1): Filter this disparity for outliers

// TODO(oalexan1): Multithreading is failing! Must have 
// one copy of the transform for each thread! See stereo_tri.cc,
// the array transforms_copy. 

#include <asp/Core/StereoSettings.h>
#include <asp/Core/DemDisparity.h>

#include <vw/Image/ImageView.h>
#include <vw/Image/Transform.h>
#include <vw/Image/MaskViews.h>
#include <vw/FileIO/DiskImageView.h>
#include <vw/Camera/CameraModel.h>
#include <vw/Cartography/CameraBBox.h>
#include <vw/Cartography/GeoReferenceUtils.h>
#include <vw/Stereo/DisparityMap.h>
#include <vw/FileIO/MatrixIO.h>
#include <vw/Cartography/Map2CamTrans.h>

#include <boost/filesystem/operations.hpp>
namespace fs = boost::filesystem;

using namespace vw;
using namespace vw::cartography;

namespace asp {

// Take a low-res pixel. Make it full res. Undo the transform. Intersect with the DEM.
// Return true on success. This is used twice. It is templated so that it can be called
// for both ImageViewRef and ImageView.
template <class DEMImageT>
inline bool 
lowResPixToDemXyz(Vector2 const& left_lowres_pix,
                  vw::Vector2f const& downsample_scale,
                  vw::TransformPtr tx_left,
                  vw::CamPtr left_camera_model,
                  double dem_error, GeoReference const& dem_georef,
                  DEMImageT & dem,
                  double height_guess,
                  // Outputs
                  vw::Vector3 & left_camera_vec, Vector3 & prev_xyz, Vector3 & xyz) {
  
  Vector2 left_fullres_pix = elem_quot(left_lowres_pix, downsample_scale);
  left_fullres_pix = tx_left->reverse(left_fullres_pix);
  
  bool has_intersection = false;
  Vector3 left_camera_ctr;
  try {
    left_camera_ctr = left_camera_model->camera_center(left_fullres_pix);
    left_camera_vec = left_camera_model->pixel_to_vector(left_fullres_pix);
  } catch (...) {
    return false;
  }
  
  // TODO(oalexan1): Decrease this tol
  double height_error_tol = std::max(dem_error/4.0, 1.0); // height error in meters
  double max_abs_tol      = height_error_tol/4.0; // abs cost function change
  double max_rel_tol      = 1e-14; // rel cost function change
  int    num_max_iter     = 50;
  bool   treat_nodata_as_zero = false;
  xyz = camera_pixel_to_dem_xyz(left_camera_ctr, left_camera_vec,
                                dem, dem_georef,
                                treat_nodata_as_zero,
                                has_intersection,
                                height_error_tol, max_abs_tol,
                                max_rel_tol, num_max_iter,
                                prev_xyz, height_guess);
  if (!has_intersection || xyz == Vector3()) 
    return false;
  
  // Update the previous guess  
  prev_xyz = xyz;
  
  return true;
}

typedef ImageViewRef<PixelGray<float>> ImgRefT;

class DemDisparity: public ImageViewBase<DemDisparity> {
  ImgRefT           m_left_image;
  double            m_dem_error;
  GeoReference      m_dem_georef;
  ImageViewRef<PixelMask<float>> m_dem;
  Vector2f          m_downsample_scale;
  vw::TransformPtr  m_tx_left, m_tx_right;
  boost::shared_ptr<camera::CameraModel> m_left_camera_model;
  boost::shared_ptr<camera::CameraModel> m_right_camera_model;
  int             m_pixel_sample;
  ImageView<PixelMask<Vector2i>> & m_disp_spread;
  double m_height_guess;

public:

DemDisparity(ImgRefT const& left_image,
              double dem_error, GeoReference dem_georef,
              ImageViewRef<PixelMask<float>> const& dem,
              Vector2f const& downsample_scale,
              vw::TransformPtr tx_left, vw::TransformPtr tx_right, 
              boost::shared_ptr<camera::CameraModel> left_camera_model,
              boost::shared_ptr<camera::CameraModel> right_camera_model,
              int pixel_sample, ImageView<PixelMask<Vector2i>> & disp_spread):
    m_left_image(left_image.impl()),
    m_dem_error(dem_error),
    m_dem_georef(dem_georef),
    m_dem(dem),
    m_downsample_scale(downsample_scale),
    m_left_camera_model(left_camera_model),
    m_right_camera_model(right_camera_model),
    m_pixel_sample(pixel_sample),
    m_disp_spread(disp_spread) {

    // Make copies of Map2Camp transforms, as those are not thread-safe
    
    if (dynamic_cast<Map2CamTrans*>(tx_left.get()) != NULL) {
      std::cout << "--make copy of left tx\n";
     m_tx_left = vw::cartography::mapproj_trans_copy(tx_left);
    }
    else
      m_tx_left = tx_left;

    if (dynamic_cast<Map2CamTrans*>(tx_right.get()) != NULL) {
      std::cout << "--make copy of right tx\n";
     m_tx_right = vw::cartography::mapproj_trans_copy(tx_right);
    }
    else
      m_tx_right = tx_right;
    
  // This can speed up and make more reliable the intersection of rays with the DEM
  m_height_guess = vw::cartography::demHeightGuess(m_dem);
}
      

// ImageView interface
typedef PixelMask<Vector2f> pixel_type;
typedef pixel_type result_type;
typedef ProceduralPixelAccessor<DemDisparity> pixel_accessor;

inline int32 cols  () const { return m_left_image.cols(); }
inline int32 rows  () const { return m_left_image.rows(); }
inline int32 planes() const { return 1; }

inline pixel_accessor origin() const { return pixel_accessor(*this, 0, 0); }

inline pixel_type operator()(double /*i*/, double /*j*/, int32 /*p*/ = 0) const {
  vw_throw(NoImplErr() << "DemDisparity::operator()(...) is not implemented.\n");
  return pixel_type();
}

typedef CropView<ImageView<pixel_type>> prerasterize_type;
inline prerasterize_type prerasterize(BBox2i const& bbox) const {
  
  prerasterize_type lowres_disparity 
    = prerasterize_type(ImageView<pixel_type>(bbox.width(), bbox.height()),
                        -bbox.min().x(), -bbox.min().y(), cols(), rows());

  for (int row = bbox.min().y(); row < bbox.max().y(); row++) {
    for (int col = bbox.min().x(); col < bbox.max().x(); col++) {
      lowres_disparity(col, row) = PixelMask<Vector2f>();
      lowres_disparity(col, row).invalidate();
      m_disp_spread(col, row) = PixelMask<Vector2i>();
      m_disp_spread(col, row).invalidate();
    }
  }

  double height_error_tol = std::max(m_dem_error/4.0, 1.0); // height error in meters
  double max_abs_tol      = height_error_tol/4.0; // abs cost function change
  double max_rel_tol      = 1e-14; // rel cost function change
  int    num_max_iter     = 50;
  bool   treat_nodata_as_zero = false;
  Vector3 prev_xyz;

  // Estimate the DEM region we expect to use and crop it into an
  // ImageView. This will make the algorithm much faster than
  // accessing individual DEM pixels from disk. To do that, find
  // the pixel values on a small set of points of the diagonals of
  // the current tile.

  std::vector<Vector2> diagonals;
  int wid = bbox.width() - 1, hgt = bbox.height() - 1;
  int dim = std::max(1, std::max(wid, hgt)/10);
  for (int i = 0; i <= dim; i++)
    diagonals.push_back(bbox.min() + Vector2(double(i)*wid/dim, double(i)*hgt/dim));
  for (int i = 0; i <= dim; i++)
    diagonals.push_back(bbox.min() + Vector2(double(i)*wid/dim, hgt - double(i)*hgt/dim));

  BBox2i dem_box;
  for (unsigned k = 0; k < diagonals.size(); k++) {

    // TODO(oalexan1): This is duplicated code. Move it to a function.
    Vector2 left_lowres_pix = diagonals[k];
    
    vw::Vector3 left_camera_vec, xyz;
    bool success = lowResPixToDemXyz(left_lowres_pix, m_downsample_scale, m_tx_left, 
                                     m_left_camera_model, m_dem_error, m_dem_georef, 
                                     m_dem, m_height_guess, 
                                     left_camera_vec, prev_xyz, xyz); // outputs
    
    if (!success) 
      continue;

    Vector3 llh = m_dem_georef.datum().cartesian_to_geodetic(xyz);
    Vector2 pix = round(m_dem_georef.lonlat_to_pixel(subvector(llh, 0, 2)));
    dem_box.grow(pix);
  }

  // Expand the DEM box just in case as the above calculation is
  // not fool-proof if the DEM has a lot of no-data regions.
  int expand = std::max(100, (int)(0.1*std::max(dem_box.width(), dem_box.height())));
  dem_box.expand(expand);
  dem_box.crop(bounding_box(m_dem));

  // Crop the georef, read the DEM region in memory
  GeoReference georef_crop = crop(m_dem_georef, dem_box);
  ImageView<PixelMask<float>> dem_crop = crop(m_dem, dem_box);

  // Compute the DEM disparity. Use one in every 'm_pixel_sample' pixels.
  for (int row = bbox.min().y(); row < bbox.max().y(); row++) {

    if (row%m_pixel_sample != 0) 
      continue;

    // Must wipe the previous guess since we are now too far from it
    prev_xyz = Vector3();

    for (int col = bbox.min().x(); col < bbox.max().x(); col++) {
      if (col%m_pixel_sample != 0) 
        continue;

      Vector2 left_lowres_pix = Vector2(col, row);
      
      vw::Vector3 left_camera_vec, xyz;
      bool success = lowResPixToDemXyz(left_lowres_pix, m_downsample_scale, m_tx_left, 
                                       m_left_camera_model, m_dem_error, georef_crop, 
                                       dem_crop, m_height_guess,
                                       left_camera_vec, prev_xyz, xyz); // outputs
      if (!success) 
        continue;

      // Since our DEM is only known approximately, the true
      // intersection point of the ray coming from the left camera
      // with the DEM could be anywhere within m_dem_error from
      // xyz. Use that to get an estimate of the disparity
      // error.

      ImageView<PixelMask<Vector2>> curr_pixel_disp_range(3, 1);
      double bias[] = {-1.0, 1.0, 0.0};
      int success_arr[] = {0, 0, 0};

      for (int k = 0; k < curr_pixel_disp_range.cols(); k++) {

        curr_pixel_disp_range(k, 0).invalidate();
        Vector2 right_fullres_pix;
        try {
          vw::Vector3 biased_xyz = xyz + bias[k] * m_dem_error * left_camera_vec;
          // Raw camera pixel
          right_fullres_pix = m_right_camera_model->point_to_pixel(biased_xyz);
          // Transformed camera pixel
          right_fullres_pix = m_tx_right->forward(right_fullres_pix);
        } catch (...) {
          continue;
        }

        Vector2 right_lowres_pix = elem_prod(right_fullres_pix, m_downsample_scale);
        curr_pixel_disp_range(k, 0) = right_lowres_pix - left_lowres_pix;
        curr_pixel_disp_range(k, 0).validate();
        success_arr[k] = 1;

        // If the disparities at the endpoints of the range were successful,
        // don't bother with the middle estimate.
        if (k == 1 && success_arr[0] && success_arr[1]) break;
      }

      BBox2f search_range = stereo::get_disparity_range(curr_pixel_disp_range);
      if (search_range == BBox2f(0,0,0,0)) 
        continue;

      lowres_disparity(col, row) = round((search_range.min() + search_range.max())/2.0);
      m_disp_spread(col, row) = ceil((search_range.max() - search_range.min())/2.0);
    }
  }

  return lowres_disparity;
}

template <class DestT>
inline void rasterize(DestT const& dest, BBox2i bbox) const {
  vw::rasterize(prerasterize(bbox), dest, bbox);
}

}; // End class DemDisparity

void produce_dem_disparity(ASPGlobalOptions & opt,
                            vw::TransformPtr tx_left, vw::TransformPtr tx_right,
                            boost::shared_ptr<camera::CameraModel> left_camera_model,
                            boost::shared_ptr<camera::CameraModel> right_camera_model,
                            std::string const& session_name) {

  if (stereo_settings().is_search_defined())
    vw_out(WarningMessage) << "Computing low-resolution disparity from DEM. "
                            << "Will ignore corr-search value: "
                            << stereo_settings().search_range << ".\n";

  // Skip pixels to speed things up, particularly for ISIS and DG.
  int pixel_sample = 2;

  DiskImageView<PixelGray<float>> left_image(opt.out_prefix+"-L.tif");
  DiskImageView<PixelGray<float>> left_image_sub(opt.out_prefix+"-L_sub.tif");

  // The DEM to use to estimate the disparity
  std::string dem_file = stereo_settings().disparity_estimation_dem;
  if (dem_file == "")
    vw::vw_throw(vw::ArgumentErr() << "dem_disparity: No value was provided for "
              << "disparity-estimation-dem.\n");
  double dem_error = stereo_settings().disparity_estimation_dem_error;
  if (dem_error < 0.0)
    vw::vw_throw(vw::ArgumentErr() << "dem_disparity: Invalid value for "
              << "disparity-estimation-dem-error: " << dem_error << ".\n");

  GeoReference dem_georef;
  bool has_georef = cartography::read_georeference(dem_georef, dem_file);
  if (!has_georef)
    vw::vw_throw(vw::ArgumentErr() << "There is no georeference information in: "
                    << dem_file << ".\n");

  // Create a masked DEM, by using the no-data value, if present
  DiskImageView<float> dem_disk_image(dem_file);
  ImageViewRef<PixelMask<float>> dem = pixel_cast<PixelMask<float>>(dem_disk_image);
  boost::shared_ptr<DiskImageResource> rsrc(DiskImageResourcePtr(dem_file));
  if (rsrc->has_nodata_read()) {
    double nodata_value = rsrc->nodata_read();
    if (!std::isnan(nodata_value))
      dem = create_mask(dem_disk_image, nodata_value);
  }

  Vector2f downsample_scale(float(left_image_sub.cols()) / float(left_image.cols()),
                            float(left_image_sub.rows()) / float(left_image.rows()));

  // Smaller tiles is better, as then more threads can run at once
  Vector2 orig_tile_size = opt.raster_tile_size;
  opt.raster_tile_size = Vector2i(64, 64);

  // This image is small enough that we can keep it in memory. It will be created
  // alongside the low-res disparity image.
  ImageView<PixelMask<Vector2i>> disp_spread(left_image_sub.cols(), left_image_sub.rows());

  // Compute and write the low-resolution disparity
  ImageViewRef<PixelMask<Vector2f>> lowres_disparity
    = DemDisparity(left_image_sub, dem_error, dem_georef, dem, downsample_scale,
                   tx_left, tx_right, left_camera_model, right_camera_model,
                   pixel_sample, disp_spread);
  std::string disparity_file = opt.out_prefix + "-D_sub.tif";
  vw_out() << "Writing low-resolution disparity: " << disparity_file << "\n";
  auto tpc1 = TerminalProgressCallback("asp", "\t--> Low-resolution disparity:");
  if (session_name.find("isis") != std::string::npos) {
    // ISIS does not support multi-threading
    boost::scoped_ptr<DiskImageResource> 
      drsrc(vw::cartography::build_gdal_rsrc(disparity_file, lowres_disparity, opt));
    vw::write_image(*drsrc, lowres_disparity, tpc1);
  } else {
    vw::cartography::block_write_gdal_image(disparity_file, lowres_disparity, opt, tpc1);
  }

  // The disparity spread is in memory by now, so can be written with multiple threads 
  std::string disp_spread_file = opt.out_prefix + "-D_sub_spread.tif";
  vw_out() << "Writing low-resolution disparity spread: " << disp_spread_file << "\n";
  auto tpc2 = TerminalProgressCallback("asp", "\t--> Low-resolution disparity spread:");
  vw::cartography::block_write_gdal_image(disp_spread_file, disp_spread, opt, tpc2);
  
  // Go back to the original tile size
  opt.raster_tile_size = orig_tile_size;
  
  return;
} // end produce_dem_disparity()
  
} // end namespace asp
