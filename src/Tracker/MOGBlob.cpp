#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/video/tracking.hpp"
#include "BlobContour.h"
#include "blob.h"
#include "BlobLibraryConfiguration.h"
#include "BlobOperators.h"
#include "BlobProperties.h"
#include "BlobResult.h"
#include "ComponentLabeling.h"

#include <iostream>
#include <stdio.h>

#include "CycleTimer.h"

using namespace cv;

#define NUM_CARS (sizeof(carNames) / sizeof(carNames[0]))

// Size of a car blob in pixels
#define CAR_BLOB_SIZE_MIN 1000
#define CAR_BLOB_SIZE_MAX 40000

#define DIST_SQUARED(x1, x2, y1, y2) ((((x2) - (x1))*((x2) - (x1))) + \
                                      (((y2) - (y1))*((y2) - (y1))))

// Location of new blob must be less than this far from the previously
// known location in pixels
#define CAR_MAX_DIST_SQUARED 1500

// Number of frames that dynamic_loc_car() has failed to find the car before we
// switch to static_loc_car()
#define STATIC_LOC_THRESHOLD 1

// For inRange()
#define CALC_RANGE_LOWER(x, buffer) (((x) >= buffer) ? (x) - buffer : 0)
#define CALC_RANGE_UPPER(x, buffer) (((x) <= (255 - buffer)) ? (x) + buffer : 255)

/* Shared between cars */
enum {
    CAPTURE_IND,
    BLURRED_IND,
    BG_SUB_IND,
    SUB_ERODE_IND,
    SUB_DILATE_IND,
    SUB_THRESH_IND,
    HSV_IND
};

static std::string sharedFrameNames[] = {
    "Capture", "Blurred", "BG Subtracted", "BG Sub + Erode",
    "BG Sub + Erode + Dilate", "BG Sub + Er+Dil + Threshold",
    "HSV Capture"
};

/* Per car frames */
enum {
    PER_CAR_COLOR_FILTER_IND,
    PER_CAR_COLOR_ERODED,
    PER_CAR_COLOR_DILATED,
    PER_CAR_BG_AND_COLOR_IND,
    PER_CAR_ANDED_DILATED,
    PER_CAR_BLOBS_IND
};

static String perCarFrameNames[] = {
    "Color Filtered", 
    "Color filtered+Eroded",
    "Color Filtered+Dilated", 
    "Color Filter ANDED with BG Sub Thresh",
    "Filtered ANDED, redilated",
    "Blobs"
};

#define NUM_SHARED_FRAMES \
    (sizeof(sharedFrameNames) / sizeof(sharedFrameNames[0]))
#define NUM_PER_CAR_FRAMES \
    (sizeof(perCarFrameNames) / sizeof(perCarFrameNames[0]))

/* Only the frames we want displayed */
#ifdef MULTI_DISPLAY
static int sharedWindows[] = {CAPTURE_IND, HSV_IND};
static int perCarWindows[] = {PER_CAR_COLOR_DILATED,
                              PER_CAR_BLOBS_IND};
#else
static int sharedWindows[] = {CAPTURE_IND};
static int perCarWindows[] = {};
#endif

#define NUM_SHARED_WINDOWS (sizeof(sharedWindows) / sizeof(sharedWindows[0]))
#define NUM_PER_CAR_WINDOWS (sizeof(perCarWindows) / sizeof(perCarWindows[0]))

static String carNames[] = {"Wario ", "Luigi "};

enum {
    IPL_1_IND,
    IPL_3_IND
};

// IplImage. Allocate once for each car. Reuse!
IplImage *tempIplImages[NUM_CARS][2];

enum Car {
    WARIO,
    LUIGI
};

struct hsv_color
{
    uchar h;
    uchar s;
    uchar v;
};

// Globals for mouse callback on the window
static pthread_mutex_t click_mx;
static pthread_cond_t click_cond;
static int need_click_xy = 0;
static int click_x = 0;
static int click_y = 0;

// Add ability to hide display (for performance)
static int hide_disp = 0;

static struct {
    // Calibrated HSV values for inRange()
    struct hsv_color min_color;
    struct hsv_color max_color;

    // Globals for tracking logic
    int immobile_frame_count; // Auto-Initialized to zero
    bool is_car_missing; // Auto-Initialized to zero
    int num_frames_missing;

    // Known location of the car. Need a mutex to lock around read/write 
    // access of these globals
    pthread_mutex_t loc_mx;
    bool car_found;
    float car_x; // Auto-Initialized to zero
    float car_y; // Auto-initialized to zero
    float box_x;
    float box_y;
} car_info[NUM_CARS];

// Structuring elements for dilation and erosion
Mat dilate_elem_norm = getStructuringElement(MORPH_RECT,
        Size(15, 15), Point(-1, -1));
Mat dilate_elem_huge = getStructuringElement(MORPH_RECT,
        Size(20, 20), Point(-1, -1));
Mat erode_elem = getStructuringElement(MORPH_RECT,
        Size(7, 7), Point(-1, -1));

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
        //exit(0);
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
void get_filtered_blobs(Mat *frame, IplImage *blob_image, CBlobResult *blobs_p,
                     int car_ind)
{
    IplImage *frame_as_image = tempIplImages[car_ind][IPL_1_IND];

    CvMat copy(*frame);
    cvCopy(&copy, frame_as_image);
    cvMerge(&copy, &copy, &copy, NULL, blob_image);

    *blobs_p = CBlobResult(frame_as_image, NULL, 0);

    blobs_p->Filter(*blobs_p, B_EXCLUDE, CBlobGetArea(), B_OUTSIDE,
            CAR_BLOB_SIZE_MIN, CAR_BLOB_SIZE_MAX);
}

IplImage* find_and_draw_best_blob(int car, CBlobResult blobs, 
                                  IplImage *blob_image, CvScalar blob_color)
{
    // Mark first blob as best candidate (doing it here makes code cleaner)
    CBlob *best_blob = blobs.GetBlob(0);
    float best_blob_x = best_blob->GetEllipse().center.x + car_info[car].box_x;
    float best_blob_y = best_blob->GetEllipse().center.y + car_info[car].box_y;

    float min_dist = DIST_SQUARED(best_blob_x, car_info[car].car_x, 
            best_blob_y, car_info[car].car_y);

    // Found multiple blobs. Pick the best one based on proximity to 
    // previous car location
    if (blobs.GetNumBlobs() > 1 && car_info[car].car_found)
    {
        for (int blobNum = 1; blobNum < blobs.GetNumBlobs(); blobNum++)
        {
            CBlob *curr_blob = blobs.GetBlob(blobNum);
            float curr_blob_x = curr_blob->GetEllipse().center.x + 
                car_info[car].box_x;
            float curr_blob_y = curr_blob->GetEllipse().center.y +
                car_info[car].box_y;
            float curr_dist = 
                    DIST_SQUARED(car_info[car].car_x, curr_blob_x, 
                                 car_info[car].car_y, curr_blob_y);

            if (curr_dist < min_dist && curr_dist < CAR_MAX_DIST_SQUARED)
            {
                best_blob = curr_blob;
                best_blob_x = curr_blob_x;
                best_blob_y = curr_blob_y;
                min_dist = curr_dist;
            }
        }
    }

    // Update global location variables
    // Make visible to python thread
    pthread_mutex_lock(&car_info[car].loc_mx);
    car_info[car].car_found = true;
    car_info[car].car_x = best_blob_x;
    car_info[car].car_y = best_blob_y;
    pthread_mutex_unlock(&car_info[car].loc_mx);

    // Color it
    best_blob->FillBlob(blob_image, blob_color);
    return blob_image;
}

bool static_loc_car(int car, Mat *color_frame, IplImage **blob_buf)
{
    CBlobResult blobs;
    *blob_buf = tempIplImages[car][IPL_3_IND];

    get_filtered_blobs(color_frame, *blob_buf, &blobs, car);

    if (blobs.GetNumBlobs() == 0)
    {
        pthread_mutex_lock(&car_info[car].loc_mx);
        car_info[car].car_found = false;
        pthread_mutex_unlock(&car_info[car].loc_mx);
        return false;
    }
    else
    {
        find_and_draw_best_blob(car, blobs, *blob_buf, 
                CV_RGB(0, 0, 255));
        return true;
    }
}

IplImage* dynamic_loc_car(int car, Mat *bit_anded_frame)
{
    CBlobResult blobs;
    IplImage *blob_image = tempIplImages[car][IPL_3_IND];

    get_filtered_blobs(bit_anded_frame, blob_image, &blobs, car);

    // Didn't find any blobs of reasonable size in the anded frame
    if (blobs.GetNumBlobs() == 0)
    {
        return NULL;
    }
    else 
    {
        return find_and_draw_best_blob(car, blobs, blob_image, 
                CV_RGB(0, 255, 0));
    }
}

void get_cap(VideoCapture &cap)
{
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
        exit(0);
    }
}

// Helpers to cap_thr

void create_windows(Mat *sharedFrames, Mat perCarFrames[][NUM_PER_CAR_FRAMES], 
        Mat &image)
{
    for (unsigned int i=0; i<NUM_SHARED_WINDOWS; i++)
    {
        int frame_ind = sharedWindows[i];
        namedWindow(sharedFrameNames[frame_ind], CV_WINDOW_AUTOSIZE);
        // Mouse callback for HSV. Callback takes
        if (frame_ind == CAPTURE_IND)
        {
            setMouseCallback(sharedFrameNames[CAPTURE_IND], 
                             HSVAndSizeMouseCallback, 
                             &sharedFrames[HSV_IND]);
        }
    }

    for (unsigned int car=0; car<NUM_CARS; car++)
    {
        for (unsigned int i=0; i<NUM_PER_CAR_WINDOWS; i++)
        {
            int frame_ind = perCarWindows[i];
            namedWindow(carNames[car] + perCarFrameNames[frame_ind], 
                    CV_WINDOW_AUTOSIZE);

            // Need to initialize BLOBS_IND frame since loc_car() usually 
            // returns NULL at first
            if (frame_ind == PER_CAR_BLOBS_IND)
            {
                perCarFrames[car][frame_ind] = image.clone();
            }
        }
    }
}

void display_windows(Mat *sharedFrames, Mat perCarFrames[][NUM_PER_CAR_FRAMES])
{
    if (hide_disp)
    {
        static int already_destroyed=0;
        if (! already_destroyed) {
            already_destroyed = 1;
            for (unsigned int i=0; i<NUM_SHARED_WINDOWS; i++) {
                int frame = sharedWindows[i];
                cvDestroyWindow(sharedFrameNames[frame].c_str());
            }
            for (unsigned int i=0; i<NUM_PER_CAR_WINDOWS; i++) {
                int frame = perCarWindows[i];
                for (int car=0; car<NUM_CARS; car++)
                    cvDestroyWindow((carNames[car]+
                                perCarFrameNames[frame]).c_str());
            }
        }
    }
    else
    {
        for (unsigned int i=0; i<NUM_SHARED_WINDOWS; i++)
        {
            int frame = sharedWindows[i];
            imshow(sharedFrameNames[frame], sharedFrames[frame]);
        }

        for (unsigned int i=0; i<NUM_PER_CAR_WINDOWS; i++)
        {
            int frame = perCarWindows[i];
            for (int car=0; car<NUM_CARS; car++)
            {
                imshow(carNames[car] + perCarFrameNames[frame], 
                        perCarFrames[car][frame]);
            }
        }
        waitKey(1);
    }
}

void free_temp_images() {
    for (unsigned int car_ind = 0; car_ind < NUM_CARS; car_ind++)
    {
        cvReleaseImage(&tempIplImages[car_ind][IPL_1_IND]);
        cvReleaseImage(&tempIplImages[car_ind][IPL_3_IND]);
    }
}

void init_temp_images(Size s) {
    for (unsigned int car_ind = 0; car_ind < NUM_CARS; car_ind++)
    {
        tempIplImages[car_ind][IPL_1_IND] =
            cvCreateImage(s, IPL_DEPTH_8U, 1);
        tempIplImages[car_ind][IPL_3_IND] =
            cvCreateImage(s, IPL_DEPTH_8U, 3);
    }
}

bool findBlobsFromCapture(int car, Mat capture, Mat *sharedFrames, 
        Mat perCarFrames[][NUM_PER_CAR_FRAMES], IplImage **blob_buf)
{
    // Blur
    medianBlur(capture, sharedFrames[BLURRED_IND], 3);
    // Convert to HSV 
    cvtColor(sharedFrames[BLURRED_IND], sharedFrames[HSV_IND], 
            CV_BGR2HSV);

    Scalar mincolor(CALC_RANGE_LOWER(car_info[car].min_color.h, 8),
            CALC_RANGE_LOWER(car_info[car].min_color.s, 70),
            CALC_RANGE_LOWER(car_info[car].min_color.v, 70));
    Scalar maxcolor(CALC_RANGE_UPPER(car_info[car].max_color.h, 8),
            CALC_RANGE_UPPER(car_info[car].max_color.s, 70),
            CALC_RANGE_UPPER(car_info[car].max_color.v, 70));

    inRange(sharedFrames[HSV_IND], mincolor, maxcolor,
            perCarFrames[car][PER_CAR_COLOR_FILTER_IND]);

    // Erode noise away and then dilate it before anding
    erode(perCarFrames[car][PER_CAR_COLOR_FILTER_IND], 
            perCarFrames[car][PER_CAR_COLOR_ERODED], erode_elem);
    dilate(perCarFrames[car][PER_CAR_COLOR_ERODED], 
            perCarFrames[car][PER_CAR_COLOR_DILATED], 
            dilate_elem_huge);

    // Reset values in box to invalid ones
    bool found = static_loc_car(car,
            &perCarFrames[car][PER_CAR_COLOR_DILATED],
            blob_buf);
    return found;
}

static double latency_t;
void end_timing_one_frame() {

    static double t;
    static double newt;
    static double tot_tp = 0;
    static double tot_lat = 0;
    static int timer_cnt = 0;

    newt = CycleTimer::currentSeconds();

    /* Measure the current time. Print every 16 frames */
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
}

void start_timing_one_frame() {
    latency_t = CycleTimer::currentSeconds();
}

// Main function for tracking
void* cap_thr(void* arg)
{
    puts("***Initializing capture***\n");

    VideoCapture cap;
    get_cap(cap);
    if (! cap.isOpened())
        return 0;

    // Initialize video capture settings
    //cap.set(CV_CAP_PROP_BRIGHTNESS, .40);
    //cap.set(CV_CAP_PROP_SATURATION, .08);
    //cap.set(CV_CAP_PROP_CONTRAST, .07);

    Mat image;
    cap >> image; /* Initial capture for the size info */

    Mat sharedFrames[NUM_SHARED_FRAMES]; /* Frames for each stage */
    Mat perCarFrames[NUM_CARS][NUM_PER_CAR_FRAMES];

    // Create windows
    create_windows(sharedFrames, perCarFrames, image);
    
    // Set up the background subtractor
    BackgroundSubtractorMOG2 mog(50, 16, true);

    puts("***Done initializing capture***\n");

    for (;;)
    {
        /* Do timing stuff */
        //end_timing_one_frame();

        cap >> image;
        if (image.empty()) break;
        
        /* Do some other timing stuff */
        //start_timing_one_frame();

        sharedFrames[CAPTURE_IND] = image.clone();

        // Color filter
        for (int car=0; car<NUM_CARS; car++)
        {

            // Look for blobs in a smaller (200 x 200) box around last known
            // location
#define HALFBOXSIZE 100
#define BOXSIZE (2 * HALFBOXSIZE)
#define clamp(x, min, max) ( ((x) < (min)) ? (min) : \
                       ( ((x) >= (max)) ? (max)-1 : (x) ))
#define clamp_to_width(x) clamp(x, 0, image.size().width)
#define clamp_to_height(y) clamp(y, 0, image.size().height)
            int boxx = clamp_to_width(car_info[car].car_x - HALFBOXSIZE);
            int boxy = clamp_to_height(car_info[car].car_y - HALFBOXSIZE);
            int boxx_R = clamp_to_width(boxx + BOXSIZE);
            int boxy_B = clamp_to_height(boxy + BOXSIZE);
            
            Range xrange(boxx, boxx_R);
            Range yrange(boxy, boxy_B);
#undef clamp
#undef clamp_to_width
#undef clamp_to_height

            Mat box = sharedFrames[CAPTURE_IND](yrange, xrange);

            // Store box top-left in globals
            car_info[car].box_x = boxx;
            car_info[car].box_y = boxy;

            // Allocate temp images to correct size
            free_temp_images();
            init_temp_images(box.size());

            IplImage *blobImage;
            bool found = findBlobsFromCapture(car, box, sharedFrames, 
                    perCarFrames, &blobImage);

            // Retry on whole image
            if (!found) {
                car_info[car].box_x = 0;
                car_info[car].box_y = 0;
                // Initialize IplImages used for cvBlobsLib
                free_temp_images();
                init_temp_images(sharedFrames[CAPTURE_IND].size());

                found = findBlobsFromCapture(car, 
                        sharedFrames[CAPTURE_IND], 
                        sharedFrames, 
                        perCarFrames,
                        &blobImage);
            }

            if (! found)
            {
                car_info[car].num_frames_missing++;
                car_info[car].is_car_missing = true;
                //std::cout << "Car is missing" << std::endl;
            }
            else
            {
                car_info[car].num_frames_missing = 0;
                car_info[car].is_car_missing = false;
                perCarFrames[car][PER_CAR_BLOBS_IND] = blobImage;
            }
            /*
            printf("car %d is at (%f, %f)\n", car,
                    car_info[car].car_x, car_info[car].car_y);
                    */
        }

        // display frames onto the windows
        display_windows(sharedFrames, perCarFrames);
    }

    return 0;
}

static void init_stuff()
{
    pthread_mutex_init(&click_mx, NULL);
    pthread_cond_init(&click_cond, NULL);

    // Lock for the car's position variables
    for (int car=0; car<NUM_CARS; car++)
        pthread_mutex_init(&car_info[car].loc_mx, NULL);

#ifndef SUPPRESS_CALIBRATE
    VideoCapture cap;
    get_cap(cap);
    if (! cap.isOpened())
        exit(0);

    // Get two clicks for calibrating colors
    Mat image;
    Mat bgr_frame;
    Mat hsv_frame;

    bgr_frame = image.clone();

    const char *windowName = "color calibration";

    // Create window
    namedWindow(windowName, CV_WINDOW_AUTOSIZE);
    // Mouse callback for HSV. Callback takes
    setMouseCallback(windowName, 
            HSVAndSizeMouseCallback, &hsv_frame);

#define NUM_CALIB_CLICKS 5
    for (int car=0; car<NUM_CARS; car++)
    {
        std::cout << "Calibrating " << carNames[car] << 
            ". Click " << NUM_CALIB_CLICKS << " times on the car\n";
        struct hsv_color &min_color = car_info[car].min_color;
        struct hsv_color &max_color = car_info[car].max_color;

        // Check values of NUM_CALIB_CLICKS. Store the
        // max and min h, s, and v values (separately)
        // in these structs
        min_color.h = min_color.s = min_color.v = 255;
        max_color.h = max_color.s = max_color.v = 0;

        for (int i=0; i<NUM_CALIB_CLICKS; i++) {
            need_click_xy = 1;
            while (need_click_xy)
            {
                cap >> image;
                bgr_frame = image.clone();

                // Convert BGR to HSV
                cvtColor(bgr_frame, hsv_frame, CV_BGR2HSV);

                imshow(windowName, bgr_frame);
                waitKey(1);
            }

#define min_f(x,y) ( ((x) < (y)) ? (x) : (y) )
#define max_f(x,y) ( ((x) > (y)) ? (x) : (y) )
            hsv_color curr = hsv_frame.at<hsv_color>(click_y, click_x);
            min_color.h = min_f(min_color.h, curr.h);
            min_color.s = min_f(min_color.s, curr.s);
            min_color.v = min_f(min_color.v, curr.v);
            max_color.h = max_f(max_color.h, curr.h);
            max_color.s = max_f(max_color.s, curr.s);
            max_color.v = max_f(max_color.v, curr.v);
#undef minf
#undef maxf
        }

        std::cout << "Min = " << min_color << std::endl;
        std::cout << "Max = " << max_color << std::endl;

        // Initialize the car location with the last click (it should be close
        // enough)
        car_info[car].car_x = click_x;
        car_info[car].car_y = click_y;
    }

    // Get rid of the calibration window
    cvDestroyWindow(windowName);
#endif

    car_info[0].min_color.h = 15;
    car_info[0].min_color.s = 50;
    car_info[0].min_color.v = 150;
    car_info[0].max_color.h = 35;
    car_info[0].max_color.s = 150;
    car_info[0].max_color.v = 255;
    car_info[1].min_color.h = 80;
    car_info[1].min_color.s = 100;
    car_info[1].min_color.v = 150;
    car_info[1].max_color.h = 100;
    car_info[1].max_color.s = 255;
    car_info[1].max_color.v = 255;
}

int main()
{
    init_stuff();
    cap_thr(NULL);
}

/**************************************/
/* Python exported functions and shit */
/**************************************/

#include <boost/python.hpp>
using namespace boost::python;

static int already_initted=0;
void init_tracker()
{
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

void destroy_tracker()
{
    exit(0);
}

void ask_for_click()
{
    need_click_xy = 1;
}

tuple get_click_loc()
{
    tuple xy;

    /* Return click_loc or NULL if haven't
       clicked yet */
    pthread_mutex_lock(&click_mx);
    if (need_click_xy)
        xy = tuple();
    else
        xy = make_tuple(click_x, click_y);
    pthread_mutex_unlock(&click_mx);

    return xy;
}

tuple get_car_loc(int car)
{
    float x=-1;
    float y=-1;
    tuple xy;

    pthread_mutex_lock(&car_info[car].loc_mx);
    if (car_info[car].car_found) {
        x = car_info[car].car_x;
        y = car_info[car].car_y;
        xy = make_tuple(x,y);
    } else {
        xy = tuple();
    }
    pthread_mutex_unlock(&car_info[car].loc_mx);

    printf("(%f, %f)\n", x, y);
    if (x == -1) {
        assert(y == -1);
        return tuple();
    }
    return xy;
}

void suppress_display()
{
   hide_disp = 1;
}

BOOST_PYTHON_MODULE(MOGBlob)
{
    def("init_tracker", init_tracker);
    def("destroy_tracker", destroy_tracker);
    def("get_car_loc", get_car_loc);
    def("ask_for_click", ask_for_click);
    def("suppress_display", suppress_display);
    def("get_click_loc", get_click_loc);
}

