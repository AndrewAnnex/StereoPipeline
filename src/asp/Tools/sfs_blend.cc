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

/// \file sfs_blend.cc
///

// A tool to take an SfS-produced DEM, and replace in the areas in
// permanent shadow this DEM with the original LOLA DEM, with a
// transition at the boundary.

// It uses the Euclidean distance to the boundary, which is better
// than the Manhattan distance employed by grassfire.

// TODO(oalexan1): Check in and document this tool!

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <limits>
#include <algorithm>

#include <vw/FileIO/DiskImageManager.h>
#include <vw/Image/InpaintView.h>
#include <vw/Image/Algorithms2.h>
#include <vw/Image/Filter.h>
#include <vw/Cartography/GeoTransform.h>
#include <asp/Core/Macros.h>
#include <asp/Core/Common.h>

#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/program_options.hpp>

#include <boost/filesystem/convenience.hpp>

using namespace std;
using namespace vw;
using namespace vw::cartography;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

GeoReference read_georef(std::string const& file){
  // Read a georef, and check for success
  GeoReference geo;
  bool is_good = read_georeference(geo, file);
  if (!is_good)
    vw_throw(ArgumentErr() << "No georeference found in " << file << ".\n");
  return geo;
}

struct Options: vw::cartography::GdalWriteOptions {
  string sfs_dem, lola_dem, max_lit_image_mosaic, output_dem, sfs_mask;
  double image_threshold, weight_blur_sigma, blend_length, min_blend_size;
  Options(): image_threshold(0.0), weight_blur_sigma(0.0), blend_length(0.0), min_blend_size(0.0) {}
};

// The workhorse of this code, do the blending
class SfsBlendView: public ImageViewBase<SfsBlendView>{
  
  ImageViewRef<float> m_sfs_dem, m_lola_dem, m_image_mosaic;
  float m_sfs_nodata, m_lola_nodata, m_mask_nodata;
  int m_extra;
  bool m_save_mask;
  Options const& m_opt;
  
  typedef float PixelT;
  
public:
  SfsBlendView(ImageViewRef<float> sfs_dem, ImageViewRef<float> lola_dem,
               ImageViewRef<float> image_mosaic,
               float sfs_nodata, float lola_nodata, float mask_nodata, int extra,
               bool save_mask, Options const& opt):
    m_sfs_dem(sfs_dem), m_lola_dem(lola_dem), m_image_mosaic(image_mosaic),
    m_sfs_nodata(sfs_nodata), m_lola_nodata(lola_nodata),
    m_mask_nodata(mask_nodata), m_extra(extra),
    m_save_mask(save_mask), m_opt(opt) {}

  typedef PixelT pixel_type;
  typedef PixelT result_type;
  typedef ProceduralPixelAccessor<SfsBlendView> pixel_accessor;
  
  inline int32 cols() const { return m_sfs_dem.cols(); }
  inline int32 rows() const { return m_sfs_dem.rows(); }
  inline int32 planes() const { return 1; }
  
  inline pixel_accessor origin() const { return pixel_accessor( *this, 0, 0 ); }
  
  inline pixel_type operator()( double/*i*/, double/*j*/, int32/*p*/ = 0 ) const {
    vw_throw(NoImplErr() << "SfsBlendView::operator()(...) is not implemented");
    return pixel_type();
  }
  
  typedef CropView<ImageView<pixel_type> > prerasterize_type;
  inline prerasterize_type prerasterize(BBox2i const& bbox) const {

    BBox2i biased_box = bbox;
    biased_box.expand(m_extra);
    biased_box.crop(bounding_box(m_sfs_dem));

    // Make crops in memory (from references)
    ImageView<pixel_type> sfs_dem_crop = crop(m_sfs_dem, biased_box);
    ImageView<pixel_type> lola_dem_crop = crop(m_lola_dem, biased_box);
    ImageView<pixel_type> image_mosaic_crop = crop(m_image_mosaic, biased_box);

    // Find the grassfire weight (Manhattan distance to the boundary
    // of the nodata-region). It will tell us on what areas to focus.
    bool no_zero_at_border = true; // don't decrease the weights to zero at image border
    ImageView<pixel_type> grass_dist
      = vw::grassfire
      (vw::copy(vw::fill_holes_grass
                (vw::copy(create_mask_less_or_equal(image_mosaic_crop, m_opt.image_threshold)),
                 m_opt.min_blend_size)),
       no_zero_at_border);

    // The clamped distance to the boundary
    ImageView<float> dist_to_bd;
    dist_to_bd.set_size(sfs_dem_crop.cols(), sfs_dem_crop.rows());
    for (int col = 0; col < sfs_dem_crop.cols(); col++) {
      
      for (int row = 0; row < sfs_dem_crop.rows(); row++) {

        if (grass_dist(col, row) >= 1.5*m_opt.blend_length) { // Too far from the boundary
          dist_to_bd(col, row) = m_opt.blend_length; // clamp at the blending length
          continue;
        }
        
        if (grass_dist(col, row) == 0) {
          // On the boundary or inside the nodata region
          dist_to_bd(col, row) = 0;
          continue;
        }
        
        // Find the shortest Euclidean distance to the no-data region.
        double dist = m_opt.blend_length;
        for (int col2 = std::max(0.0, col - m_opt.blend_length);
             col2 <= std::min(sfs_dem_crop.cols() - 1.0, col + m_opt.blend_length);
             col2++) {

          // Estimate the range of rows for the circle with given radius
          // at given col value.
          double ht_val = ceil(sqrt(double(m_opt.blend_length * m_opt.blend_length) -
                                    double((col - col2) * (col - col2))));
          
          for (int row2 = std::max(0.0, row - ht_val);
               row2 <= std::min(sfs_dem_crop.rows() - 1.0, row + ht_val);
               row2++) {

            if (grass_dist(col2, row2) > 0) 
              continue; // not at boundary and not inside the no-data region

            // See if the current point is closer than anything so far
            double curr_dist = sqrt( double(col - col2) * (col - col2) +
                                     double(row - row2) * (row - row2) );
            if (curr_dist < dist) 
              dist = curr_dist;
            
          }
        }

        // The closest we've got
        dist_to_bd(col, row) = dist;
      }
    }

    // Apply the blur
    if (m_opt.weight_blur_sigma > 0)
      dist_to_bd = vw::gaussian_filter(dist_to_bd, m_opt.weight_blur_sigma);

    // Do the blending
    ImageView<float> blended_dem;
    blended_dem.set_size(sfs_dem_crop.cols(), sfs_dem_crop.rows());
    for (int col = 0; col < sfs_dem_crop.cols(); col++) {
      for (int row = 0; row < sfs_dem_crop.rows(); row++) {

        if (!m_save_mask)
          blended_dem(col, row) = m_sfs_nodata;
        else
          blended_dem(col, row) = m_mask_nodata;
          
        double weight = dist_to_bd(col, row)/m_opt.blend_length;

        // These are not strictly necessary but enforce them
        if (weight > 1.0)
          weight = 1.0;
        if (weight < 0.0)
          weight = 0.0;
          
        // Handle no-data values. These are not meant to happen, but do this just in case.
        if (sfs_dem_crop(col, row) == m_sfs_nodata)
          continue;
        if (lola_dem_crop(col, row) == m_lola_nodata) 
          continue;

        if (!m_save_mask) 
          blended_dem(col, row)
            = weight * sfs_dem_crop(col, row) + (1.0 - weight) * lola_dem_crop(col, row);
        else
          blended_dem(col, row) = float( weight != 0.0 );
      }
    }
    
    return prerasterize_type(blended_dem, -biased_box.min().x(), -biased_box.min().y(),
                             cols(), rows());
  }

  template <class DestT>
  inline void rasterize(DestT const& dest, BBox2i bbox) const {
    vw::rasterize(prerasterize(bbox), dest, bbox);
  }
};

void handle_arguments(int argc, char *argv[], Options& opt) {

  po::options_description general_options("Options");
  general_options.add_options()
    ("sfs-dem", po::value<string>(&opt.sfs_dem),
     "The SfS DEM to process.")
    ("lola-dem", po::value<string>(&opt.lola_dem),
     "The LOLA DEM to fill in the regions in permanent shadow.")
    ("max-lit-image-mosaic", po::value<string>(&opt.max_lit_image_mosaic),
     "The maximally lit image mosaic to use to determine the permanently shadowed regions.")
    ("image-threshold",  po::value<double>(&opt.image_threshold)->default_value(0.0),
     "The value separating permanently shadowed pixels from lit pixels in the maximally lit image mosaic.")
    ("blend-length", po::value<double>(&opt.blend_length)->default_value(0.0),
     "The length, in pixels, over which to blend the SfS and LOLA DEMs at the boundary of the permanently shadowed region.")
    ("weight-blur-sigma", po::value<double>(&opt.weight_blur_sigma)->default_value(0.0),
     "The standard deviation of the Gaussian used to blur the weight that performs the transition from the SfS to the LOLA DEM. A higher value results in a smoother transition (this does not smooth the DEMs). The extent of the blur is about 7 times this deviation. Set to 0 to not use this operation.")
    ("min-blend-size", po::value<double>(&opt.min_blend_size)->default_value(0.0),
     "Do not apply blending in shadowed areas of dimensions less than this.")
    ("output-dem", po::value(&opt.output_dem), "The blended output DEM to save.")
    ("sfs-mask", po::value(&opt.sfs_mask), "The output mask having 1 for pixels obtained with SfS (and some LOLA blending at interfaces) and 0 for pixels purely from LOLA.")
    //("blending-weight", po::value(&opt.blending_weight),
    // "Save here the weight being used to blend the SfS and LOLA DEMs (optional).")
    ;
  
  po::options_description positional("");
  po::positional_options_description positional_desc;

  std::string usage("[options]");
  bool allow_unregistered = false;
  std::vector<std::string> unregistered;
  po::variables_map vm =
    asp::check_command_line(argc, argv, opt, general_options, general_options,
                            positional, positional_desc, usage,
                            allow_unregistered, unregistered);
  
  // Error checking
  if (opt.sfs_dem == "" || opt.lola_dem == "" ||
      opt.max_lit_image_mosaic == "" || opt.output_dem == "" || opt.sfs_mask == "")
    vw_throw(ArgumentErr() << "Not all input or output files were specified.\n"
                           << usage << general_options );
  if (opt.blend_length <= 0)
    vw_throw(ArgumentErr() << "The blending length must be positive.\n"
                           << usage << general_options );
  if (opt.image_threshold <= 0)
    vw_throw(ArgumentErr() << "The image threshold must be positive.\n"
                           << usage << general_options );

  // Create the output directory
  vw::create_out_dir(opt.output_dem);
} // End function handle_arguments

int main(int argc, char *argv[]) {

  Options opt;
  
  try{

    handle_arguments(argc, argv, opt);

    vw_out() << "Reading SfS DEM: " << opt.sfs_dem << std::endl;
    DiskImageView<float> sfs_dem(opt.sfs_dem);

    
    vw_out() << "Reading LOLA DEM: " << opt.lola_dem << std::endl;
    DiskImageView<float> lola_dem(opt.lola_dem);
    
    vw_out() << "Reading maximally-lit image mosaic: " << opt.max_lit_image_mosaic << std::endl;
    DiskImageView<float> image_mosaic(opt.max_lit_image_mosaic);

    if (sfs_dem.cols() != lola_dem.cols() || sfs_dem.rows() != lola_dem.rows())
      vw_throw(ArgumentErr() << "The SfS DEM and LOLA DEM must have the same dimensions.");
    
    if (sfs_dem.cols() != image_mosaic.cols() || sfs_dem.rows() != image_mosaic.rows())
      vw_throw(ArgumentErr() << "The SfS DEM and image mosaic must have the same dimensions.");
    
    GeoReference sfs_georef   = read_georef(opt.sfs_dem);
    GeoReference lola_georef  = read_georef(opt.lola_dem);
    GeoReference image_georef = read_georef(opt.max_lit_image_mosaic);
    if (sfs_georef.proj4_str() != lola_georef.proj4_str() ||
        sfs_georef.proj4_str() != image_georef.proj4_str())
      vw_throw(ArgumentErr() << "The SfS DEM, LOLA DEM, and image mosaic "
               << "must have the same PROJ4 string.");

    float sfs_nodata = -1.0, lola_nodata = -1.0, image_nodata = -1.0;
    DiskImageResourceGDAL sfs_rsrc(opt.sfs_dem);
    if (sfs_rsrc.has_nodata_read())
      sfs_nodata = sfs_rsrc.nodata_read();
    else
      vw_throw(ArgumentErr() << "The SfS DEM does not have a no-data value.");
    DiskImageResourceGDAL lola_rsrc(opt.lola_dem);
    if (lola_rsrc.has_nodata_read())
      lola_nodata = lola_rsrc.nodata_read();
    else
      vw_throw(ArgumentErr() << "The LOLA DEM does not have a no-data value.");
    DiskImageResourceGDAL image_rsrc(opt.max_lit_image_mosaic);
    if (image_rsrc.has_nodata_read())
      image_nodata = image_rsrc.nodata_read();
    else
      vw_throw(ArgumentErr() << "The maximally-lit mosaic does not have a no-data value.");
    
    // When processing the DEM tile by tile, need to see further in
    // each tile because of blending and blurring
    int extra = 2*opt.blend_length + opt.min_blend_size;
    if (opt.weight_blur_sigma > 0)
      extra += vw::compute_kernel_size(opt.weight_blur_sigma);

    // Write bigger tiles to make the processing with the extra margin
    // more efficient.
    int block_size = 256 + 2 * extra;
    block_size = 16*ceil(block_size/16.0); // internal constraint

    vw_out() << "Writing: " << opt.output_dem << std::endl;
    bool has_georef = true, has_nodata = true;
    TerminalProgressCallback tpc("asp", ": ");
    float mask_nodata = -1.0;
    bool save_mask = false;
    asp::save_with_temp_big_blocks(block_size,
                                   opt.output_dem,
                                   SfsBlendView(sfs_dem, lola_dem, image_mosaic,
                                                sfs_nodata, lola_nodata, mask_nodata,
                                                extra, save_mask, opt),
                                   has_georef, sfs_georef,
                                   has_nodata, sfs_nodata, opt, tpc);

    // Write the mask. Have to rerun the same logic due to ASP's limitations,
    // it cannot write two large files at the same time.
    vw_out() << "Writing the mask showing the (blended) SfS pixels: " << opt.sfs_mask << std::endl;
    save_mask = true;
    asp::save_with_temp_big_blocks(block_size,
                                   opt.sfs_mask,
                                   SfsBlendView(sfs_dem, lola_dem, image_mosaic,
                                                sfs_nodata, lola_nodata, mask_nodata,
                                                extra, save_mask, opt),
                                   has_georef, sfs_georef,
                                   has_nodata, mask_nodata, opt, tpc);
    
    
  } ASP_STANDARD_CATCHES;

  return 0;
}
