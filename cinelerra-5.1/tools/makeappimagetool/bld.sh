# Build for for CinGG's own version of linuxdeploy, suitable for
# aarch64 and armhf too.

set +x

( ./autogen.sh
  ./configure
  make -j$(nproc) ) 2>&1 | tee log

# TODO: avoid building with -g (=debug) option, causes
# large files. This options is put there by ./configure. This
# required objcopy, which is needed for CinGG build anyway.

# for debug, comment this out
objcopy -S makeappimage

# Because this is not part of Cinelerra proper, there is no need
# to rebuild it each time. To force a rebuild, remove the MakeAppImage
# executable from the tools directory, then it will be rebuild next
# time the CinGG AppImage is built.
# All generated files except the build log are deleted by "autogen.sh clean"

mv makeappimage ..

./autogen.sh clean
