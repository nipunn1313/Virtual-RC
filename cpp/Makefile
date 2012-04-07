all:
	g++ -shared -fPIC -o MOGBlob.so MOGBlob.cpp \
		`pkg-config opencv --cflags --libs` -lblob \
		-I/usr/include/python2.7 -lboost_python -lpython2.7

clean:
	rm MOGBlob
