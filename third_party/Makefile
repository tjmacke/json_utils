CWD = $(shell pwd)
ROOT = $(shell dirname $(CWD))

all:	clean
	tar xvzf jansson-2.5.tar.gz
	cd jansson-2.5 ; ./configure --prefix=$(ROOT) ; make ; make install

clean:
	rm -rf jansson-2.5
