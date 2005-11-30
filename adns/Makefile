.PHONY: all clean

all : libadns.a

libadns.a: libadns.a(check.o) libadns.a(event.o) libadns.a(general.o) libadns.a(parse.o) libadns.a(poll.o) libadns.a(query.o) libadns.a(reply.o) libadns.a(setup.o) libadns.a(transmit.o) libadns.a(types.o)
	ranlib libadns.a

clean:
	-rm *.o libadns.a
