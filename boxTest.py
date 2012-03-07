# ! /usr/bin/python

import sys

import cv 
#from opencv.cv import *
#from opencv.highgui import *
import pdb

from FPSLogger import *

printcolor = True;
def HSVMouseCallback(event, x, y, flags, hsv_frame):
    if (printcolor and event == cv.CV_EVENT_MOUSEMOVE):
        color = cv.Get2D(hsv_frame, y, x);
        print "HSV: " + str(color);
def BGRMouseCallback(event, x, y, flags, rgb_frame):
    if (printcolor and event == cv.CV_EVENT_MOUSEMOVE):
        color = rgb_frame[y][x];
        print "BGR: " + str(color);
    
# Defines for our library routines (TODO: Move to separate file)
BOX_WIDTH = 150;
BOX_HEIGHT = 150;
CAR_RADIUS = 22;
box = cv.CreateImage((BOX_WIDTH, BOX_HEIGHT), cv.IPL_DEPTH_8U, 1);

# Matrices for GoodFeaturesToTrack
eig_mat_box = cv.CreateMat(BOX_WIDTH, BOX_HEIGHT, cv.CV_8UC1)
temp_mat_box = cv.CreateMat(BOX_WIDTH, BOX_HEIGHT, cv.CV_8UC1)
eig_mat = cv.CreateMat(BOX_WIDTH, BOX_HEIGHT, cv.CV_8UC1)
temp_mat = cv.CreateMat(BOX_WIDTH, BOX_HEIGHT, cv.CV_8UC1)

def CalcCoordStatically(frame):
    features = cv.GoodFeaturesToTrack(frame, eig_mat, temp_mat, 50, 0.01,
            10, None, 3, 0);

    max_util = 0;
    best = (0,0);
    for feature in features:
        util = GetUtilOfFeature(feature, features);
        if util > max_util:
            max_util = util;
            best = feature;

    return best;


def CalcCoordFromPrev(frame, prev_pos):
    # Check validity of prev_pos. Use CalcCoordStatically() if invalid
    size = cv.GetSize(frame);

    if (prev_pos[0] < 0 or prev_pos[1] < 0 or prev_pos[0] >= size[0] or
        prev_pos[1] >= size[1]):
        return (CalcCoordStatically(frame), box);

    # Slice the image to the box around the previous position
    prev_box = SliceFrameFromPrev(frame, prev_pos); 

    features = cv.GoodFeaturesToTrack(prev_box, eig_mat_box, temp_mat_box, 50, 0.01, 
            10, None, 3, 0);

    max_util = 0;
    best = (0,0);
    for feature in features:
        util = GetUtilOfFeature(feature, features);
        if util > max_util:
            max_util = util;
            best = feature;

    # Convert coordinates back to ones on original image
    # Relative -> Absolute
    best = (best[0] - 0.5*BOX_WIDTH + prev_pos[0],
            best[1] - 0.5*BOX_HEIGHT + prev_pos[1]);

    return (best, prev_box);

def SliceFrameFromPrev(frame, prev_pos):
    cv.GetRectSubPix(frame, box, prev_pos);
    return box;

def GetUtilOfFeature(feature, features):
    (x,y) = feature;
    util = 0;
    for x2,y2 in features:
        xdiff = (x-x2);
        ydiff = (y-y2);
        
        # Feature is within CAR_WIDTH distance from the feature we're checking
        if (xdiff*xdiff + ydiff*ydiff) <= CAR_RADIUS*CAR_RADIUS:
            util = util + 1;
            
#        util += xdiff*xdiff + ydiff*ydiff;

    return util;

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
    size = cv.GetSize(frame);

    # Create placeholder image matrices
    hsv_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 3);
    smooth_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 3);
    green_filter_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 1);
    red1_filter_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 1);
    red2_filter_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 1);
    mario_filter_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 1);
    luigi_features_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 3);
    mario_features_frame = cv.CreateImage(size, cv.IPL_DEPTH_8U, 3);

    # Matrices and images for GoodFeaturesToTrack()
    #eig_mat = cv.CreateMat(size[0], size[1], cv.CV_8UC1)
    #temp_mat = cv.CreateMat(size[0], size[1], cv.CV_8UC1)
    
    # Make Windows!
    camWindow = "Raw Input"
    filterWindow = "Green Filter"
    luigiWindow = "Luigi"
    marioWindow = "Mario"
    boxWindow = "Prev Box"
    cv.NamedWindow(camWindow, cv.CV_WINDOW_AUTOSIZE);
    cv.NamedWindow(filterWindow, cv.CV_WINDOW_AUTOSIZE);
    cv.NamedWindow(luigiWindow, cv.CV_WINDOW_AUTOSIZE);
    #cv.NamedWindow(marioWindow, cv.CV_WINDOW_AUTOSIZE);
    cv.NamedWindow(boxWindow, cv.CV_WINDOW_AUTOSIZE);
    
    # Callback to figure out HSV hues
    cv.SetMouseCallback(camWindow, HSVMouseCallback, hsv_frame);

    # Initial position of the car (default to center of frame)
    #prev_pos = (size[0]/2, size[1]/2);
    prev_pos = (320.0, 240.0);

    print "Successfully Initialized the Tracker";
    
    # Now to do the fun stuff!
    # fps = FPSLogger();
    while 1:
        # Query for current frame and show frame
        frame = cv.QueryFrame(capture);
        if frame == None:
            print "Yikes. Frame is None. Aborting"
            exit(1);

        # Blur colors for easier filtering
        cv.Smooth(frame, smooth_frame, cv.CV_BLUR, 3);

        # Convert to HSV
        cv.CvtColor(smooth_frame, hsv_frame, cv.CV_BGR2HSV);

        # Filter Luigi by green
        cv.InRangeS(hsv_frame, (52, 30, 30), (79, 160, 140),
                            green_filter_frame);

        # Filter Mario by red (0-10, 170-180)
        cv.InRangeS(hsv_frame, (0, 180, 90), (10, 245, 180),
                            red1_filter_frame);
        cv.InRangeS(hsv_frame, (170, 190, 110), (180, 245, 180),
                            red2_filter_frame);
        cv.Add(red1_filter_frame, red2_filter_frame, mario_filter_frame);

        cv.Copy(frame, luigi_features_frame);
        cv.Copy(frame, mario_features_frame);

        # Try GoodFeatures...() on the filtered images
#        for x,y in cv.GoodFeaturesToTrack(green_filter_frame, eig_mat, 
#                temp_mat, 50, 0.01, 10, None, 3, 0):
#
#            # Draw dots for features
#            cv.Circle(luigi_features_frame, (int(x), int(y)), 3,
#                    cv.Scalar(0, 50, 255, 0), -1, 8, 0);
#
#        for x,y in cv.GoodFeaturesToTrack(mario_filter_frame, eig_mat, 
#                temp_mat, 50, 0.01, 10, None, 3, 0):
#
#            # Draw dots for features
#            cv.Circle(mario_features_frame, (int(x), int(y)), 3,
#                    cv.Scalar(0, 50, 255, 0), -1, 8, 0);

        # Draw prev_pos
        (prev_pos, prev_box) = CalcCoordFromPrev(green_filter_frame, prev_pos);

        int_pos = (int(prev_pos[0]), int(prev_pos[1]));
        cv.Circle(luigi_features_frame, int_pos, 30,
                cv.Scalar(0, 0, 0, 0), 2, 8, 0);

        #print prev_pos

        # Print images in the windows
        cv.ShowImage(camWindow, frame);
        cv.ShowImage(filterWindow, green_filter_frame);
        cv.ShowImage(luigiWindow, luigi_features_frame);
        #cv.ShowImage(marioWindow, mario_features_frame);
        cv.ShowImage(boxWindow, prev_box);

        # WaitKey magically flushes buffer? Dunno why.
        cv.WaitKey(1);
    
        # Measure rate
        # fps.printFPS();
