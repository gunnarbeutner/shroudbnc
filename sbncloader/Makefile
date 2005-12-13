CXXFLAGS = -g
LDFLAGS = -ldl
OBJS = sbncloader.o AssocArray.o SimpleIRCConnection.o ../snprintf.o ../unix.o

all : sbncloader

sbncloader : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)
