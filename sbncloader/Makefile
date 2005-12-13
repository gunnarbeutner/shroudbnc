.PHONY: all sbncloader clean
CXXFLAGS = -g
OBJS = sbncloader.o AssocArray.o SimpleIRCConnection.o ../snprintf.o ../unix.o

all : sbncloader

sbncloader : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(DL_LIB)

clean :
	-rm $(OBJS)
	-rm sbncloader
