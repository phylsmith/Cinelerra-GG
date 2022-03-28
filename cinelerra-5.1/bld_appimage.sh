#!/bin/bash

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

# this should basically be the same as a static build, but the 
# "make install" is followed by the appimage creation. After the
# install does its work, the bin directory and subdirectories 
# contain the whole application.

# Build Cinelerra itself.
( ./autogen.sh
  ./configure $CONFIG_OPTIONS
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile
			
rm -rf AppDir           # remove old in case a file gets deleted from the distribution
mkdir AppDir            # create lowest level
mkdir AppDir/usr

cp -r bin AppDir/usr/   # copy whole of bin directory

# Use own version of linuxdeploy, makeappimage, plus plugin makeappimageplugin.
# They should be in the subdirectory tools, if not, go to tools/makeappimagetool
# and do bld.sh there. That will place the two needed executables in tools, and
# then delete all other generated files (except the small log).

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
# and put it in your path. Only install the version for your platform.

tools/makeappimage --appdir=AppDir -d image/cin.desktop -i image/cin.svg -e bin/cin -e bin/mpeg2enc -e bin/mplex -e bin/hveg2enc -e bin/lv2ui -e bin/bdwrite -e bin/zmpeg3toc -e bin/zmpeg3show -e bin/zmpeg3cat -e bin/zmpeg3ifochk -e bin/zmpeg3cc2txt -e bin/mplexlo 2>&1 | tee appimage.log

# There is now an appimage in the cinelerra-5.1 directory. 
