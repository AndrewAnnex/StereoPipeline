#!/bin/bash

# Set up the compiler
isMac=$(uname -s | grep Darwin)
if [ "$isMac" != "" ]; then
  cc_comp=clang
  cxx_comp=clang++
else
  cc_comp=x86_64-conda_cos6-linux-gnu-gcc
  cxx_comp=x86_64-conda_cos6-linux-gnu-g++
fi

# Set up some variables
aspRepoDir=$(pwd) # $HOME/work/StereoPipeline/StereoPipeline
baseDir=$HOME/work/StereoPipeline
installDir=$baseDir/install
envPath=/usr/local/miniconda/envs/asp_deps
# packageDir will later be uploaded, as set in the yml file
packageDir=$baseDir/packages 
testDir=$baseDir/StereoPipelineTest

# Build visionworkbench
mkdir -p $baseDir
cd $baseDir
git clone https://github.com/visionworkbench/visionworkbench.git
cd visionworkbench
mkdir -p build
cd build
$envPath/bin/cmake ..                             \
  -DASP_DEPS_DIR=$envPath                         \
  -DCMAKE_VERBOSE_MAKEFILE=ON                     \
  -DCMAKE_INSTALL_PREFIX=$installDir              \
  -DCMAKE_C_COMPILER=${envPath}/bin/$cc_comp      \
  -DCMAKE_CXX_COMPILER=${envPath}/bin/$cxx_comp
make -j10 && make install

# Build StereoPipeline
cd $aspRepoDir
mkdir -p build
cd build
$envPath/bin/cmake ..                             \
  -DASP_DEPS_DIR=$envPath                         \
  -DCMAKE_VERBOSE_MAKEFILE=ON                     \
  -DCMAKE_INSTALL_PREFIX=$installDir              \
  -DVISIONWORKBENCH_INSTALL_DIR=$installDir       \
  -DCMAKE_C_COMPILER=${envPath}/bin/$cc_comp      \
  -DCMAKE_CXX_COMPILER=${envPath}/bin/$cxx_comp
make -j10 && make install

# Now package with BinaryBuilder
cd $baseDir
git clone https://github.com/NeoGeographyToolkit/BinaryBuilder
cd BinaryBuilder
./make-dist.py $installDir \
  --asp-deps-dir $envPath  \
  --python-env /usr/local/miniconda/envs/python_isis8

# Prepare the package for upload
mkdir -p $packageDir
mv -fv Stereo* $packageDir

# Extract the tarball
cd $packageDir
tarBall=$(ls StereoPipeline-*OSX.tar.bz2)
if [ "$tarBall" == "" ]; then
  echo Cannot find the packaged ASP tarball
  exit 1
fi
tar xjfv $tarBall

# Path to executables
binDir=$packageDir/$tarBall
binDir=${binDir/.tar.bz2/}
binDir=$binDir/bin
export PATH=$binDir:$PATH
echo "Binaries are in $binDir"
if [ ! -d "$binDir" ]; then
    echo "Error: Directory: $binDir does not exist"
    exit 1
fi

# Extract the tests
cd $baseDir
wget https://github.com/NeoGeographyToolkit/StereoPipelineTest/releases/download/0.0.1/StereoPipelineTest.tar
tar xfv StereoPipelineTest.tar

# Go to test dir
if [ ! -d "$testDir" ]; then
    echo "Error: Directory: $testDir does not exist"
    exit 1
fi
cd $testDir

# Run the tests. Failed to install pytest, despite trying hard.
# Just run them manually.
ans=0
for d in ss*; do 
    # Skip unless a directory
    if [ ! -d "$d" ]; then continue; fi
    cd $d
    pwd
    ./run.sh >/dev/null 2>&1
    ./validate.sh
    ans0=$?
    echo "Test $d returned $ans0"
    # increment to ans
    if [ "$ans0" -ne 0 ]; then
        ans=1
    fi
    cd ..
done
echo ans is $ans

if [ "$ans" -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
    exit 1
fi
    