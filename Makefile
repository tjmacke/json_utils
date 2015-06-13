code:
	cd third_party ; make
	cd src ; make ; make install
test::
	cd test ; make

clean:
	cd src ; make clean
	cd third_party ; make clean
	rm -rf bin lib include
