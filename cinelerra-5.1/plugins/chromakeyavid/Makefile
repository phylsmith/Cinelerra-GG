include ../../plugin_defs

PLUGIN = chromakeyavid
OUTPUT_BINS := $(OBJDIR)/chromakeyavid_sl.o
OBJS := $(OBJDIR)/chromakey.o

include ../../plugin_config

$(OBJDIR)/chromakey.o: chromakey.C
$(OUTPUT_BINS): chromakeyavid.sl
	cd $(OBJDIR) && \
	../../../guicast/$(OBJDIR)/bootstrap -s chromakeyavid_sl.o ../chromakeyavid.sl

