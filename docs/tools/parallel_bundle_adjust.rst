.. _parallel_bundle_adjust:

parallel_bundle_adjust
----------------------

The ``parallel_bundle_adjust`` program is a modification of
``bundle_adjust`` designed to distribute some of the preprocessing steps
over multiple processes and multiple computing nodes. It uses GNU
Parallel to manage the jobs in the same manner as ``parallel_stereo``.
For information on how to set up and use the node list see
:numref:`parallel_stereo`.

The ``parallel_bundle_adjust`` has three processing steps: statistics,
matching, and optimization. Only the first two steps can be done in
parallel and in fact after you have run steps 0 and 1 in a folder with
``parallel_bundle_adjust`` you could just call regular ``bundle_adjust``
to complete processing in the folder. Steps 0 and 1 produce the
-stats.tif and .match files that are used in the last step.

Command-line options for parallel_bundle_adjust:

-h, --help
    Display the help message.

--nodes-list <filename>
    The list of computing nodes, one per line. If not provided, run
    on the local machine.

-e, --entry-point <integer (default: 0)>
    Stereo Pipeline entry point (start at this stage).

--stop-point <integer(default: 1)>
    Stereo Pipeline stop point (stop at the stage *right before*
    this value).

--verbose
    Display the commands being executed.

--processes <integer>
    The number of processes to use per node.

--threads-multiprocess <integer>
    The number of threads to use per process.

--threads-singleprocess <integer>
    The number of threads to use when running a single process (for
    pre-processing and filtering).
