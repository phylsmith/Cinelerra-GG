PLATFORM = $(uname -o)
ifneq ($(PLATFORM),Android)
all:
#
#	mv pt-test.c pt-test
#	cat `ls *.c` > just-pt.c
	$(CXX) -c -o jpt.o just-pt.cc -I.
	ar crs jpt.a jpt.o
clean:
	rm -f *.a *.o
install:
	
else
all:
install:
clean:
	
endif
