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

#include <stdio.h>

using namespace cv;

#define CAPTURE_IND 0
#define MOGGED_IND 1
#define THRESHOLDED_IND 2
#define BLURRED_IND 3
#define ERODED_IND 4
#define DILATED_IND 5
#define NUM_WINDOWS 6
#define NUM_FRAMES 6

int main()
{
    puts("***Initializing capture***\n");

    VideoCapture cap;

    cap.open(1);
    if (!cap.isOpened())
    {
        puts("***Initializing camera failed***\n");
        return 0;
    }

    String windowNames[NUM_WINDOWS] = 
        {"Capture", "Mogged", "Thresholded", "Blurred", "Eroded", "Dilated"};
    // Create windows
    for (int i = 0; i < NUM_WINDOWS; i++)
    {
        namedWindow(windowNames[i], CV_WINDOW_AUTOSIZE);
    }

    Mat image;
    // Correspond to the windows
    Mat frames[NUM_FRAMES];
    
    moggedAndSmoothed = createImage(CV_WINDOW_AUTOSIZE, IPL_DEPTH_8U, 1);
    blobImage = createImage(CV_WINDOW_AUTOSIZE, IPL_DEPTH_8U, 1);

    BackgroundSubtractorMOG2 mog;

    puts("***Done initializing capture***\n");
    
    for (;;)
    {
        cap >> image;
        if (image.empty())
        {
            break;
        }

        frames[0] = image.clone();

        mog(frames[0], frames[1], -1);

        threshold(frames[1], frames[2], 128, 255, THRESH_BINARY);
        medianBlur(frames[2], frames[3], 9);
        erode(frames[3], frames[4], Mat());
        dilate(frames[4], frames[5], Mat());

        CBlobResult blobs;
        CBlob *blob;

        blobs = CBlobResult(frames[5], NULL, 255);
        blobs.Filter(blobs, B_EXCLUDE, CBlobGetArea(), B_GREATER, 10000);

        for (i = 0; i < blobs.GetNumBlobs(); i++)
        {
            currentBlob = blobs.GetBlob(i);
            currentBlob->FillBlob(blobImage, CV_RGB(255, 0, 0));
        }

        imshow(windowNames[0], frames[0]);
        imshow(windowNames[5], frames[5]);

        waitKey(1);
    }
}
