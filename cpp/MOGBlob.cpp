#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/video/tracking.hpp"
#include "cvblob/BlobContour.h"
#include "cvblob/blob.h"
#include "cvblob/BlobLibraryConfiguration.h"
#include "cvblob/BlobOperators.h"
#include "cvblob/BlobProperties.h"
#include "cvblob/BlobResult.h"
#include "cvblob/ComponentLabeling.h"

#include <iostream>
#include <stdio.h>

using namespace cv;

#define CAPTURE_IND 0
#define MOGGED_IND 1
#define THRESHOLDED_IND 2
#define BLURRED_IND 3
#define ERODED_IND 4
#define DILATED_IND 5
#define NUM_WINDOWS 3
#define NUM_FRAMES 6

struct hsv_color {
    uchar h;
    uchar s;
    uchar v;
};

std::ostream& operator<<(std::ostream& o, hsv_color &c) {
    o << "(" << (int)c.h << "," << (int)c.s << "," << (int)c.v << ")";
    return o;
}

void HSVMouseCallback(int event, int x, int y, int flags, void *frame_p)
{
    Mat *mat_p = (Mat *) frame_p;

    hsv_color color = mat_p->at<hsv_color>(y,x);
    std::cout << "Color=" << color << std::endl;
}

int main()
{
    puts("***Initializing capture***\n");

    VideoCapture cap;

    for (int i=1; i>=0; i--) {
        cap.open(i);
        if (cap.isOpened())
            break;
    }
    if (!cap.isOpened())
    {
        puts("***Initializing camera failed***\n");
        return 0;
    }

    String windowNames[NUM_WINDOWS] = 
    {"Capture", "Blobs", "filtered image"};
        //{"Capture", "Mogged", "Thresholded"};// "Blurred", "Eroded", "Dilated"};

    // Create windows
    for (int i = 0; i < NUM_WINDOWS; i++)
    {
        namedWindow(windowNames[i], CV_WINDOW_AUTOSIZE);
    }

    Mat image;
    // Correspond to the windows
    Mat frames[NUM_FRAMES];

    // Mouse callback for HSV
    setMouseCallback(windowNames[0], HSVMouseCallback, &frames[2]);

    cap >> image;
    
    IplImage *moggedAndSmoothed;
    IplImage *blobImage;
    moggedAndSmoothed = cvCreateImage(image.size(), IPL_DEPTH_8U, 1);
    blobImage = cvCreateImage(image.size(), IPL_DEPTH_8U, 3);

    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");
    
    for (;;)
    {
        cap >> image;
        if (image.empty())
        {
            break;
        }

        frames[0] = image.clone();

        //threshold(frames[0], frames[1], 128, 255, THRESH_BINARY);
        //erode(frames[2], frames[3], Mat());
        //dilate(frames[3], frames[4], Mat());
        
        medianBlur(frames[0], frames[1], 3);
        cvtColor(frames[1], frames[2], CV_BGR2HSV);
        inRange(frames[2], (0, 0, 0), (250, 250, 250), frames[3]);
        //inRange(frames[2], (70, 50, 90), (90, 160, 150), frames[3]);
	
        mog(frames[3], frames[4], -1);
	    medianBlur(frames[4], frames[5], 9);

	    // Get blobs
        CBlobResult blobs;

	    // We need to copy over the filtered frame for the CBlobResult()
        CvMat copy(frames[5]);
        CvMat copy2(frames[5]);
        cvCopy(&copy, moggedAndSmoothed);
        cvMerge(&copy2, &copy2, &copy2, NULL, blobImage);

        blobs = CBlobResult(moggedAndSmoothed, NULL, 0);
        blobs.Filter(blobs, B_EXCLUDE, CBlobGetArea(), B_OUTSIDE, 
                100, 250);

        for (int i = 0; i < blobs.GetNumBlobs(); i++)
        {
            CBlob *currentBlob;
            currentBlob = blobs.GetBlob(i);
            currentBlob->FillBlob(blobImage, CV_RGB(255, 0, 0));
	        /*
            printf("Found a blob! x=(%f,%f) y=(%f,%f)\n", currentBlob->MinX(),
                    currentBlob->MaxX(),
                    currentBlob->MinY(),
                    currentBlob->MaxY()
                    );
		    */
        }

        imshow(windowNames[0], frames[0]);
        imshow(windowNames[2], frames[2]);
        Mat b(blobImage);
        imshow(windowNames[1], b);

        waitKey(1);
    }
}
