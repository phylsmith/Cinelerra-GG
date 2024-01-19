#!/bin/bash
#  IMPORTANT comments below to change the configure line
# For python version 12, add --without-lv2
# For newer operating system versions, add --enable-libsvtav1
# For really old versions, such as ubuntu 16, add --enable-libaom=no
( ./autogen.sh
  ./configure --with-single-user --with-booby
  make && make install ) 2>&1 | tee log
mv Makefile Makefile.cfg
cp Makefile.devel Makefile
