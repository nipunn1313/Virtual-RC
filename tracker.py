#!/usr/bin/python

from opencv.cv import *
from opencv.highgui import *
from threading import Thread
import time
import pdb

class FPS_Logger:
    def __init__(self):
        self.numframes = 0;
        self.t0 = time.time();
    def measure_FPS(self):
        #Measure FPS and print it.
        self.numframes += 1;
        t = time.time();
        rate = self.numframes / (t - self.t0);
        if (self.numframes % 100 == 0):
            self.numframes = 0;
            self.t0 = time.time();
#print "FPS = %f" % rate;
        return rate;

class TrackableBall:
    def __init__(self, HSV_L, HSV_H, color, name): 
        self.hsv_min = cvScalar(*HSV_L);
        self.hsv_min2 = cvScalar(*HSV_L);
        self.hsv_max = cvScalar(*HSV_H);
        self.hsv_max2 = cvScalar(*HSV_H);
        #Wraparound if necessary!
        if HSV_H[0] < HSV_L[0]:
            self.hsv_max[0] = 256;
            self.hsv_min2[0] = 0;
        self.cvColor = cvScalar(*color);
        self.thresholded = cvCreateImage(size, IPL_DEPTH_8U, 1);
        self.thresholded2 = cvCreateImage(size, IPL_DEPTH_8U, 1);
        self.storage = cvCreateMemStorage(0);
        self.name = name;
        cvNamedWindow(name, CV_WINDOW_AUTOSIZE);

    def track(self, hsv_frame):
        #Limit HSV range
        (cvInRangeS(hsv_frame, self.hsv_min, 
                    self.hsv_max, self.thresholded));
        (cvInRangeS(hsv_frame, self.hsv_min2, 
                    self.hsv_max2, self.thresholded2));
        (cvOr(self.thresholded, self.thresholded2, 
              self.thresholded));
        (cvSmooth(self.thresholded, self.thresholded, 
                  CV_GAUSSIAN, 9, 9));
        circles = (cvHoughCircles(self.thresholded, 
                    self.storage, CV_HOUGH_GRADIENT, 2, 
                    self.thresholded.height/4, 
                    100, 40, 20, 50));

        return circles;

    def display(self, frames):
        for frame in frames:
            #Display the circles it finds
            for circle in circles:
                p = cvPoint(int(circle[0]), int(circle[1]));
                r = int(circle[2]);
                cvCircle(frame, p, r, white);



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

#Provide HSV range for orange ball
orange_ball = TrackableBall((5,150,100),(15,256,200),(255,115,0), "orange ball");
luigi_ball = TrackableBall((40,200,50),(70,256,100),(255,115,0), "Luigi!");
#List of balls to track
balls = [orange_ball, luigi_ball];

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
fps = FPS_Logger();
while 1:
    #Query and show frame
    frame = cvQueryFrame(capture);
    if frame == None:
        print "Yikes. Frame is None. Aborting"
        exit(1);

    #Convert BGR -> HSV
    cvCvtColor(frame, hsv_frame, CV_BGR2HSV);

    for ball in balls:
        circles = ball.track(hsv_frame);
        ball.display([frame, hsv_frame, ball.thresholded]);
        cvShowImage(ball.name, ball.thresholded);

    #Show images. 
    #WaitKey magically flushes buffer? Dunno why.
    cvShowImage("Webcam", frame);
    cvShowImage("HSV_Webcam", hsv_frame);
    cvWaitKey(1);

    #Measure rate
    fps.measure_FPS();

