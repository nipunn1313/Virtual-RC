#!/usr/bin/python
#use python -i for interactive

import pygame;
from pygame.locals import *;

import sys;
import os;
from affine import Affine_Fit;

sys.path.append(os.getcwd() + '/lib');
import MOGBlob;

# These are the imports necessary for sending speed to the cars
import xbee;
import serial;
from speedsender import SpeedSender

title = 'Virtual RC'
display_dims = (900,600);
camsize = (640,480);
img_dir = 'Images/'
track_img_fn = img_dir + 'racetrack.png'
calibrate_img_fn = img_dir + 'arrow.png'

brown = (45,  4,   4,   255)
grey  = (102, 106, 102, 255)
black = (0,   0,   0,   255)

def inScreen(pos):
    (x,y) = pos;
    (xd,yd) = display_dims;
    return ((0 <= x < xd) and (0 <= y < yd))

def calibrate(surface):
    (width, height) = display_dims;
    # Points in the four corners
    dx = 50;
    to_pts = ([(dx,dx), (width-dx, dx), (dx, height-dx),
                (width-dx, height-dx)]);
    from_pts = [];

    # Load arrow image
    arrow = pygame.image.load(calibrate_img_fn);

    # Get user to click
    for to_pt in to_pts:
        copy = surface.copy();
        copy.blit(arrow, to_pt);

        screen = pygame.display.get_surface();
        screen.blit(copy, (0,0));
        pygame.display.flip();

        MOGBlob.ask_for_click();
        loc = MOGBlob.get_click_loc();

        while not loc:
            pygame.display.flip();
            loc = MOGBlob.get_click_loc();

        from_pts.append(loc);

    trn = Affine_Fit(from_pts, to_pts);
    return trn;

if __name__ == '__main__':
    pygame.init();

    window = pygame.display.set_mode(display_dims);
    pygame.display.set_caption(title);

    screen = pygame.display.get_surface();
    race_surf_full = pygame.image.load(track_img_fn);
    race_surf = pygame.transform.scale(race_surf_full, display_dims);

    # Enable mousebuttondown event. Need to disable all others before enabling
    # it to prevent other events from firing.
#pygame.event.set_allowed(None);
#pygame.event.set_allowed([MOUSEBUTTONDOWN, QUIT]);

	# Initialize the speed sender thread for car1, with a 1.0 second interval
    xbeeSender = xbee.XBee(serial.Serial('/dev/ttyUSB0', 9600))
    car1 = SpeedSender.forXBee(1.000, xbeeSender, SpeedSender.DEST2)
    car1.start()

    # Initialize MOGBlob code
    MOGBlob.init_tracker();

    # Do Calibration
    trnFn = calibrate(race_surf);
    if not trnFn:
        print "Calibration Transformation failed!"
        sys.exit();

    print "Transformation is:"
    print trnFn.To_Str()

    #Draw to screen and show changes
    screen.blit(race_surf, (0,0));
    pygame.display.flip();

    MOGBlob.ask_for_click();
    while True:
        # Quit on quit events
        events = pygame.event.get();
        for event in events:
            print event
            if event.type == QUIT:
                pygame.display.quit();
                sys.exit(0);

        pos = MOGBlob.get_curr_loc();
        # On click, get curr loc and display!
        clickpos = MOGBlob.get_click_loc();
        if clickpos and pos:
            (tx,ty) = trnFn.Transform(pos);
            trnpos = (int(tx), int(ty));
            if inScreen(trnpos):
                color = screen.get_at(trnpos);
            else:
                color = 'Not in screen'
            print ('CarPos=%s CarTrnPos=%s Color=%s' %
                   (pos, trnpos, color));
            # Below is a prototype for how the car's speed can be changed
            # in response to what color it's on
            if color == grey:
                car1.changeSpeed(SpeedSender.NORM)
            elif color == brown:
                car1.changeSpeed(SpeedSender.SLOW)
            elif color == black:
                car1.changeSpeed(SpeedSender.NORM)
            else:
                car1.changeSpeed(SpeedSender.STOP)

            # Ask for more!
            MOGBlob.ask_for_click();

