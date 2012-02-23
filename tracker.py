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

#Open Camera!
capture = cvCreateCameraCapture(-1);
if not capture:
    print "Could not open webcam";
    exit(1);
frame = cvQueryFrame(capture);

#Allocate!
white = cvScalar(255,255,255,0);
blocksize = (7,7);
size = cvGetDims(frame);
size = (size[1], size[0]); #It's backwards. Dunno why.
#BitDepth && number of channesl
hsv = cvCreateImage(size, IPL_DEPTH_8U, 3);

velx = cvCreateImage(size, IPL_DEPTH_32F, 1);
vely = cvCreateImage(size, IPL_DEPTH_32F, 1);

#Make Window!
camWindow = "Webcam"
flowWindow = "Flow"
grayWindow = "Gray Image"
cvNamedWindow(camWindow, CV_WINDOW_AUTOSIZE);
cvNamedWindow(flowWindow, CV_WINDOW_AUTOSIZE);
cvNamedWindow(grayWindow, CV_WINDOW_AUTOSIZE);

#Customize Window!
#cvSetMouseCallback(camWindow, BGRMouseCallback, frame);

print "Successfully Initialized the Tracker";

#Now to do the fun stuff!
fps = FPSLogger();
image = None
while 1:
    #Query and show frame
    frame = cvQueryFrame(capture);
    if frame == None:
        print "Yikes. Frame is None. Aborting"
        exit(1);

    #Convert BGR -> HSV
    #cvCvtColor(frame, hsv_frame, CV_BGR2HSV);

    if image is None:
        image = cvCreateImage(size, IPL_DEPTH_8U, 3);
        gray = cvCreateImage(size, IPL_DEPTH_8U, 1);
        gray_prev = cvCreateImage(size, IPL_DEPTH_8U, 1);
        pyramid = cvCreateImage(size, IPL_DEPTH_8U, 1);
        prev_pyramid = cvCreateImage(size, IPL_DEPTH_8U, 1);
        eig = cvCreateImage(size, IPL_DEPTH_32F, 1);
        temp = cvCreateImage(size, IPL_DEPTH_32F, 1);
        points = [[],[]]

    cvCopy(frame, image)

    gray_prev = gray
    cvCvtColor(image, gray, CV_BGR2GRAY);

    #Do optical flow calculation
    cvCalcOpticalFlowLK(gray_prev, gray, blocksize, velx, vely);

    scribble = cvCreateImage(size, IPL_DEPTH_8U, 3)
    cvCvtColor(gray, scribble, cv.CV_GRAY2BGR)
    for y in range(0, size[1], 8):
        for x in range(0, size[0], 8):
            cvLine(scribble, (x, y), (int(x+velx[y,x]), int(y + vely[y,x])), (0,255,0))

    #Show images. 
    #WaitKey magically flushes buffer? Dunno why.
    cvShowImage(camWindow, frame);
    cvShowImage(grayWindow, gray);
    cvShowImage(flowWindow, scribble);

    cvWaitKey(1);

    #Measure rate
    #fps.printFPS();

