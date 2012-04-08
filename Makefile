CC=g++ -g -pg
CC_FLAGS=`pkg-config opencv --cflags --libs` -lblob \
		 -I/usr/include/python2.7 -lboost_python -lpython2.7

TRACKER_DIR=src/Tracker
GAME_DIR=src/Game
LIB_DIR=lib

all: tracker game

tracker: $(TRACKER_DIR)/MOGBlob.cpp
	$(CC) -o MOGBlob $(TRACKER_DIR)/MOGBlob.cpp $(CC_FLAGS)
	$(CC) -fPIC -shared -o $(LIB_DIR)/MOGBlob.so \
		$(TRACKER_DIR)/MOGBlob.cpp $(CC_FLAGS)

game: $(GAME_DIR)/game.py
	rm -f game.py
	ln -s $(GAME_DIR)/game.py

clean:
	rm -f game.py game.pyc $(LIB_DIR)/* MOGBlob

