.PHONY: all sbncloader clean
CXXFLAGS = -g
OBJS = sbncloader.o AssocArray.o ../snprintf.o ../unix.o
CFLAGS = -fPIC

all : sbncloader

sbncloader : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(DL_LIB)

clean :
	-rm $(OBJS)
	-rm sbncloader
