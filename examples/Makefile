
# set this to the directory that you installed websim
# WSPREFIX=/Users/vaughan/Stage-3.0

WSLDFLAGS=`pkg-config --libs websim`
WSCFLAGS=`pkg-config --cflags websim`

all: clean websimple

clean:
	rm -f websimple

websimple: simple.cc
	g++ ${WSCFLAGS}  simple.cc -o $@ ${WSLDFLAGS}