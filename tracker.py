#! /usr/bin/python

import sys

from opencv.cv import *
from opencv.highgui import *
import pdb

from FPSLogger import *

#Mouse Callbacks

printcolor = True;

def HSVMouseCallback(event, x, y, flags, hsv_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = hsv_frame[y][x];
        print "HSV: " + str(color);
def BGRMouseCallback(event, x, y, flags, rgb_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = rgb_frame[y][x];
        print "BGR: " + str(color);

#Main

if __name__ == '__main__':

    print "Initializing the Tracker";

    #Initialize
    WHITE = cvScalar(255,255,255,0);
    SIZE = cvSize(640, 480);

    # Capture webcam
    try:
        # try to get the device number from the command line
        device = int (sys.argv [1])

        # got it ! so remove it from the arguments
        del sys.argv [1]
    except (IndexError, ValueError):
        # no device number on the command line, assume we want the 1st device
        device = -1

    # Access to the webcam stream
    capture = cvCreateCameraCapture(device);

    if not capture:
        print "Could not open webcam";
        exit(1);
    
    # Create windows
    cvNamedWindow("Webcam", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("HSV_Webcam", CV_WINDOW_AUTOSIZE);

    # Grab buffer of original image
    frame = cvQueryFrame(capture);
    # Create buffer for the hsv image
    hsv_frame = cvCreateImage(SIZE, IPL_DEPTH_8U, 3);

    # Set up mouse callbacks
    cvSetMouseCallback("Webcam", BGRMouseCallback, frame);
    cvSetMouseCallback("HSV_Webcam", HSVMouseCallback, hsv_frame);

    print "Successfully Initialized the Tracker";

    #Now to do the fun stuff!
    fps = FPSLogger();
    while 1:
        #Query and show frame
        frame = cvQueryFrame(capture);
        if frame == None:
            print "Yikes. Frame is None. Aborting"
            exit(1);

        #Convert BGR -> HSV
        cvCvtColor(frame, hsv_frame, CV_BGR2HSV);

        #Show images.
        #WaitKey magically flushes buffer? Dunno why.
        cvShowImage("Webcam", frame);
        cvShowImage("HSV_Webcam", hsv_frame);
        cvWaitKey(1);

        #Measure rate
        #fps.printFPS();

