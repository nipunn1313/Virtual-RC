#!/usr/bin/python

from opencv.cv import *
from opencv.highgui import *
from threading import Thread
import time

capture = cvCreateCameraCapture(-1);
if not capture:
    print "Could not open webcam";
    exit(1);

cvNamedWindow("Webcam", CV_WINDOW_AUTOSIZE);


t0 = time.time();
numframes = 0;
while 1:
    #Query and show frame
    frame = cvQueryFrame(capture);
    cvShowImage("Webcam", frame);
    cvWaitKey(1);

    #Measure FPS and print it.
    numframes += 1;
    t = time.time();
    rate = numframes / (t - t0);
    print ("FPS = %f" % (rate));
    if (numframes % 100 == 0):
        numframes = 0;
        t0 = time.time();

