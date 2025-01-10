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


#include <asp/Camera/RPCModelGen.h>
#include <asp/Camera/RPCModel.h>
#include <vw/Math/Geometry.h>

using namespace vw;

namespace asp {

// There are 20 polynomial coefficients for each of lineNum,
// lineDen, sampNum, sampDen, as the RPC polynomials are of degree
// 3 in x and y.  However, the 0 degree coeffs for both
// denominators is 1, so only 78 coefficients vary. Extract the
// variable coefficients from a vector.
void unpackCoeffs(Vector<double> const& C,
                  RPCModel::CoeffVec& lineNum, RPCModel::CoeffVec& lineDen,
                  RPCModel::CoeffVec& sampNum, RPCModel::CoeffVec& sampDen) {

  VW_ASSERT(C.size() == 78,
            ArgumentErr() << "Must have 78 coefficients.\n");

  int start = 0;
  // Line
  subvector(lineNum, 0, 20) = subvector(C, start, 20); start += 20;
  lineDen[0] = 1;
  subvector(lineDen, 1, 19) = subvector(C, start, 19); start += 19;
  // Sample
  subvector(sampNum, 0, 20) = subvector(C, start, 20); start += 20;
  sampDen[0] = 1;
  subvector(sampDen, 1, 19) = subvector(C, start, 19); start += 19;

  VW_ASSERT(start == 78, ArgumentErr() << "Book-keeping error.\n");

  return;
}

// This function does the reverse of unpackCoeffs().
void packCoeffs(RPCModel::CoeffVec const& lineNum, RPCModel::CoeffVec const& lineDen,
                RPCModel::CoeffVec const& sampNum, RPCModel::CoeffVec const& sampDen,
                Vector<double> & C) {

  C.set_size(78);
  int start = 0;
  subvector(C, start, 20) = subvector(lineNum, 0, 20); start += 20;
  subvector(C, start, 19) = subvector(lineDen, 1, 19); start += 19;
  subvector(C, start, 20) = subvector(sampNum, 0, 20); start += 20;
  subvector(C, start, 19) = subvector(sampDen, 1, 19); start += 19;

  return;
}

// Print out a name followed by the vector of values
void print_vec(std::string const& name, Vector<double> const& vals) {
  std::cout.precision(16);
  std::cout << name << ",";
  int len = vals.size();
  for (int i = 0; i < len - 1; i++)
    std::cout << vals[i] << ",";
  if (len > 0)
    std::cout << vals[len-1];
  std::cout << "\n";
}

// Dump a vector to a text file, one value per row.
void print_vec_to_file(std::string const& path, Vector<double> const& vals) {
  std::ofstream outFile(path.c_str());
  outFile.precision(16);
  int len = vals.size();
  for (int i = 0; i < len - 1; i++)
    outFile << vals[i] << "\n";
  if (len > 0)
    outFile << vals[len-1];
  outFile.close();

}

// Computes a system solution from a seed and returns the final error number.
int find_solution_from_seed(RpcSolveLMA    const& lma_model,
                            Vector<double> const& seed_params,
                            Vector<double> const& actual_observations,
                            Vector<double>      & final_params,
                            double              & norm_error) {

  // Initialize a zero vector of RPC model coefficients
  int status;

  // Use the L-M solver to optimize the RPC model coefficient values.
  const double abs_tolerance  = 1e-24;
  const double rel_tolerance  = 1e-24;
  const int    max_iterations = 2000;
  final_params = math::levenberg_marquardt(lma_model, seed_params, actual_observations,
                                            status, abs_tolerance, rel_tolerance,
                                            max_iterations);

  if (status < 1) { // This means the solver failed to converge
    VW_OUT(DebugMessage, "asp") 
      << "rpc_gen: WARNING --> Levenberg-Marquardt solver status = " << status << "\n";
  }

  // Otherwise the solver converged, return the final error number.
  Vector<double> final_projected = lma_model(final_params);
  Vector<double> final_error     = lma_model.difference(final_projected, actual_observations);
  norm_error = norm_2(final_error);
  return status;
}

// Find the best-fit affine transform, that will serve as initial guess for RPC
void initRpcAsAffine(// Inputs
                     Vector<double> const& normalized_geodetics,
                     Vector<double> const& normalized_pixels,
                     Vector3 const& llh_scale,
                     Vector3 const& llh_offset,
                     Vector2 const& uv_scale,
                     Vector2 const& uv_offset,
                     // Outputs
                     RPCModel::CoeffVec & line_num,
                     RPCModel::CoeffVec & line_den,
                     RPCModel::CoeffVec & samp_num,
                     RPCModel::CoeffVec & samp_den) {
  
  int numPts = normalized_geodetics.size()/RPCModel::GEODETIC_COORD_SIZE;
  int numPts2 = (normalized_pixels.size() - asp::RpcSolveLMA::NUM_PENALTY_TERMS)
    / RPCModel::IMAGE_COORD_SIZE;
  
  if (numPts != numPts2) 
    vw_throw( ArgumentErr() << "Error in " << __FILE__
              << ". Number of inputs and outputs do not agree.\n");
  std::vector< Vector<double, RPCModel::GEODETIC_COORD_SIZE+1> > in(numPts), out(numPts);
  for (int p = 0; p < numPts; p++) {

    Vector<double, RPCModel::GEODETIC_COORD_SIZE+1> P;
    for (int q = 0; q < RPCModel::GEODETIC_COORD_SIZE; q++) 
      P[q] = normalized_geodetics[p*RPCModel::GEODETIC_COORD_SIZE + q]; // P[0], P[1], P[2]
    P[RPCModel::GEODETIC_COORD_SIZE] = 1; // P[3]
    in[p] = P;
    
    for (int q = 0; q < RPCModel::IMAGE_COORD_SIZE; q++) 
      P[q] = normalized_pixels[p*RPCModel::IMAGE_COORD_SIZE + q]; // P[0], P[1]
    P[RPCModel::IMAGE_COORD_SIZE]   = 0; // P[2]
    P[RPCModel::IMAGE_COORD_SIZE+1] = 1; // P[3]
    out[p] = P;
  }
  Matrix4x4 T = math::AffineFittingFunctorN<RPCModel::GEODETIC_COORD_SIZE>()(in, out);

  // Put this matrix in the format acceptable for the RPC solver
  for (int p = 0; p < int(line_num.size()); p++) {
    samp_num[p] = 0; samp_den[p] = 0; // first coordinate of output is sample
    line_num[p] = 0; line_den[p] = 0; // second coordinate of output is line
  }

  // The first coordinate of the output
  samp_num[0] = T(0, 3); // the d value, the translation, in a*x + b*y + c*z + d
  samp_num[1] = T(0, 0); samp_num[2] = T(0, 1); samp_num[3] = T(0, 2); // linear part, a, b, c
  
  // The second coordinate of the output
  line_num[0] = T(1, 3); // the d value, the translation, in a*x + b*y + c*z + d
  line_num[1] = T(1, 0); line_num[2] = T(1, 1); line_num[3] = T(1, 2); // linear part, a, b, c

  // The denominator is just 1 to start
  samp_den[0] = 1.0;
  line_den[0] = 1.0;
}
  
void gen_rpc(// Inputs
             double penalty_weight,
             Vector<double> const& normalized_geodetics,
             Vector<double> const& normalized_pixels,
             Vector3 const& llh_scale,
             Vector3 const& llh_offset,
             Vector2 const& uv_scale,
             Vector2 const& uv_offset,
             // Outputs
             RPCModel::CoeffVec & line_num,
             RPCModel::CoeffVec & line_den,
             RPCModel::CoeffVec & samp_num,
             RPCModel::CoeffVec & samp_den) {

  VW_ASSERT(penalty_weight >= 0, 
            ArgumentErr() << "The RPC penalty weight must be non-negative.\n" );

  // The percentage of the error that the penalty weights should represent
  double penalty_weight_fraction = penalty_weight;
  // Fraction with no adjustment
  double native_penalty_fraction
    = (double)RpcSolveLMA::NUM_PENALTY_TERMS / (double)normalized_pixels.size();
  double penalty_adjustment = penalty_weight_fraction / native_penalty_fraction;

  // Initialize the RPC model with an affine transform
  initRpcAsAffine(normalized_geodetics, normalized_pixels,
                  llh_scale, llh_offset, uv_scale, uv_offset,
                  line_num, line_den, samp_num, samp_den);      
  
  // Initialize the model
  Vector<double> startGuess;
  startGuess.set_size(RPCModel::NUM_RPC_COEFFS);
  packCoeffs(line_num, line_den, samp_num, samp_den, startGuess);

  Vector<double> solution;
  double norm_error = -1.0;

  // Initialize a specialized least squares solver object and load the input data
  RpcSolveLMA lma_model(normalized_geodetics, normalized_pixels, penalty_adjustment);

  // Use the L-M solver to optimize the RPC model coefficient values.
  int status = find_solution_from_seed(lma_model, startGuess, normalized_pixels,
                                       solution, norm_error);
  VW_OUT(DebugMessage, "asp") << "Solved RPC coeffs: " << solution << "\n";
  VW_OUT(DebugMessage, "asp") << "rpc_gen: norm_error = " << norm_error << "\n";

  // If we ever want to improve our results further we should
  // experiment with multiple starting seeds
  unpackCoeffs(solution, line_num, line_den, samp_num, samp_den);
}

} // end namespace asp
