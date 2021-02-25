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


/// \file bathy_correct.cc
///

#include <asp/Core/PointUtils.h>
#include <asp/Core/Macros.h>
#include <asp/Core/Common.h>
#include <asp/Core/StereoSettings.h>

#include <vw/Core/Stopwatch.h>
#include <vw/FileIO/DiskImageUtils.h>
#include <vw/Geometry/shapeFile.h>

#include <vw/Math/RANSAC.h>

#include <Eigen/Dense>

using namespace vw;
using namespace vw::cartography;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

// Compute the 3D locations at the shape corners based on interpolating
// into the DEM and converting to ECEF.
void find_xyz_at_shape_corners(std::vector<vw::geometry::dPoly> const& polyVec,
                               vw::cartography::GeoReference const& shape_georef,
                               vw::cartography::GeoReference const& dem_georef,
                               ImageViewRef< PixelMask<float> > interp_dem,
                               std::vector<Eigen::Vector3d> & xyz_vec) {

  xyz_vec.clear();
  
  for (size_t p = 0; p < polyVec.size(); p++){
    vw::geometry::dPoly const& poly = polyVec[p];
      
    const double * xv       = poly.get_xv();
    const double * yv       = poly.get_yv();
    const int    * numVerts = poly.get_numVerts();
    int numPolys            = poly.get_numPolys();

    int start = 0;
    for (int pIter = 0; pIter < numPolys; pIter++){
        
      if (pIter > 0) start += numVerts[pIter - 1];

      int numV = numVerts[pIter];
      for (int vIter = 0; vIter < numV; vIter++) {

        Vector2 proj_pt(xv[start + vIter], yv[start + vIter]);

        // Convert from projected coordinates to lonlat
        Vector2 lonlat = shape_georef.point_to_lonlat(proj_pt);

        // Convert to DEM pixel
        Vector2 pix = dem_georef.lonlat_to_pixel(lonlat);

        PixelMask<float> h = interp_dem(pix.x(), pix.y());

        if (!is_valid(h)) 
          continue;

        Vector3 llh;
        llh[0] = lonlat[0];
        llh[1] = lonlat[1];
        llh[2] = h.child();

        Vector3 xyz = dem_georef.datum().geodetic_to_cartesian(llh);

        Eigen::Vector3d eigen_xyz;
        for (size_t coord = 0; coord < 3; coord++) 
          eigen_xyz[coord] = xyz[coord];

        xyz_vec.push_back(eigen_xyz);
      }
    }
      
  }
}

// Best fit plane without outlier removal
std::pair<Eigen::Vector3d, Eigen::Vector3d>
best_plane_from_points(const std::vector<Eigen::Vector3d> & c) {
  
  // Copy coordinates to  matrix in Eigen format
  size_t num_points = c.size();
  Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic > coord(3, num_points);
  for (size_t i = 0; i < num_points; i++)
    coord.col(i) = c[i];
  
  // calculate centroid
  Eigen::Vector3d centroid(coord.row(0).mean(), coord.row(1).mean(), coord.row(2).mean());
  
  // subtract centroid
  for (size_t i = 0; i < 3; i++) 
    coord.row(i).array() -= centroid(i);
  
  // We only need the left-singular matrix here
  // http://math.stackexchange.com/questions/99299/best-fitting-plane-given-a-set-of-points
  auto svd = coord.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV);
  Eigen::Vector3d plane_normal = svd.matrixU().rightCols<1>();
  return std::make_pair(centroid, plane_normal);
}

// A functor which returns the best fit plane a*x + b*y + c*z + d = 0
// as the vector (a, b, c, d) with a*a + b*b + c*c = 1 to be used
// with RANSAC to remove outliers.

struct BestFitPlaneFunctor {
  typedef vw::Matrix<double, 1, 4> result_type;

    /// A best fit plane requires pairs of data points to make a fit.
    template <class ContainerT>
    size_t min_elements_needed_for_fit(ContainerT const& /*example*/) const { return 3; }

    /// This function can match points in any container that supports
    /// the size() and operator[] methods.  The container is usually a
    /// vw::Vector<>, but you could substitute other classes here as
    /// well.
    template <class ContainerT>
    vw::Matrix<double> operator() (std::vector<ContainerT> const& p1,
                                   std::vector<ContainerT> const& p2,
                                   vw::Matrix<double> const& /*seed_input*/
                                   = vw::Matrix<double>() ) const {

      // check consistency
      VW_ASSERT( p1.size() == p2.size(),
                 vw::ArgumentErr() << "Cannot compute similarity transformation. "
                 << "p1 and p2 are not the same size." );
      VW_ASSERT( !p1.empty() && p1.size() >= min_elements_needed_for_fit(p1[0]),
                 vw::ArgumentErr() << "Cannot compute similarity transformation. "
                 << "Insufficient data.\n");

      std::pair<Eigen::Vector3d, Eigen::Vector3d> plane = best_plane_from_points(p1);

      Eigen::Vector3d & centroid = plane.first;
      Eigen::Vector3d & normal = plane.second;
      
      Matrix<double> result(1, 4);
      for (int col = 0; col < 3; col++)
        result(0, col) = normal[col];
        
      result(0, 3) = -normal.dot(centroid);

      return result;
    }
};

struct BestFitPlaneErrorMetric {
  template <class RelationT, class ContainerT>
  double operator() (RelationT  const& H, ContainerT const& p1, ContainerT const& p2) const {

    // Compute abs(a*x + b*y + c*z + d), where p1 = (x, y, z) and H = (a, b, c, d).
    double ans = 0.0;
    for (unsigned col = 0; col < 3; col++) {
      ans += H(0, col) * p1[col];
    }
    ans += H(0, 3);

    return std::abs(ans);
  }
};


struct Options : vw::cartography::GdalWriteOptions {
  std::string shapefile, dem;
  double water_surface_outlier_threshold;
  int num_ransac_iterations;
  Options():water_surface_outlier_threshold(2.0), num_ransac_iterations(1000) {}
};

void handle_arguments(int argc, char *argv[], Options& opt) {

  po::options_description general_options("General Options");
  general_options.add_options()
    ("shapefile",   po::value(&opt.shapefile),
     "Specify the shapefile enclosing the region in which to do bathymetry.")
    ("dem",   po::value(&opt.dem),
     "Specify the dem to correct.")
    ("water-surface-outlier-threshold",
     po::value(&opt.water_surface_outlier_threshold)->default_value(2.0),
     "A value, in meters, to determine the distance from a water edge sample point to the "
     "best-fit water surface plane to determine if it will be marked as outlier and not "
     "included in the calculation of that plane.")
    ("num-ransac-iterations", 
     po::value(&opt.num_ransac_iterations)->default_value(1000),
     "Number of RANSAC iterations to use to find the plane fitting best the water surface.");
  
  general_options.add( vw::cartography::GdalWriteOptionsDescription(opt) );

  po::options_description positional("");
  //positional.add_options()
  //   ("input-files", po::value< std::vector<std::string> >(), "Input files");

  po::positional_options_description positional_desc;
  //positional_desc.add("input-files", -1);

  std::string usage("[options]");
  bool allow_unregistered = false;
  std::vector<std::string> unregistered;
  po::variables_map vm =
    asp::check_command_line(argc, argv, opt, general_options, general_options,
                            positional, positional_desc, usage,
                            allow_unregistered, unregistered);

  if (opt.shapefile == "")
    vw_throw( ArgumentErr() << "Missing the input shapefile.\n" << usage << general_options );
  if (opt.dem == "")
    vw_throw( ArgumentErr() << "Missing the input dem.\n" << usage << general_options );
}

int main( int argc, char *argv[] ) {

  Options opt;
  try {
    handle_arguments(argc, argv, opt);

    // Read the shapefile
    std::cout << "Reading the shapefile: " << opt.shapefile << std::endl;
    bool has_shape_georef;
    std::vector<vw::geometry::dPoly> polyVec;
    std::string poly_color;
    vw::cartography::GeoReference shape_georef;
    read_shapefile(opt.shapefile, poly_color, has_shape_georef, shape_georef, polyVec);
    if (!has_shape_georef) 
      vw_throw( ArgumentErr() << "The input shapefile has no georeference.\n" );

    // Read the DEM and its associated data
    std::cout << "Reading the DEM: " << opt.dem << std::endl;
    vw::cartography::GeoReference dem_georef;
    if (!read_georeference(dem_georef, opt.dem))
      vw_throw( ArgumentErr() << "The input DEM has no georeference.\n" );
    double dem_nodata_val = -std::numeric_limits<float>::max(); // note we use a float nodata
    if (!vw::read_nodata_val(opt.dem, dem_nodata_val))
      vw_throw( ArgumentErr() << "Could not read the DEM nodata value.\n");
    std::cout << "Read DEM nodata value: " << dem_nodata_val << std::endl;
    DiskImageView<float> dem(opt.dem);
    std::cout << "The DEM width and height are: " << dem.cols() << ' ' << dem.rows() << std::endl;
    ImageViewRef< PixelMask<float> > interp_dem
      = interpolate(create_mask(dem, dem_nodata_val),
                    BicubicInterpolation(), ConstantEdgeExtension());


    // Find the ECEF coordinates of the shape corners
    std::vector<Eigen::Vector3d> xyz_vec;
    find_xyz_at_shape_corners(polyVec,shape_georef, dem_georef, interp_dem, xyz_vec);

    // Compute the water surface using RANSAC
    std::vector<Eigen::Vector3d> dummy_vec(xyz_vec.size()); // Required by the interface
    std::vector<size_t> inlier_indices;
    double inlier_threshold = opt.water_surface_outlier_threshold;
    int    min_num_output_inliers = std::max(xyz_vec.size()/2, size_t(3));
    bool   reduce_min_num_output_inliers_if_no_fit = true;
    vw::Matrix<double> H;
    try {
      math::RandomSampleConsensus<BestFitPlaneFunctor, BestFitPlaneErrorMetric> 
        ransac(BestFitPlaneFunctor(), BestFitPlaneErrorMetric(),
               opt.num_ransac_iterations, inlier_threshold,
               min_num_output_inliers, reduce_min_num_output_inliers_if_no_fit);
    
      H = ransac(xyz_vec, dummy_vec);
      inlier_indices = ransac.inlier_indices(H, xyz_vec, dummy_vec);
    } catch (const vw::math::RANSACErr& e ) {
      vw_out() << "RANSAC Failed: " << e.what() << "\n";
    }
    vw_out() << "Found " << inlier_indices.size() << " final matches.\n";
    std::cout << "Final matrix is " << H << std::endl;

    
  } ASP_STANDARD_CATCHES;
  
  return 0;
}
