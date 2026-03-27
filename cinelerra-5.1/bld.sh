#!/bin/bash
#  IMPORTANT comments below to change the configure line
# For newer operating system versions, add --enable-libsvtav1
# For older versions, you may need to update nasm and/or cmake or you
# can disable any thirdparty library that has cmake or assembly errors.
# For example an older ubuntu 16 version might need:  add --enable-libaom=no
# To add OpenCV plugins, add --with-opencv=sta,tar=https://download.cinelerra-gg.org/download.php?file=opencv%2Fopencv-20200306.tgz
( ./autogen.sh
  ./configure --with-single-user --with-booby
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile
