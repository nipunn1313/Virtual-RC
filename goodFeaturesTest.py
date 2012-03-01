# ! /usr/bin/python

import sys

import cv
#from opencv.cv import *
#from opencv.highgui import *
import pdb

from FPSLogger import *

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
    capture = cv.CreateCameraCapture(device);

    if not capture:
        print "Could not open webcam";
        exit(1);

    # Grab first frame so we can get its size and initialize other frame
    # variables
    frame = cv.QueryFrame(capture)
    if frame == None:
        print "Yikes. Frame is None. Aborting"
        exit(1);
    
    # Allocate!
    size = cv.GetDims(frame);
    size = (size[1], size[0]); # It's backwards. Dunno why.

    # BitDepth && number of channels
    features_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 3);
    gray_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 1);

    # Create temporary matrices for GoodFeaturesToTrack()
    eig_image = cv.CreateMat(size[0], size[1], cv.CV_8UC1)
    temp_image = cv.CreateMat(size[0], size[1], cv.CV_8UC1)
    
    # Make Windows!
    camWindow = "Raw Input"
    featuresWindow = "Good Features"
    cv.NamedWindow(camWindow, cv.CV_WINDOW_AUTOSIZE);
    cv.NamedWindow(featuresWindow, cv.CV_WINDOW_AUTOSIZE);
    
    # Customize Window!
    # cvSetMouseCallback(camWindow, BGRMouseCallback, frame);
    
    print "Successfully Initialized the Tracker";
    
    # Now to do the fun stuff!
    # fps = FPSLogger();
    while 1:
        # Query for current frame and show frame
        frame = cv.QueryFrame(capture);
        if frame == None:
            print "Yikes. Frame is None. Aborting"
            exit(1);

        cv.CvtColor(frame, gray_frame, cv.CV_BGR2GRAY);

        cv.Copy(frame, features_frame);

        # Generate the features frame
        for x,y in cv.GoodFeaturesToTrack(gray_frame, eig_image, temp_image,
                50, 0.01, 10, None, 3, 0):
            
            cv.Circle(features_frame, (int(x), int(y)), 3, 
                    cv.Scalar(0,50,255,0), -1, 8, 0);

        # Print images in the windows
        cv.ShowImage(camWindow, frame);
        cv.ShowImage(featuresWindow, features_frame);

        # WaitKey magically flushes buffer? Dunno why.
        cv.WaitKey(1);
    
        # Measure rate
        # fps.printFPS();
    
