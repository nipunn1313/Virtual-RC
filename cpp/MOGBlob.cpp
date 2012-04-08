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
#define ERODED_IND 7
#define DILATED_IND 8
#define BG_AND_COLOR_IND 9
#define THRESH_IND 10
#define COLOR_DILATED 11

#define NUM_FRAMES (sizeof(frameNames) / sizeof(frameNames[0]))
#define NUM_WINDOWS (sizeof(windows) / sizeof(windows[0]))

static String frameNames[] = 
    {"Capture", "Blurred", "HSV", "InRange", "BG Subtracted", "Reblurred", 
        "Blobs", "BGSub&Eroded", "BGSub&Dilated", "BGSub AND color",
        "Thresholded BGSub", "Color filter dilated"};
/* Only the frames we want displayed */
static int windows[] = {CAPTURE_IND, 
                        THRESH_IND,
                        COLOR_DILATED,
                        BLOBS_IND};

static float car_x = 0;
static float car_y = 0;

struct hsv_color
{
    uchar h;
    uchar s;
    uchar v;
};

std::ostream& operator<<(std::ostream& o, hsv_color &c)
{
    o << "(" << (int)c.h << "," << (int)c.s << "," << (int)c.v << ")";
    return o;
}

std::ostream& operator<<(std::ostream& o, CvPoint2D32f &p)
{
    o << "(" << p.x << "," << p.y << ")";
    return o;
}

void HSVAndSizeMouseCallback(int event, int x, int y, int flags, void *frame_p)
{
    Mat *mat_p = (Mat *) frame_p;
    static int click_parity = 0;
    static int top_left_x;
    static int top_left_y;

    // Print HSV values at (x,y) (assumes frame_p is the HSV frame)
    if (event == CV_EVENT_LBUTTONDOWN)
    {
        hsv_color color = mat_p->at<hsv_color>(y,x);
        std::cout << "HSVColor=" << color << std::endl;
    }
    
    // Calculate size of rectangle based on two right-clicks (top left corner
    // and bottom right corner). Remember that top left of image is (0,0)
    if (event == CV_EVENT_RBUTTONDOWN)
    {
        // First click for top left corner of rectangle
        if (click_parity == 0)
        {
            top_left_x = x;
            top_left_y = y;
        }
        else // Bottom right (x,y) coordinates
        {
            std::cout << "(" << x - top_left_x << "," << y - top_left_y << ")" 
                      << std::endl;
        }

        click_parity = (click_parity + 1) % 2;
    }
}

void* cap_thr(void* arg)
{
    puts("***Initializing capture***\n");

    VideoCapture cap;

    for (int i=1; i>=0; i--)
    {
        cap.open(i);
        if (cap.isOpened())
        {
            break;
        }
    }
    if (!cap.isOpened())
    {
        puts("***Initializing camera failed***\n");
        return 0;
    }

    // Initialize video capture settings
    cap.set(CV_CAP_PROP_BRIGHTNESS, .70);
    cap.set(CV_CAP_PROP_SATURATION, .106);

    Mat image;
    cap >> image; /* Initial capture for the size info */
    Mat frames[NUM_FRAMES]; /* Frames for each stage */

    // Create windows
    for (int i = 0; i < NUM_WINDOWS; i++)
    {
        int frame = windows[i];
        namedWindow(frameNames[frame], CV_WINDOW_AUTOSIZE);

        // Mouse callback for HSV. Callback takes
        if (frame == CAPTURE_IND)
        {
            setMouseCallback(frameNames[CAPTURE_IND], HSVAndSizeMouseCallback, 
                    &frames[HSV_IND]);
        }
    }
    
    IplImage *moggedAndSmoothed;
    IplImage *blobImage;
    moggedAndSmoothed = cvCreateImage(image.size(), IPL_DEPTH_8U, 1);
    blobImage = cvCreateImage(image.size(), IPL_DEPTH_8U, 3);

    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");
    
    Mat dilate_elem_norm = getStructuringElement(MORPH_ELLIPSE,
            Size(15, 15), Point(-1, -1));
    Mat dilate_elem_huge = getStructuringElement(MORPH_ELLIPSE,
            Size(25, 25), Point(-1, -1));
    Mat erode_elem = getStructuringElement(MORPH_ELLIPSE,
            Size(10, 10), Point(-1, -1));

    for (;;)
    {
        cap >> image;
        if (image.empty()) break;

        frames[CAPTURE_IND] = image.clone();
        
        medianBlur(frames[CAPTURE_IND], frames[BLURRED_IND], 3);
        mog(frames[BLURRED_IND], frames[BG_SUB_IND], -1);
        erode(frames[BG_SUB_IND], frames[ERODED_IND], Mat());
        dilate(frames[ERODED_IND], frames[ERODED_IND], dilate_elem_norm);
        threshold(frames[ERODED_IND], frames[THRESH_IND], 128, 255, 
                THRESH_BINARY);

        // Color filter (green)
        cvtColor(frames[BLURRED_IND], frames[HSV_IND], CV_BGR2HSV);
        //inRange(frames[HSV_IND], Scalar(60, 50, 70), 
                //Scalar(130, 160, 200), frames[COLOR_FILTER_IND]);
        inRange(frames[HSV_IND], Scalar(19, 0, 0), 
                Scalar(23, 255, 255), frames[COLOR_FILTER_IND]);
        // Erode noise away and then dilate it before anding
        erode(frames[COLOR_FILTER_IND], frames[COLOR_FILTER_IND], erode_elem);
        dilate(frames[COLOR_FILTER_IND], frames[COLOR_DILATED], 
                dilate_elem_huge);

        bitwise_and(frames[THRESH_IND], frames[COLOR_DILATED], 
               frames[BG_AND_COLOR_IND]);

        dilate(frames[BG_AND_COLOR_IND], frames[DILATED_IND], dilate_elem_norm);
    
        // Get blobs
        CBlobResult blobs;

        // We need to copy over the filtered frame for the CBlobResult()
        CvMat copy(frames[DILATED_IND]);
        cvCopy(&copy, moggedAndSmoothed);
        cvMerge(&copy, &copy, &copy, NULL, blobImage);

        blobs = CBlobResult(moggedAndSmoothed, NULL, 0);
        blobs.Filter(blobs, B_EXCLUDE, CBlobGetArea(), B_OUTSIDE, 
                1000, 4000);

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
            CvPoint2D32f center = currentBlob->GetEllipse().center;
            //car_x = center.x;
            //car_y = center.y;
            std::cout << center << std::endl;
        }

        frames[BLOBS_IND] = blobImage;

        for (int i=0; i<NUM_WINDOWS; i++)
        {
            int frame = windows[i];
            imshow(frameNames[frame], frames[frame]);
        }

        waitKey(1);
    }

    return 0;
}

int main()
{
    cap_thr(NULL);
}

#include <boost/python.hpp>
using namespace boost::python;

void init_tracker()
{
    static int already_initted=0;
    pthread_t thr;

    if (! already_initted) 
    {
        already_initted = 1;
        pthread_create(&thr, NULL, cap_thr, NULL);
    } 
    else
    {
        std::cout << "Tracker is already initialized";
    }
}

tuple get_curr_loc()
{
    return make_tuple(69, 69);
}

BOOST_PYTHON_MODULE(MOGBlob)
{
    def("init_tracker", init_tracker);
    def("get_curr_loc", get_curr_loc);
}

