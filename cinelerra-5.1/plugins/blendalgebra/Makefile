include ../../plugin_defs

PLATFORM = $(shell uname)
ifneq ($(PLATFORM), NetBSD)
LDFLAGS += -ldl
endif

OBJS = $(OBJDIR)/blendalgebra.o

PLUGIN = blendalgebra

all::

include ../../plugin_config

CC ?= gcc

FRONT = BlendAlgebraStart

HELPER = BlendAlgebraCompile.pl

DLFCN_DIR = $(BINDIR)/dlfcn
DLFCN_BA_DIR = $(DLFCN_DIR)/ba

BA_OBJS = \
	ovl_normal.ba.so \
	arith_addition.ba.so \
	arith_subtract.ba.so \
	arith_multiply.ba.so \
	arith_divide.ba.so \
	arith_replace.ba.so \
	porterduff_dst.ba.so \
	porterduff_dstatop.ba.so \
	porterduff_dstin.ba.so \
	porterduff_dstout.ba.so \
	porterduff_dstover.ba.so \
	porterduff_src.ba.so \
	porterduff_srcatop.ba.so \
	porterduff_srcin.ba.so \
	porterduff_srcout.ba.so \
	porterduff_srcover.ba.so \
	logical_min.ba.so \
	logical_max.ba.so \
	logical_lighten.ba.so \
	logical_darken.ba.so \
	logical_and.ba.so \
	logical_or.ba.so \
	logical_xor.ba.so \
	graphart_overlay.ba.so \
	graphart_screen.ba.so \
	graphart_burn.ba.so \
	graphart_dodge.ba.so \
	graphart_difference.ba.so \
	graphart_hardlight.ba.so \
	graphart_softlight.ba.so \
	ydiff.ba.so

BA_SRCS = $(BA_OBJS:.ba.so=.ba)

$(OBJDIR)/blendalgebra.o: blendalgebra.C blendalgebra.h

%.ba.so: %.ba $(FRONT) $(HELPER)
	rm -f $@
	CIN_CC="$(CC)" CIN_DAT= ./$(HELPER) -noapi -compile -opt -warn $<

all:: $(OUTPUT) $(BA_OBJS)

install:: $(BA_OBJS)

install::
	mkdir -p $(DLFCN_DIR)
	mkdir -p $(DLFCN_BA_DIR)
	cp -a $(FRONT) $(HELPER) $(DLFCN_DIR)
	chmod +x $(DLFCN_DIR)/$(HELPER)
	cp -a $(BA_SRCS) $(BA_OBJS) $(DLFCN_BA_DIR)

clean::
	rm -f  $(BA_OBJS) *~
