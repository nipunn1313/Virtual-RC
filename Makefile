TRACKER_DIR=src/Tracker
GAME_DIR=src/Game
BLOBS_DIR=src/cvblobs8.3_linux
LIB_DIR=lib

CC=g++ -O3 -g
CC_FLAGS=`pkg-config opencv --cflags --libs` \
		 -I$(BLOBS_DIR) -I/usr/include/python2.7 \
		 -L$(BLOBS_DIR) \
		 -lblob -lboost_python -lpython2.7

all: blobs tracker game

blobs:
	cd $(BLOBS_DIR) ; test -f libblob.a || make

tracker: trackerso trackerexec 
	
trackerexec: $(TRACKER_DIR)/MOGBlob.cpp
	$(CC) -o MOGBlob -DMULTI_DISPLAY $(TRACKER_DIR)/MOGBlob.cpp $(CC_FLAGS)

trackerso: $(TRACKER_DIR)/MOGBlob.cpp
	mkdir -p $(LIB_DIR)
	$(CC) -fPIC -shared -o $(LIB_DIR)/MOGBlob.so \
		$(TRACKER_DIR)/MOGBlob.cpp $(CC_FLAGS)

game: $(GAME_DIR)/game.py
	rm -f game.py
	ln -s $(GAME_DIR)/game.py

clean:
	rm -f game.py game.pyc $(LIB_DIR)/* MOGBlob

