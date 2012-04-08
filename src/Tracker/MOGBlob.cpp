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

#define NUM_FRAMES (sizeof(frameNames) / sizeof(frameNames[0]))
#define NUM_WINDOWS (sizeof(windows) / sizeof(windows[0]))

// Size of a car blob in pixels
#define CAR_BLOB_SIZE_MIN 1000
#define CAR_BLOB_SIZE_MAX 5500

#define DIST_SQUARED(x1, y1, x2, y2) (((x2) - (x1))*((x2) - (x1)) + \
                                      ((y2) - (y1))*((y2) - (y1)))

// Location of new blob must be less than this far from the previously
// known location in pixels
// TODO: Why does this need to be so high
#define CAR_MAX_DIST_SQUARED 500000

// Number of frames that dynamic_loc_car() has failed to find the car before we
// switch to static_loc_car()
#define STATIC_LOC_THRESHOLD 1

//#define GOING_FOR_SPEED "YEAH"

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

// Globals for tracking logic
static int immobile_frame_count = 0;
static bool is_car_missing = false;

// Known location of the car. Need a mutex to lock around read/write access of
// these globals
static pthread_mutex_t loc_mx;
static float car_x = 0;
static float car_y = 0;

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
            CAR_BLOB_SIZE_MIN, CAR_BLOB_SIZE_MAX);
}

IplImage* find_and_draw_best_blob(CBlobResult blobs, IplImage *blob_image, 
                                  CvScalar blob_color)
{
    // Mark first blob as best candidate (doing it here makes code cleaner)
    CBlob *best_blob = blobs.GetBlob(0);
    float best_blob_x = best_blob->GetEllipse().center.x;
    float best_blob_y = best_blob->GetEllipse().center.y;
    float min_dist = DIST_SQUARED(best_blob_x, car_x, best_blob_y, car_y);

    // Found too many blobs. Pick the best one based on proximity to 
    // previous car location
    if (blobs.GetNumBlobs() > 1)   
    {
        for (int blobNum = 1; blobNum < blobs.GetNumBlobs(); blobNum++)
        {
            CBlob *curr_blob = blobs.GetBlob(blobNum);
            float curr_blob_x = curr_blob->GetEllipse().center.x;
            float curr_blob_y = curr_blob->GetEllipse().center.y;
            float curr_dist = DIST_SQUARED(car_x, curr_blob_x, 
                    car_y, curr_blob_y);

            if (curr_dist < min_dist && curr_dist < CAR_MAX_DIST_SQUARED)
            {
                best_blob = curr_blob;
                best_blob_x = curr_blob_x;
                best_blob_y = curr_blob_y;
                min_dist = curr_dist;
            }
        }
    }

    // The best blob is too far!
    if (min_dist >= CAR_MAX_DIST_SQUARED)
    {
        return NULL;
    }
    else
    {
        // Update global location variables
        pthread_mutex_lock(&loc_mx);
        car_x = best_blob_x;
        car_y = best_blob_y;
        pthread_mutex_unlock(&loc_mx);

        // Color it
        best_blob->FillBlob(blob_image, blob_color);
        return blob_image;
    }
}

IplImage* static_loc_car(Mat *color_frame)
{
    CBlobResult blobs;
    IplImage *blob_image = temp_IPL_8U_3;

    locate_car_core(color_frame, blob_image, &blobs);

    if (blobs.GetNumBlobs() == 0)
    {
        return NULL;
    }
    else
    {
        return find_and_draw_best_blob(blobs, blob_image, CV_RGB(0, 0, 255));
    }
}

IplImage* dynamic_loc_car(Mat *bit_anded_frame)
{
    CBlobResult blobs;
    IplImage *blob_image = temp_IPL_8U_3;

    locate_car_core(bit_anded_frame, blob_image, &blobs);

    // Didn't find any blobs of reasonable size in the anded frame
    if (blobs.GetNumBlobs() == 0)
    {
        return NULL;
    }
    else 
    {
        return find_and_draw_best_blob(blobs, blob_image, CV_RGB(0, 255, 0));
    }
}

// Main function for tracking
void* cap_thr(void* arg)
{
    // TODO: For testing. Remove
    //int num_frames = 0;
    //int frames_failed = 0;
    
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
    //cap.set(CV_CAP_PROP_BRIGHTNESS, .40);
    //cap.set(CV_CAP_PROP_SATURATION, .08);
    //cap.set(CV_CAP_PROP_CONTRAST, .07);

    Mat image;
    cap >> image; /* Initial capture for the size info */
    Mat frames[NUM_FRAMES]; /* Frames for each stage */

    // Create windows
    for (unsigned int i = 0; i < NUM_WINDOWS; i++)
    {
        int frame_ind = windows[i];
        namedWindow(frameNames[frame_ind], CV_WINDOW_AUTOSIZE);

        // Mouse callback for HSV. Callback takes
        if (frame_ind == CAPTURE_IND)
        {
            setMouseCallback(frameNames[CAPTURE_IND], 
                             HSVAndSizeMouseCallback, &frames[HSV_IND]);
        }
        // Need to initialize BLOBS_IND frame since loc_car() usually returns
        // NULL at first
        if (frame_ind == BLOBS_IND)
        {
            frames[frame_ind] = image;
        }
    }
    
    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");
    
    // Structuring elements for dilation and erosion
    Mat dilate_elem_norm = getStructuringElement(MORPH_RECT,
            Size(15, 15), Point(-1, -1));
    Mat dilate_elem_huge = getStructuringElement(MORPH_RECT,
            Size(20, 20), Point(-1, -1));
    Mat erode_elem = getStructuringElement(MORPH_RECT,
            Size(7, 7), Point(-1, -1));

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
        /*
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
        */

        cap >> image;
        if (image.empty()) break;
        
        latency_t = CycleTimer::currentSeconds();

        //num_frames++;

        frames[CAPTURE_IND] = image.clone();
        
        medianBlur(frames[CAPTURE_IND], frames[BLURRED_IND], 3);
        mog(frames[BLURRED_IND], frames[BG_SUB_IND], -1);
        erode(frames[BG_SUB_IND], frames[ERODED_IND], Mat());
        dilate(frames[ERODED_IND], frames[ERODED_IND], dilate_elem_norm);
        threshold(frames[ERODED_IND], frames[THRESH_IND], 128, 255, 
                THRESH_BINARY);

        // Color filter (yellow)
        cvtColor(frames[BLURRED_IND], frames[HSV_IND], CV_BGR2HSV);
        inRange(frames[HSV_IND], Scalar(20, 40, 120), 
                Scalar(40, 130, 255), frames[COLOR_FILTER_IND]);
        // Erode noise away and then dilate it before anding
        erode(frames[COLOR_FILTER_IND], frames[COLOR_FILTER_IND], erode_elem);
        dilate(frames[COLOR_FILTER_IND], frames[COLOR_DILATED], 
                dilate_elem_huge);

        bitwise_and(frames[THRESH_IND], frames[COLOR_DILATED], 
               frames[BG_AND_COLOR_IND]);

        dilate(frames[BG_AND_COLOR_IND], frames[DILATED_IND], dilate_elem_norm);

        // Get blobs
        IplImage *blobImage;
        // First try dynamic
        blobImage = dynamic_loc_car(&frames[DILATED_IND]);

        if (blobImage == NULL)
        {
            immobile_frame_count++;

            // Try static_loc_car() if dynamic has failed for too many frames
            if (immobile_frame_count > STATIC_LOC_THRESHOLD)
            {
                blobImage = static_loc_car(&frames[COLOR_DILATED]);

                if (blobImage == NULL)
                {
                    // We failed to find it with both location functions!
                    is_car_missing = true;
                    //std::cout << "Car is missing" << std::endl;
                }
                else
                {
                    // We found the car statically
                    is_car_missing = false;
                    immobile_frame_count = 0;
                }
            }
        }
        else
        {
            is_car_missing = false;
            immobile_frame_count = 0;
        }

        // Only update if we actually got a new blob
        if (blobImage != NULL)
        {
            frames[BLOBS_IND] = blobImage;
        }

        for (unsigned int i=0; i<NUM_WINDOWS; i++)
        {
            int frame = windows[i];
            imshow(frameNames[frame], frames[frame]);
        }

        //std::cout << num_frames << ", " << frames_failed << std::endl;

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

