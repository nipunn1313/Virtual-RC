# ! /usr/bin/python

import sys

from opencv.cv import *
from opencv.highgui import *
import pdb

from FPSLogger import *

# Mouse Callbacks

printcolor = True;

def HSVMouseCallback(event, x, y, flags, hsv_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = hsv_frame[y][x];
        print "HSV: " + str(color);
def BGRMouseCallback(event, x, y, flags, rgb_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = rgb_frame[y][x];
        print "BGR: " + str(color);

# Main

if __name__ == '__main__':

    print "Initializing the Tracker";

    # Set device number to param or default
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

    # Grab first frame so we can get its size and initialize other frame
    # variables
    frame = cvQueryFrame(capture)
    if frame == None:
        print "Yikes. Frame is None. Aborting"
        exit(1);
    
    # Allocate!
    white = cvScalar(255,255,255,0);
    size = cvGetDims(frame);
    size = (size[1], size[0]); # It's backwards. Dunno why.

    # BitDepth && number of channels
    prev_frame = cvCreateImage(size, IPL_DEPTH_8U, 3);
    diff_frame = cvCreateImage(size, IPL_DEPTH_8U, 3);
    
    # Make Windows!
    camWindow = "Raw Input"
    diffWindow = "Diff"
    cvNamedWindow(camWindow, CV_WINDOW_AUTOSIZE);
    cvNamedWindow(diffWindow, CV_WINDOW_AUTOSIZE);
    
    # Customize Window!
    # cvSetMouseCallback(camWindow, BGRMouseCallback, frame);
    
    print "Successfully Initialized the Tracker";
    
    # Now to do the fun stuff!
    # fps = FPSLogger();
    while 1:
        # Update prev_frame
        cvCopy(frame, prev_frame);

        # Query for current frame and show frame
        frame = cvQueryFrame(capture);
        if frame == None:
            print "Yikes. Frame is None. Aborting"
            exit(1);
    
        # Generate the diff frame 
        cvSub(frame, prev_frame, diff_frame);

        # Print images in the windows
        cvShowImage(camWindow, frame);
        cvShowImage(diffWindow, diff_frame);

        # WaitKey magically flushes buffer? Dunno why.
        cvWaitKey(1);
    
        # Measure rate
        # fps.printFPS();
    
