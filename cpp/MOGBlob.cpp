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
#define BLURRED_IND 1
#define HSV_IND 2
#define COLOR_FILTER_IND 3
#define BG_SUB_IND 4
#define REBLURRED_IND 5
#define BLOBS_IND 6

#define NUM_WINDOWS 7
#define NUM_FRAMES 7
static String frameNames[NUM_FRAMES] = 
    {"Capture", "Blurred", "HSV", "InRange", "BG Subtracted", "Reblurred", 
        "Blobs"};
/* Only the frames we want displayed */
static int windows[NUM_WINDOWS] = {0, 1, 2, 3, 4, 5, 6};

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

    Mat image;
    cap >> image; /* Initial capture for the size info */
    Mat frames[NUM_FRAMES]; /* Frames for each stage */

    // Create windows
    for (int i = 0; i < NUM_WINDOWS; i++)
    {
        int frame = windows[i];
        namedWindow(frameNames[frame], CV_WINDOW_AUTOSIZE);

        // Mouse callback for HSV. Callback takes
        if (frame == CAPTURE_IND) {
            setMouseCallback(frameNames[CAPTURE_IND], HSVMouseCallback, 
                    &frames[CAPTURE_IND]);
        }
    }
    
    IplImage *moggedAndSmoothed;
    IplImage *blobImage;
    moggedAndSmoothed = cvCreateImage(image.size(), IPL_DEPTH_8U, 1);
    blobImage = cvCreateImage(image.size(), IPL_DEPTH_8U, 3);

    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");
    
    for (;;)
    {
        cap >> image;
        if (image.empty()) break;

        frames[CAPTURE_IND] = image.clone();

        //threshold(frames[0], frames[1], 128, 255, THRESH_BINARY);
        //erode(frames[2], frames[3], Mat());
        //dilate(frames[3], frames[4], Mat());
        
        medianBlur(frames[CAPTURE_IND], frames[BLURRED_IND], 3);
        cvtColor(frames[BLURRED_IND], frames[HSV_IND], CV_BGR2HSV);
        inRange(frames[HSV_IND], Scalar(70, 50, 90), 
                Scalar(90, 160, 150), frames[COLOR_FILTER_IND]);
	
        mog(frames[COLOR_FILTER_IND], frames[BG_SUB_IND], -1);
	    medianBlur(frames[BG_SUB_IND], frames[REBLURRED_IND], 9);

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

        frames[BLOBS_IND] = blobImage;

        for (int i=0; i<NUM_WINDOWS; i++) {
            int frame = windows[i];
            imshow(frameNames[frame], frames[frame]);
        }

        waitKey(1);
    }
}

