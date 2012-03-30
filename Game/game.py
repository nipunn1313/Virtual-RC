#!/usr/bin/python
#use python -i for interactive

import pygame;
import sys;
from pygame.locals import *;
from affine import Affine_Fit

title = 'Virtual RC'
display_dims = (1200,800);
track_img_fn = 'racetrack.png'
calibrate_img_fn = 'arrow.png'

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

    # Load image
    arrow = pygame.image.load(calibrate_img_fn);

    # Get user to click
    for to_pt in to_pts:
        copy = surface.copy();
        copy.blit(arrow, to_pt);

        screen = pygame.display.get_surface();
        screen.blit(copy, (0,0));
        
        pygame.event.clear()
        pygame.display.flip();
        event = pygame.event.wait();

        from_pts.append(event.pos);

    trn = Affine_Fit(from_pts, to_pts);
    return trn;

if __name__ == '__main__':
    pygame.init();
    window = pygame.display.set_mode(display_dims);
    pygame.display.set_caption(title);

    screen = pygame.display.get_surface();
    race_surf = pygame.image.load(track_img_fn);

    pygame.event.set_allowed(None);
    pygame.event.set_allowed([MOUSEBUTTONDOWN, QUIT]);

    #Do Calibration
    trnFn = calibrate(race_surf);
    print "Transformation is:"
    print trnFn.To_Str()

    #Draw to screen and show changes
    screen.blit(race_surf, (0,0));
    pygame.display.flip();

    while True:
        events = pygame.event.get();
        for event in events:
            print event
            if event.type == QUIT:
                pygame.display.quit();
                sys.exit(0);

            if event.type == MOUSEBUTTONDOWN:
                pos = event.pos;
                (tx,ty) = trnFn.Transform(pos);
                trnpos = (int(tx), int(ty));

                if inScreen(trnpos):
                    color = screen.get_at(trnpos);
                else:
                    color = 'Not in screen'

                print ('Pos=%s TrnPos=%s Color=%s' %
                       (pos, trnpos, color));

