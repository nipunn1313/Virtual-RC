#!/usr/bin/python

from opencv.cv import *
from opencv.highgui import *
import pdb

from FPSLogger import *

printcolor = True;
def HSVMouseCallback(event, x, y, flags, hsv_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = hsv_frame[y][x];
        print "HSV: " + str(color);
def BGRMouseCallback(event, x, y, flags, rgb_frame):
    if (printcolor and event == CV_EVENT_MOUSEMOVE):
        color = rgb_frame[y][x];
        print "BGR: " + str(color);


print "Initializing the Tracker";

#Initialize
white = cvScalar(255,255,255,0);
size = cvSize(640, 480);
hsv_frame = cvCreateImage(size, IPL_DEPTH_8U, 3);

capture = cvCreateCameraCapture(-1);
if not capture:
    print "Could not open webcam";
    exit(1);
frame = cvQueryFrame(capture);

cvNamedWindow("Webcam", CV_WINDOW_AUTOSIZE);
cvNamedWindow("HSV_Webcam", CV_WINDOW_AUTOSIZE);

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

