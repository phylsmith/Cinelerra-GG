#!/bin/bash
#  IMPORTANT comments below to change the configure line
# For newer operating system versions, add --enable-libsvtav1
# For really old versions, such as ubuntu 16, add --enable-libaom=no
# Faster compile if include --disable-x265_hidepth
# To add OpenCV plugins, add --with-opencv=sta,tar=https://download.cinelerra-gg.org/download.php?file=opencv%2Fopencv-20200306.tgz
( ./autogen.sh
  ./configure --with-single-user --with-booby
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile
