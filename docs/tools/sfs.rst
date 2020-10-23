.. _sfs:

sfs
---

The ``sfs`` tool can improve a DEM using shape-from-shading. Examples
for how to invoke it are in the :ref:`SfS usage <sfs_usage>` chapter. The tool
``parallel_sfs`` (:numref:`parallel_sfs`) extends ``sfs`` to run using multiple
processes and potentially on multiple machines.

Usage::

     sfs -i <input DEM> -n <max iterations> -o <output prefix> <images> [other options]

The tool outputs at each iteration the current DEM and a slew of other
auxiliary and appropriately-named datasets.

Command-line options:

-i, --input-dem <filename>
    The input DEM to refine using SfS.

-o, --output-prefix <string>
    Prefix for output filenames.

-n, --max-iterations <integer (default: 100)>
    Set the maximum number of iterations.

--reflectance-type <integer (default: 1)>
    Reflectance types:
    0. Lambertian
    1. Lunar-Lambert
    2. Hapke
    3. Experimental extension of Lunar-Lambert
    4. Charon model (a variation of Lunar-Lambert)

--smoothness-weight <float (default 0.04)>
    A larger value will result in a smoother solution.

--initial-dem-constraint-weight <float (default:0)>
    A larger value will try harder to keep the SfS-optimized DEM
    closer to the initial guess DEM.

--albedo-constraint-weight <float (default: 0)>
    If floating the albedo, a larger value will try harder to keep
    the optimized albedo close to the nominal value of 1.

--bundle-adjust-prefix <path>
    Use the camera adjustments obtained by previously running
    bundle_adjust with this output prefix.

--float-albedo
    Float the albedo for each pixel.  Will give incorrect results
    if only one image is present.

--float-exposure
    Float the exposure for each image. Will give incorrect results
    if only one image is present.

--float-cameras
    Float the camera pose for each image except the first one.

--float-all-cameras
    Float the camera pose for each image, including the first one. Experimental.

--model-shadows
    Model the fact that some points on the DEM are in the shadow
    (occluded from the Sun).

--shadow-thresholds <arg>
    Optional shadow thresholds for the input images (a list of real
    values in quotes, one per image).

--shadow-threshold <arg>
    A shadow threshold to apply to all images instead of using
    individual thresholds. (Must be positive.)

--custom-shadow-threshold-list <arg> 
    A list having one image and one shadow threshold per line. For the
    images specified there, override the shadow threshold supplied by
    other means with this value.

--robust-threshold <arg>
    If positive, set the threshold for the robust
    measured-to-simulated intensity difference (using the Cauchy
    loss). Any difference much larger than this will be penalized.

--estimate-height-errors
    Estimate the SfS DEM height uncertainty by finding the height
    perturbation at each grid point which will make at least one of
    the simulated images at that point change by more than twice the
    discrepancy between the unperturbed simulated image and the
    measured image. The SfS DEM must be provided via the -i option.

--save-dem-with-nodata
    Save a copy of the DEM while using a no-data value at a DEM
    grid point where all images show shadows. To be used if shadow
    thresholds are set.

--use-approx-camera-models
    Use approximate camera models for speed.

--use-rpc-approximation
    Use RPC approximations for the camera models instead of approximate
    tabulated camera models (invoke with ``–use-approx-camera-models``).
    This is broken and should not be used.

--rpc-penalty-weight <float (default: 0.1)>
    The RPC penalty weight to use to keep the higher-order RPC
    coefficients small, if the RPC model approximation is used.
    Higher penalty weight results in smaller such coefficients.

--coarse-levels <integer (default: 0)>
    Solve the problem on a grid coarser than the original by a
    factor of 2 to this power, then refine the solution on finer
    grids. Experimental.

--max-coarse-iterations <integer (default: 50)>
    How many iterations to do at levels of resolution coarser than
    the final result.

--crop-input-images
    Crop the images to a region that was computed to be large enough
    and keep them fully in memory, for speed.

--blending-dist <integer (default: 0)>
    Give less weight to image pixels close to no-data or boundary
    values. Enabled only when crop-input-images is true, for
    performance reasons. Blend over this many pixels.

--blending-power <integer (default: 2)>
    A higher value will result in smoother blending.

--min-blend-size <integer (default: 0)>
    Do not apply blending in shadowed areas of dimensions less than this.

--compute-exposures-only
    Quit after saving the exposures.  This should be done once for
    a big DEM, before using these for small sub-clips without
    recomputing them.

--image-exposures-prefix <path>
    Use this prefix to optionally read initial exposures (filename
    is ``<path>-exposures.txt``).

--model-coeffs-prefix <path>
    Use this prefix to optionally read model coefficients from a
    file (filename is ``<path>-model_coeffs.txt``).

--model-coeffs <string of space-separated numbers>
    Use the model coefficients specified as a list of numbers in
    quotes. For example:

    * Lunar-Lambertian: O, A, B, C, would be ``"1 0.019 0.000242 -0.00000146"``
    * Hapke: omega, b, c, B0, h, would be  ``"0.68 0.17 0.62 0.52 0.52"``
    * Charon: A, f(alpha), would be ``"0.7 0.63"``

--crop-win <xoff yoff xsize ysize>
    Crop the input DEM to this region before continuing.

--init-dem-height <float (default: nan)>
    Use this value for initial DEM heights. An input DEM still needs
    to be provided for georeference information.

--nodata-value <float (default: nan)>
    Use this as the DEM no-data value, over-riding what is in the
    initial guess DEM.

--float-dem-at-boundary
    Allow the DEM values at the boundary of the region to also float
    (not advised).

--fix-dem
    Do not float the DEM at all.  Useful when floating the model params.

--float-reflectance-model
    Allow the coefficients of the reflectance model to float (not
    recommended).

--integrability-constraint-weight <float (default: 0.0)>
    Use the integrability constraint from Horn 1990 with this value
    of its weight (experimental).

--smoothness-weight-pq <float (default: 0.0)>
    Smoothness weight for p and q, when the integrability constraint
    is used. A larger value will result in a smoother solution
    (experimental).

--query
    Print some info, including DEM size and the solar azimuth and
    elevation for the images, and exit. Invoked from parallel_sfs.

--camera-position-step-size <integer (default: 1)>
    Larger step size will result in more aggressiveness in varying
    the camera position if it is being floated (which may result
    in a better solution or in divergence).

--threads <integer (default: 0)>
    Select the number of processors (threads) to use.

--no-bigtiff
    Tell GDAL to not create bigtiffs.

--tif-compress <None|LZW|Deflate|Packbits (default: LZW)>
    TIFF Compression method.

-v, --version
    Display the version of software.

-h, --help
    Display this help message.
