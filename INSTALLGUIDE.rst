Installation
============

Precompiled binaries are available for the stable releases and the
current development build.  Stereo Pipeline can also be compiled 
from source, but this is not recommended.

Precompiled Binaries (Linux and macOS)
--------------------------------------

Simply download the appropriate distirbution for your operating
system, extract, and run the executables in the ``bin`` subdirectory.
No other 'installation' steps or administrative rights are necessary.

- `Stable Releases
  <https://github.com/NeoGeographyToolkit/StereoPipeline/releases>`_

- `Development Build <http://byss.arc.nasa.gov/stereopipeline/daily_build/>`_

See the NEWS file for the most recent additions.

To permanently add the ASP executable subdirectory to your PATH,
you can add the following line to your shell configuration (e.g.,
``~/.bashrc``), replacing ``/path/to/StereoPipeline/bin`` with the location
on your filesystem: ``export PATH=${PATH}:/path/to/StereoPipeline/bin``


Planetary Images
~~~~~~~~~~~~~~~~

If you plan to process images from NASA's spacecraft that are
exploring other planets, you will probably need to have :term:`ISIS`
installed.  A full ISIS installation is not required for operation
of the main Stereo Pipeline programs (only the ISIS data directory
is needed), but it is required for certain preprocessing steps
before Stereo Pipeline programs are run for planetary data.  If you
only want to process terrestrial Digital Globe images, skip to the
`Quick Start for Digital Globe Users`_ section.

To perform pre-processing (radiometric caligration, ephemeris
processing, etc.), of non-terrestrial images prior to running Stereo
Pipeline, you will need to install :term:`ISIS`.  Just as with our 
binaries, you can use the ISIS binaries as-is.

If you need to recompile, you can follow the instructions for
`Building from Source`_ (but we don't recommend it).  If the
current version of ISIS is newer than the version of ISIS that the
Stereo Pipeline is compiled against (listed in the ASP Release
Notes), be assured that we're working on rolling out a new version.
However, since Stereo Pipeline has its own self-contained version
of ISIS's libraries built internally, you should be able to use a
newer version of ISIS with the now dated version of ASP. This is
assuming no major changes have taken place in the data formats or
camera models by the ISIS Developers. At the very least, you should
be able to install older versions of ISIS if a failure is found.
To do so, follow the ISIS installation instructions, but create a
new conda environment (not the one with your current ISIS), and right
before you would run ``conda install isis3``, run ``conda search
isis`` to find all of the versions of ISIS available for installation.
For example, if you wanted to install ISIS 3.9.0, and it is available
in the ``conda search isis`` listing, you can run ``conda install
isis3=3.9.0`` (to install that specific version of ISIS) and then
follow the remainder of the ISIS installation instructions.

In closing, running the Stereo Pipeline executables only requires
that you have downloaded the ISIS secondary data and have
appropriately set the ``ISIS3DATA`` environment variable. This is
normally performed for the user by starting up the conda ISIS 
environment.

Quick Start for ISIS Users
~~~~~~~~~~~~~~~~~~~~~~~~~~

#. Fetch Stereo Pipeline from
   https://github.com/NeoGeographyToolkit/StereoPipeline/releases

#. Fetch ISIS Binaries and install, following
   https://github.com/USGS-Astrogeology/ISIS3#installation

#. Fetch ISIS Data, as detailed at
   https://github.com/USGS-Astrogeology/ISIS3#the-isis-data-area

#. Untar Stereo Pipeline::

     tar xzvf StereoPipeline-<VERSION>-<ARCH>-<OS>.tar.gz

#. Add Stereo Pipeline to Path (optional):

   - bash: ``export PATH="</path/to/StereoPipeline>/bin:${PATH}"``
   - csh: ``setenv PATH "</path/to/StereoPipeline>/bin:${PATH}"``

#. Try It Out: See :numref:`moc_tutorial` for an example.


Quick Start for Digital Globe Users
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. Fetch Stereo Pipeline from
   https://github.com/NeoGeographyToolkit/StereoPipeline/releases

#. Untar Stereo Pipeline::

     tar xzvf StereoPipeline-<VERSION>-<ARCH>-<OS>.tar.gz

#. Try It Out: Processing Earth images is described in the data processing
   tutorial in :numref:`dg_tutorial`.


Quick Start for Aerial and Historical Images
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Fetch the software as above. Processing images without accurate camera
pose information is described in :numref:`sfm`.


Common Errors
-------------

Here are some errors you might see, and what it could mean. Treat these
as templates for problems. In practice, the error messages might be
slightly different.

::

   **I/O ERROR** Unable to open [$ISIS3DATA/<Some/Path/Here>].
   Stereo step 0: Preprocessing failed

You need to set up your ISIS environment or manually set the correct
location for ``ISIS3DATA``.

::

   point2mesh stereo-output-PC.tif stereo-output-L.tif
   [...]
   99%  Vertices:   [**********************************] Complete!
          > size: 82212 vertices
   Drawing Triangle Strips
   Attaching Texture Data
   zsh: bus error  point2mesh stereo-output-PC.tif stereo-output-L.tif

The source of this problem is an old version of OpenSceneGraph in your
library path. Check your ``LD_LIBRARY_PATH`` (for Linux),
``DYLD_LIBRARY_PATH`` (for macOS), or your ``DYLD_FALLBACK_LIBRARY_PATH``
(for macOS) to see if you have an old version listed, and remove it from
the path if that is the case. It is not necessary to remove the old
versions from your computer, you just need to remove the reference to
them from your library path.

::

   bash: stereo: command not found

You need to add the ``bin`` directory of your deployed Stereo Pipeline
installation to the environmental variable ``PATH``.


Building from Source
--------------------

This method is for advanced users. You will need to fetch the Stereo
Pipeline source code from GitHub at
https://github.com/NeoGeographyToolkit/StereoPipeline and then
follow these instructions.

Building Stereo Pipeline from source can be difficult, due to the
large number of dependencies, and the variety of Linux and Mac
architectures that Stereo Pipeline supports. A separate software
package called `BinaryBuilder
<https://github.com/NeoGeographyToolkit/BinaryBuilder>`_ will take
care of setting up the build environment and building the code. We
describe below what dependencies BinaryBuilder needs and then how
to invoke it.

Base Dependencies
~~~~~~~~~~~~~~~~~

Linux
.....

It may be potentially easier to build Stereo Pipeline and its
dependencies in a virtual machine. How to set one up is described
in the file VIRTMACHINES.

The following packages need to be installed in order to build
Stereo Pipeline (this list is not exhaustive, see the precise
commands to install dependencies below):

It is very important to note that the only GCC version that 
ASP can be built with is gcc 5. Version 4 is too old, and some 
dependencies do not build with version 6.

- Python (version >= 3 preferred, version >= 2.7 supported--but not for long)
- gcc, g++, gfortran
- cmake (version >= 3.11)
- csh
- libtool
- autoconf
- automake
- openssl-dev
- wget
- curl
- git (version >= 1.6) 
- subversion
- zip
- xserver-xorg-dev
- xorg-dev
- libx11-dev
- libxext-dev
- libxmu
- libxmu-dev
- libxi-dev
- libxcb-dev
- libgl1-mesa-dev
- libglu1-mesa-dev
- freeglut3-dev
- gtk2-dev

If you have root access on your machine you can install them on a
Debian-based distribution (for example Ubuntu version >= 16) using the
following command (note that sometimes the precise names of packages
may change and perhaps some new repository may need to be added)::

     sudo apt-get update -y
     sudo apt-get install -y gcc g++ gfortran tcsh libtool binutils     \
        m4 autoconf automake libssl-dev wget curl git subversion zip    \
        xorg-dev libx11-dev libxext-dev libxmu6 libxmu-dev libxi-dev    \
        '^libxcb.*-dev' libx11-xcb-dev libgl1-mesa-dev libglu1-mesa-dev \
        freeglut3-dev libgtk2.0-dev texlive-latex-base graphviz texinfo

For Red Hat-based distributions (CentOS/RHEL version >= 7) one can 
do instead::

     sudo yum update -y
     sudo yum -y install python gcc-c++ gcc-gfortran tcsh libtool m4 \
        autoconf automake openssl-devel wget curl git subversion     \
        xorg-x11-server-Xorg libX11-devel libXext-devel libXmu       \
        libXmu-devel libXi-devel libxcb libxcb-devel xcb-util        \
        xcb-util-devel mesa-libGL-devel freeglut-devel gtk2-devel    \
        patch texlive texlive-latex texlive-latexextra               \
        texlive-collection\* graphviz texinfo texinfo-tex            \

If your git install fails on Red Hat, you may need to enable the EPEL
repository. 

On Linux, gcc 5 can be built from scratch. Here is an example for CentOS 7::

     sudo yum -y install gmp-devel mpfr-devel libmpc-devel glibc-devel \
        glibc-devel.i686 zip unzip jar
     wget https://ftp.gnu.org/gnu/gcc/gcc-5.4.0/gcc-5.4.0.tar.bz2

Then unzip the archive and run in the extracted directory::

     ./configure --prefix=$HOME/projects/gcc5 --enable-gold=yes \
        --enable-ld=yes --disable-multilib 

followed by running make and installing it.

Set::

     export LD_LIBRARY_PATH=<path to libstdc++ for your version compliant gcc>:$LD_LIBRARY_PATH


macOS
.....

First install Homebrew or Macports, if you do not already have it.

Next you need the following packages:

- automake
- libtool
- openssl
- git
- wget 
- curl
- xz

Here's an example for how to install some of these. First read
http://superuser.com/questions/619498/can-i-install-homebrew-without-sudo-privileges
about how to install homebrew without sudo. Then do::

    export HOMEBREW_PREFIX=$HOME/usr/local
    export PATH=$HOMEBREW_PREFIX/bin:$PATH

    brew update
    brew doctor
    brew install automake libtool openssl git wget curl xz

All our software is built with clang on the Mac.


Setting up ISIS dependencies via conda
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ASP depends heavily on :term:`ISIS` and its dependencies. The
dependencies should be installed using conda based on the 
instructions at: https://github.com/USGS-Astrogeology/ISIS3/wiki/Developing-ISIS3-with-cmake

This is needed even if it is desired to build VisionWorkbench only.

One should always use the dependencies in environment.yml on all
platforms, as we use gcc 5, rather than environment_gcc4.yml mentioned
there for CentOS.

Some of the .la files created by conda point to other .la files that
are not available.  For that reason, those files should be edited to
replace::

    /path/to/libmylibrary.la

with::

    -L/path/to -lmylibrary

This can be done with the following commands::

    cd ~/miniconda3/envs/isis3/lib
    mkdir -p  backup
    cp -fv  *la backup
    perl -pi -e "s#(/[^\s]*?lib)/lib([^\s]+).la#-L\$1 -l\$2#g" *la

At some point in the near future likely all dependencies, 
including the ones installed so far in a system location using
apt-get or yum, can likely be transitioned to using conda and 
having them in user space.


Invoking Binary Builder
~~~~~~~~~~~~~~~~~~~~~~~

Having installed the tools, base libraries, and ISIS, the following
lines of code will start the build of Stereo Pipeline in the
directory ``~/projects/BinaryBuilder``::

    cd ~; mkdir projects; cd projects
    git clone https://github.com/NeoGeographyToolkit/BinaryBuilder.git
    cd BinaryBuilder
    conda activate isis3
    source ./auto_build/utils.sh
    ./build.py 

One may need to set some paths in ``./auto_build/utils.sh`` to get
things to work.
   
One can specify the compilers as::

    ./build.py --cc=/path/to/gcc --cxx=/path/to/g++ --gfortran=/path/to/gfortran

If the conda packages were installed in a location other than
``$HOME/miniconda3/envs/isis3``, the path to that directory should be
set via ``--isis3-deps-dir``.

Due to the amount of code that must be downloaded and built,
BinaryBuilder will take quite a while to finish.  If you see the
message "All done!" then it has succeeded.  Otherwise something has
gone wrong and must be fixed before the build can continue. Often this
is due to one of the dependencies listed earlier being too old or
missing.

If the build failed and you need to restart it after finding a fix,
the name of the individual package that needs to be built can be
passed to ``build.py`` as an argument. Note that this tool keeps track of
built packages in::

    build_asp/done.txt

so to force one to rebuild one can remove its entry from there.

Once the build is successful you should make a distribution tarball to
store the completed build. Do so using the following command from the
BinaryBuilder directory::

    ./make-dist.py last-completed-run/install


Building Documentation
----------------------

The ASP documentation is encoded in ReStructured Text and is built
with the Sphinx-Doc system (https://www.sphinx-doc.org) with 
sphinxcontrib-bibtex (https://sphinxcontrib-bibtex.readthedocs.io).

The ``docs\`` directory contains the root of the documentation and 
``make html`` and ``make latexpdf`` will create the HTML and PDF versions
of the documentation.
