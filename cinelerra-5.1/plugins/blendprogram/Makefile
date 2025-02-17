include ../../plugin_defs

LDFLAGS += -ldl

OBJS = $(OBJDIR)/blendprogram.o

PLUGIN = blendprogram

all::

include ../../plugin_config

CC ?= gcc

FRONT = BlendProgramStart

HELPER = BlendProgramCompile.pl

DLFCN_DIR = $(BINDIR)/dlfcn
DLFCN_BP_DIR = $(DLFCN_DIR)/bp

BP_OBJS = \
	chromakey.bp.so \
	background.bp.so

BP_SRCS = $(BP_OBJS:.bp.so=.bp)

$(OBJDIR)/blendprogram.o: blendprogram.C blendprogram.h

%.bp.so: %.bp $(FRONT) $(HELPER)
	rm -f $@
	CIN_CC=$(CC) CIN_DAT= ./$(HELPER) -noapi -compile -opt -warn $<

all:: $(OUTPUT) $(BP_OBJS)

install:: $(BP_OBJS)

install::
	mkdir -p $(DLFCN_DIR)
	mkdir -p $(DLFCN_BP_DIR)
	cp -a $(FRONT) $(HELPER) $(DLFCN_DIR)
	chmod +x $(DLFCN_DIR)/$(HELPER)
	cp -a $(BP_SRCS) $(BP_OBJS) $(DLFCN_BP_DIR)

clean::
	rm -f  $(BP_OBJS) *~
