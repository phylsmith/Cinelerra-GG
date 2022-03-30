#!/bin/bash

# Build Cinelerra-GG as AppImage.
# This follows four steps:
# - Build static version of Cinelerra-GG.
# - If desired and needed, build the manual for context-sensitive help.
# - Organize code and manual in a AppImage specific format AppDir. Do
#   this with tool makeappimage, which is built if needed.
# - Call an external tool to make the AppDuir into an AppImage

# We don't do cross-compiling, so the system we're running on
# is the target too. Some extra options have to be passed to
# the configure script depending on the target. The configure.ac
# file has something for android, but that is an OS, not a hardware
# platform

target=$(uname -m)
echo target = $target
CONFIG_OPTIONS="--with-single-user --with-booby --enable-static-build" 
if [ $target = "aarch64" ] ; then
CONFIG_OPTIONS="$CONFIG_OPTIONS --disable-dav1d"
elif [ $target = "armv7l" ] ; then
CONFIG_OPTIONS="$CONFIG_OPTIONS --disable-dav1d --disable-libaom --disable libwebp --without-nv"
fi
echo configure script options are $CONFIG_OPTIONS

# Build Cinelerra itself.
( ./autogen.sh
  ./configure $CONFIG_OPTIONS
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile

# To include the context-sensitive help, specify the directory
# where the manual source is. This script will then call the "translate"
# script there to build the manual, and copy it to the proper place
# before the AppImage is built.

CINGG_DIRECTORY=$(pwd)
MANUAL_DIRECTORY=$(pwd)/../../cin-manual-latex

# Build the manual if the directory is specified and exists, and copy it
# to CinGG
if [ -d "$MANUAL_DIRECTORY" ]; then
  pushd $MANUAL_DIRECTORY
  # run the build in a subshell so if some error causes an exit
  # the popd will still work.
  (./translate_manual) 2>&1 | tee $CINGG_DIRECTORY/manual.log
  popd
  rm -rf bin/doc/CinelerraGG_Manual
  cp -r $MANUAL_DIRECTORY/CinelerraGG_Manual bin/doc/
fi

rm -rf AppDir           # remove old in case a file gets deleted from the distribution
mkdir AppDir            # create lowest level
mkdir AppDir/usr

cp -r bin AppDir/usr/   # copy whole of bin directory

# Use own version of linuxdeploy, makeappimage. This should be in
# subdirectory tools, if not, go to tools/makeappimagetool and do bld.sh
# there. That will place the makeappimage executable in tools, and
# then delete all other generated files (except the small log). The tools
# directory is not cleared with "make clean" or "autogen.sh clean"

if [ ! -f tools/makeappimage ]; then
	export cingg=`pwd`
	cd tools/makeappimagetool
	./bld.sh
	cd $cingg
fi

# We need to specify all executables, so makeappimage can pick up dependencies.
# Any executable code in other places is not picked up (yet).
# makeappimage will populate the AppDir directory, and then call a platform dependent
# appimagetool which must be in the path. The four supported platforms are:
# - appimagetool-aarch64.AppImage
# - appimagetool-armhf.AppImage
# - appimagetool-i686.AppImage
# - appimagetool-x86_64.AppImage
# Get the appropriate appimagetool from https://github.com/AppImage/AppImageKit/releases
# and put it in your path. Only install the version for your platform
# and mark it executable. The file name must start with "appimagetool".

tools/makeappimage --appdir=AppDir -d image/cin.desktop -i image/cin.svg -e bin/cin -e bin/mpeg2enc -e bin/mplex -e bin/hveg2enc -e bin/lv2ui -e bin/bdwrite -e bin/zmpeg3toc -e bin/zmpeg3show -e bin/zmpeg3cat -e bin/zmpeg3ifochk -e bin/zmpeg3cc2txt -e bin/mplexlo 2>&1 | tee appimage.log

# There is now an appimage in the cinelerra-5.1 directory. 
