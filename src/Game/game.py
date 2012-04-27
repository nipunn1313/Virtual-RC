#!/usr/bin/python
#use python -i for interactive

import pygame;
from pygame.locals import *;

import sys;
import os;
import game_logic;

sys.path.append(os.getcwd() + '/lib');
sys.path.append(os.getcwd() + '/../../lib');
import MOGBlob;

# These are the imports necessary for sending speed to the cars
import xbee;
import serial;
from speedsender import SpeedSender

camsize = (640,480);

use_wired_instead_of_xbee = False

def get_click_pos():
    MOGBlob.ask_for_click();
    loc = MOGBlob.get_click_loc();

    while not loc:
        pygame.display.flip();
        loc = MOGBlob.get_click_loc();
    return loc;


if __name__ == '__main__':
	# Initialize the speed sender thread for car1, with a 1.0 second interval
    if (use_wired_instead_of_xbee):
        car1 = SpeedSender.forSerial(1.000, 0)
        car2 = SpeedSender.forSerial(1.000, 1)
    else:
        xbeeSender = xbee.XBee(serial.Serial('/dev/ttyUSB0', 9600))
        car1 = SpeedSender.forXBee(1.000, xbeeSender, SpeedSender.DEST2)
        car2 = SpeedSender.forXBee(1.000, xbeeSender, SpeedSender.DEST3)

    cars = [car1, car2]
    for car in cars:
        car.start();

    def setCarSpeed(carnum, speed):
        cars[carnum].changeSpeed(speed);

    # Initialize MOGBlob code
    MOGBlob.init_tracker();

    # Initialize game logic for 2 cars
    game = game_logic.GameLogic();
    game.setClickPosFunc(get_click_pos);
    game.setCarSpeedFunc(setCarSpeed);
    game.setCarLocFunc(MOGBlob.get_car_loc);
    game.setDestroyFunc(MOGBlob.destroy_tracker);

    # Do Coordinate Calibration
    trnFn = game.coord_calibrate();
    if not trnFn:
        print "Calibration Transformation failed!"
        sys.exit();

    print "Transformation is:"
    print trnFn.To_Str()

    MOGBlob.suppress_display();

    game.main_loop();

