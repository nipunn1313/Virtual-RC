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

#include "CycleTimer.h"

using namespace cv;

#define CAPTURE_IND 0
#define BLURRED_IND 1
#define HSV_IND 2
#define COLOR_FILTER_IND 3
#define BG_SUB_IND 4
#define DYN_BLOBS_IND 5
#define BLOBS_IND 6
#define ERODED_IND 7
#define DILATED_IND 8
#define BG_AND_COLOR_IND 9
#define THRESH_IND 10
#define COLOR_DILATED 11

#define STATIC_FRAME_THRESHOLD 1

#define NUM_FRAMES (sizeof(frameNames) / sizeof(frameNames[0]))
#define NUM_WINDOWS (sizeof(windows) / sizeof(windows[0]))

// Size of a car blob in pixels
#define CAR_BLOB_SIZE 3000

#define GOING_FOR_SPEED "YEAH"

static String frameNames[] = 
    {"Capture", "Blurred", "HSV", "InRange", "BG Subtracted", "Dynamic Blobs",
        "Blobs", "BGSub&Eroded", "BGSub&Dilated", "BGSub AND color",
        "Thresholded BGSub", "Color filter dilated"};
/* Only the frames we want displayed */
#ifdef GOING_FOR_SPEED
static int windows[] = {CAPTURE_IND};
#else
static int windows[] = {CAPTURE_IND, 
                        //THRESH_IND,
                        COLOR_FILTER_IND,
                        //COLOR_DILATED,
                        BLOBS_IND
                        };
#endif

// Known location of the car. Need a mutex to lock around read/write access of
// these globals
static pthread_mutex_t loc_mx;
static float car_x = 0;
static float car_y = 0;
static bool use_static = false;

static pthread_mutex_t click_mx;
static pthread_cond_t click_cond;
static int need_click_xy = 0;
static int click_x = 0;
static int click_y = 0;

IplImage *temp_IPL_8U_1;
IplImage *temp_IPL_8U_3;

struct hsv_color
{
    uchar h;
    uchar s;
    uchar v;
};

/* Functions for printing the HSV frame and a CvPoint2D32f */
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

// Callbacks to find HSV value of a left click or the size of two right clicks
void HSVAndSizeMouseCallback(int event, int x, int y, int flags, void *frame_p)
{
    Mat *mat_p = (Mat *) frame_p;
    static int click_parity = 0;
    static int top_left_x;
    static int top_left_y;

    // Print HSV values at (x,y) (assumes frame_p is the HSV frame)
    if (event == CV_EVENT_LBUTTONDOWN)
    {
        pthread_mutex_lock(&click_mx);
        if (need_click_xy)
        {
            click_x = x;
            click_y = y;
            need_click_xy = 0;
        }
        pthread_mutex_unlock(&click_mx);

        hsv_color color = mat_p->at<hsv_color>(y,x);
        std::cout << "HSVColor=" << color << std::endl;
    }
    
    // Calculate size of rectangle based on two right-clicks (top left corner
    // and bottom right corner). Remember that top left of image is (0,0)
    if (event == CV_EVENT_RBUTTONDOWN)
    {
        exit(0);
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

// Generates blob_image and blobs
void locate_car_core(Mat *frame, IplImage *blob_image, CBlobResult *blobs_p)
{
    IplImage *frame_as_image = temp_IPL_8U_1;

    CvMat copy(*frame);
    cvCopy(&copy, frame_as_image);
    cvMerge(&copy, &copy, &copy, NULL, blob_image);

    *blobs_p = CBlobResult(frame_as_image, NULL, 0);

    blobs_p->Filter(*blobs_p, B_EXCLUDE, CBlobGetArea(), B_OUTSIDE,
            CAR_BLOB_SIZE - 2000, CAR_BLOB_SIZE + 2000);
}

IplImage* static_loc_car(Mat *color_frame)
{
    CBlobResult blobs;
    IplImage *blob_image = temp_IPL_8U_3;

    locate_car_core(color_frame, blob_image, &blobs);

    for (int i = 0; i < blobs.GetNumBlobs(); i++)
    {
        CBlob *currentBlob;
        currentBlob = blobs.GetBlob(i);
        currentBlob->FillBlob(blob_image, CV_RGB(0, 0, 255));

        CvPoint2D32f center = currentBlob->GetEllipse().center;
        pthread_mutex_lock(&loc_mx);
        car_x = center.x;
        car_y = center.y;
        pthread_mutex_unlock(&loc_mx);
        //std::cout << center << std::endl;
    }

    return blob_image;
}

IplImage* dynamic_loc_car(Mat *bit_anded_frame)
{
    static int missing_car_frame_count = 0;

    CBlobResult blobs;
    IplImage *blob_image = temp_IPL_8U_3;

    locate_car_core(bit_anded_frame, blob_image, &blobs);

    for (int i = 0; i < blobs.GetNumBlobs(); i++)
    {
        CBlob *currentBlob;
        currentBlob = blobs.GetBlob(i);
        currentBlob->FillBlob(blob_image, CV_RGB(0, 255, 0));

        // Reset the number of frames that the car has been missing in
        // dynamic_loc_car(). We always add to this after the loop
        missing_car_frame_count = -1;

        // TODO: add logic. For now, we just assign the last blob's center value
        // to car_x and car_y. If not here, then in static_loc_car. If it fails
        // there then we just leave the old value for now.
        CvPoint2D32f center = currentBlob->GetEllipse().center;
        pthread_mutex_lock(&loc_mx);
        car_x = center.x;
        car_y = center.y;
        pthread_mutex_unlock(&loc_mx);
        //std::cout << center << std::endl;
    }

    // Sort of hacky. We always set it to -1 in the loop so it'll be 0 if it was
    // found in this frame
    missing_car_frame_count++;

    if (missing_car_frame_count > STATIC_FRAME_THRESHOLD)
    {
        return NULL;
    }
    else
    {
        return blob_image;
    }
}

// Main function for tracking
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
    //cap.set(CV_CAP_PROP_BRIGHTNESS, .60);
    //cap.set(CV_CAP_PROP_SATURATION, .08);
    //cap.set(CV_CAP_PROP_CONTRAST, .07);

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
    
    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");
    
    Mat dilate_elem_norm = getStructuringElement(MORPH_ELLIPSE,
            Size(15, 15), Point(-1, -1));
    Mat dilate_elem_huge = getStructuringElement(MORPH_ELLIPSE,
            Size(25, 25), Point(-1, -1));
    Mat erode_elem = getStructuringElement(MORPH_RECT,
            Size(5, 5), Point(-1, -1));

    temp_IPL_8U_1 = cvCreateImage(image.size(), IPL_DEPTH_8U, 1);
    temp_IPL_8U_3 = cvCreateImage(image.size(), IPL_DEPTH_8U, 3);

    double t = CycleTimer::currentSeconds();
    double latency_t = CycleTimer::currentSeconds();
    double tot_tp = 0;
    double tot_lat = 0;
    int timer_cnt = 0;

    for (;;)
    {
        /* Do timing stuff */
        double newt = CycleTimer::currentSeconds();
        tot_tp += (newt - t);
        tot_lat += (newt - latency_t);
        t = newt;
        if ((++timer_cnt) % 16 == 0)
        {
            std::cout << "FPS (Throughput)=" << (16 / tot_tp) 
                << "\n";
            std::cout << "Frame Latency =" << (tot_lat / 16) 
                << "\n";
            tot_tp = tot_lat = 0;
        }

        cap >> image;
        if (image.empty()) break;
        
        latency_t = CycleTimer::currentSeconds();

        frames[CAPTURE_IND] = image.clone();
        
        medianBlur(frames[CAPTURE_IND], frames[BLURRED_IND], 3);
        mog(frames[BLURRED_IND], frames[BG_SUB_IND], -1);
        erode(frames[BG_SUB_IND], frames[ERODED_IND], Mat());
        dilate(frames[ERODED_IND], frames[ERODED_IND], dilate_elem_norm);
        threshold(frames[ERODED_IND], frames[THRESH_IND], 128, 255, 
                THRESH_BINARY);

        // Color filter (yellow)
        cvtColor(frames[BLURRED_IND], frames[HSV_IND], CV_BGR2HSV);
        inRange(frames[HSV_IND], Scalar(20, 35, 150), 
                Scalar(45, 90, 255), frames[COLOR_FILTER_IND]);
        // Erode noise away and then dilate it before anding
        erode(frames[COLOR_FILTER_IND], frames[COLOR_FILTER_IND], erode_elem);
        dilate(frames[COLOR_FILTER_IND], frames[COLOR_DILATED], 
                dilate_elem_huge);

        bitwise_and(frames[THRESH_IND], frames[COLOR_DILATED], 
               frames[BG_AND_COLOR_IND]);

        dilate(frames[BG_AND_COLOR_IND], frames[DILATED_IND], dilate_elem_norm);

        // Get blobs
        IplImage *blobImage;
        if (use_static == false)
        {
            blobImage = dynamic_loc_car(&frames[DILATED_IND]);

            if (blobImage == NULL)
            {
                // Use static_loc_car() the next frame;
                use_static = true;
            }
        }
        else
        {
            blobImage = static_loc_car(&frames[COLOR_DILATED]);
            use_static = false;
        }

        //std::cout << car_x << "," << car_y << std::endl;

        // Only update if we actually got a new blob
        if (blobImage != NULL)
        {
            frames[BLOBS_IND] = blobImage;
        }

        for (int i=0; i<NUM_WINDOWS; i++)
        {
            int frame = windows[i];
            imshow(frameNames[frame], frames[frame]);
        }

        waitKey(1);
    }

    return 0;
}

static void init_stuff()
{
    pthread_mutex_init(&click_mx, NULL);
    pthread_cond_init(&click_cond, NULL);

    // Lock for the car's position variables
    pthread_mutex_init(&loc_mx, NULL);
        
}

int main()
{
    init_stuff();
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
        init_stuff();
        pthread_create(&thr, NULL, cap_thr, NULL);
    } 
    else
    {
        std::cout << "Tracker is already initialized";
    }
}

void ask_for_click()
{
    need_click_xy = 1;
}

tuple get_click_loc()
{
    tuple xy;

    /* Set the need for a click loc and
       spin on it */
    pthread_mutex_lock(&click_mx);
    if (need_click_xy)
        xy = tuple();
    else
        xy = make_tuple(click_x, click_y);
    pthread_mutex_unlock(&click_mx);

    return xy;
}

tuple get_curr_loc()
{
    float x, y;

    pthread_mutex_lock(&loc_mx);
    x = car_x;
    y = car_y;
    pthread_mutex_unlock(&loc_mx);

    return make_tuple(x, y);
}

BOOST_PYTHON_MODULE(MOGBlob)
{
    def("init_tracker", init_tracker);
    def("get_curr_loc", get_curr_loc);
    def("ask_for_click", ask_for_click);
    def("get_click_loc", get_click_loc);
}

