#! /bin/bash

# Call this as "./autogen.sh clean" to prepare for
# distribution. 
# start from scratch

if [ -s Makefile ];
then
    make clean 
fi

rm -f global_config configure Makefile Makefile.in
rm -f aclocal.m4 ltmain.sh stamp-h1
rm -f config.{log,guess,h,h.in,sub,status}
rm -rf autom4te.cache cfg .deps 

if [ "$1" = "clean" ]; then exit 0; fi

#autoupdate
mkdir cfg
autoreconf --install

# After this, run ./configure, then make as
# "make -j$(nproc)" (on linux)

