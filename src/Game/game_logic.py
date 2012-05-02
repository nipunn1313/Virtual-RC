#!/usr/bin/python

import random
import sys
import time

import pygame;
from pygame.locals import *;
from affine import Affine_Fit;
import speeds

title = 'Virtual RC'
display_dims = (1170,780);
item_dims = (40,40);
item_buf = 45;
img_dir = 'Images/'
track_img_fn = img_dir + 'fattrack.png'
calibrate_img_fn = img_dir + 'arrow.png'
mushroom_img_fn = img_dir + 'mushroom.bmp'

brown = (45,  4,   4,   255)
grey  = (102, 106, 102, 255)
black = (0,   0,   0,   255)

car_radius = 20

def inScreen(pos):
    (x,y) = pos;
    (xd,yd) = display_dims;
    return ((0 <= x < xd) and (0 <= y < yd))

class Item():
    def __init__(self, img_fn):
        surf = pygame.image.load(img_fn);
        self.img_surf = pygame.transform.scale(surf, item_dims);
        self.img_surf.set_colorkey((255,255,255));
        self.on_map = 0;
        self.affecting = -1;

    def appear_randomly(self, game):
        if not self.on_map:
            (width, height) = display_dims;
            pos = (item_buf,item_buf);
            while (game.screen.get_at(pos) != grey or
                   game.screen.get_at((pos[0]+item_buf, pos[1]+item_buf)) != grey or
                   game.screen.get_at((pos[0]+item_buf, pos[1]-item_buf)) != grey or
                   game.screen.get_at((pos[0]-item_buf, pos[1]+item_buf)) != grey or
                   game.screen.get_at((pos[0]-item_buf, pos[1]-item_buf)) != grey
                  ):
                pos = (random.randint(item_buf,width-item_buf-1),
                        random.randint(item_buf,height-item_buf-1))

            # Create rect. Center it
            self.rect = pygame.Rect(pos, item_dims);
            topleft = self.rect.topleft
            self.rect.center = topleft;

            self.on_map = 1;

    def blit(self, game):
        game.draw(self.img_surf, self.rect.topleft);

    # Effects of carnum obtaining this item. Should be overridden
    def apply_effects(self, game):
        pass;
    def effect_on(self, carnum):
        pass;

    def update(self, game, carRects):
        if not self.on_map:
            self.appear_randomly(game);

        for carnum in range(2):
            carrect = carRects[carnum];
            if carrect and self.rect.colliderect(carrect):
                self.affecting = carnum;
                self.apply_effects(game);
                self.on_map = 0;
            else:
                self.blit(game);

class Mushroom(Item):
    def __init__(self):
        Item.__init__(self, mushroom_img_fn);

    def apply_effects(self, game):
        game.itemAffectingCar[self.affecting] = self;

    def effect_on(self, carnum):
        if carnum == self.affecting:
            return speeds.FAST;
        else:
            return None


class GameLogic:
    def __init__(self):
        # Setup pygame and screen
        pygame.init();
        self.window = pygame.display.set_mode(display_dims);
        pygame.display.set_caption(title);
        self.screen = pygame.display.get_surface();

        # Load race surface
        race_surf_full = pygame.image.load(track_img_fn);
        self.race_surf = pygame.transform.scale(race_surf_full, 
                display_dims);

        # Display race surface
        self.draw();
        pygame.display.flip();

        # Disable most events for performance
        pygame.event.set_allowed(None);
        pygame.event.set_allowed([QUIT]);

        # Default coordinate transform is identity
        self.transformFunc = lambda x:x;
        self.destroyFunc = lambda :0

    def setClickPosFunc(self, func):
        self.clickPosFunc = func;

    def setCarSpeedFunc(self, func):
        self.carSpeedFunc = func;

    def setCarLocFunc(self, func):
        self.carLocFunc = func;

    def setDestroyFunc(self, func):
        self.destroyFunc = func;
    
    def coord_calibrate(self):
        (width, height) = display_dims;
        # Points in the four corners
        dx = 200;
        to_pts = ([(dx,dx), (width-dx, dx), (dx, height-dx),
                    (width-dx, height-dx)]);
        from_pts = [];
    
        # Load arrow image
        arrow = pygame.image.load(calibrate_img_fn);
    
        # Get user to click
        for to_pt in to_pts:
            copy = self.race_surf.copy();
            copy.blit(arrow, to_pt);
    
            self.draw(copy, (0,0));
            pygame.display.flip();

            loc = self.clickPosFunc();
            from_pts.append(loc);
    
        trnFn = Affine_Fit(from_pts, to_pts);
        self.transformFunc = trnFn.Transform;
        return trnFn;

    def draw(self, surface=None, coord=(0,0)):
        if surface == None:
            surface = self.race_surf;
        self.screen.blit(surface, coord);

    def check_events(self):
        # Quit on quit events
        events = pygame.event.get([QUIT]);
        for event in events:
            #print event
            if event.type == QUIT:
                print "QUITTTTIN"
                pygame.display.quit();
                self.destroyFunc();
                sys.exit(0);

    def main_loop(self):
        font_fn = pygame.font.get_default_font();
        font = pygame.font.Font(font_fn, 48);
        clock = pygame.time.Clock();
        clock.tick();
        millis = 0.0;

        self.draw();

        speedmap = {
            grey:speeds.NORM,
            brown:speeds.SLOW,
            black:speeds.NORM
        };

        items = [ Mushroom(), Mushroom() ];

        # Positions of the cars (state)
        carposs = [None, None];
        carspeeds = [None, None];
        # Each car obtained nothing
        self.itemAffectingCar = [None, None];

        while True:
            self.check_events();

            # Reset screen
            self.draw()
    
            for carnum in range(2):
                pos = self.carLocFunc(carnum);
                if pos:
                    (tx,ty) = self.transformFunc(pos);
                    trnpos = (int(tx), int(ty));
                    if inScreen(trnpos):
                        c = self.screen.get_at(trnpos);
                        color = (c.r, c.g, c.b, c.a)
                        carposs[carnum] = trnpos;
                    else:
                        color = 'Not in screen'
                        carposs[carnum] = None;

                    newspeed = speeds.STOP;
                    if color in speedmap:
                        newspeed = speedmap[color];

                    # This is where we stop effects of items
                    if newspeed == speeds.SLOW or newspeed == speeds.STOP:
                        self.itemAffectingCar[carnum] = None;
                    elif self.itemAffectingCar[carnum]:
                        effect = (self.itemAffectingCar[carnum].
                                    effect_on(carnum));
                        if effect:
                            newspeed = effect
                else:
                    newspeed = speeds.STOP
                    carposs[carnum] = None;

                old = carspeeds[carnum]
                carspeeds[carnum] = newspeed;
                if (old != newspeed):
                    print "Carspeeds=",carspeeds;
                self.carSpeedFunc(carnum, newspeed)

                #print ('Car=%d CarPos=%s CarTrnPos=%s Color=%s Speed=%d' %
                       #(carnum, pos, trnpos, color, newspeed));

            # Blit stuff on top of screen

            # Draw circles where we think cars are
            carRects = [None, None];
            if carposs[0]:
                carRects[0] = (pygame.draw.circle(self.screen, 
                            (255,255,255), carposs[0], car_radius));
            else:
                pygame.draw.circle(self.screen, (255,255,255), (30, 30));
            if carposs[1]:
                carRects[1] = (pygame.draw.circle(self.screen, 
                            (0,0,0), carposs[1], car_radius));
            else:
                pygame.draw.circle(self.screen, (0,0,0), (60, 30));

            # Items do their thing based on cars' locations
            for item in items:
                item.update(self, carRects);

            # Blit clock in center
            millis = millis + clock.tick();
            timemin = int(millis) / (60000);
            timesec = (int(millis) / (1000)) % 60;
            timehund = (int(millis) % 1000) / 10;
            timestr = '%02d:%02d:%02d' % (timemin, timesec, timehund);

            clocksurf = font.render(timestr, True, (255,255,255), brown);
            clocksize = clocksurf.get_size();
            self.draw(clocksurf, ((display_dims[0]-200)/2, 
                        (display_dims[1]-50)/2));

            pygame.display.flip();

if __name__ == '__main__':

    dx = 200;
    (width, height) = display_dims;
    to_pts = ([(dx,dx), (width-dx, dx), (dx, height-dx),
                (width-dx, height-dx)]);
    def getClickPos():
        global to_pts
        return to_pts.pop(0);

    carspeeds = [speeds.STOP, speeds.STOP];
    def setCarSpeed(carnum, speed):
        global carspeeds
        old = carspeeds[carnum];
        carspeeds[carnum] = speed;
        if old != speed:
            print "Speeds=", carspeeds

    carlocs = [(0,0), (0,0)];
    def getCarLoc(carnum):
        global carlocs
        events = pygame.event.get([MOUSEBUTTONDOWN]);
        if events:
            if events[0].button == 1:
                carlocs[0] = events[0].pos;
            else:
                carlocs[1] = events[0].pos;
            print "Loc=",carlocs
        return carlocs[carnum];

    game = GameLogic();
    game.setClickPosFunc(getClickPos);
    game.setCarSpeedFunc(setCarSpeed);
    game.setCarLocFunc(getCarLoc);

    pygame.event.set_allowed([QUIT, MOUSEBUTTONDOWN]);

    game.main_loop();

