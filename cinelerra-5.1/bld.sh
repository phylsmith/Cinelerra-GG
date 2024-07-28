#!/bin/bash
#  IMPORTANT comments below to change the configure line
# For newer operating system versions, add --enable-libsvtav1
# For really old versions, such as ubuntu 16, add --enable-libaom=no
# To add OpenCV plugins, add --with-opencv=sta,tar=http://cinelerra-gg.org/download/opencv/opencv-20200306.tgz
( ./autogen.sh
  ./configure --with-single-user --with-booby
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile
