import time

class FPSLogger:
    def __init__(self):
        self.numframes = 0;
        self.t0 = time.time();

    def measureFPS(self):
        #Measure FPS and print it.
        self.numframes += 1;
        t = time.time();
        rate = self.numframes / (t - self.t0);
        if (self.numframes % 100 == 0):
            self.numframes = 0;
            self.t0 = time.time();
        return rate;

    def printFPS(self):
        rate = self.measureFPS();
        print "FPS = %f" % rate;
        return rate

