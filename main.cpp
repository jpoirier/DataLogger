// Copyright (c) 2015 Joseph D Poirier
// Distributable under the terms of The Simplified BSD License
// that can be found in the LICENSE file.

#ifdef _WIN32 /* this is set for 64 bit as well */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#ifdef _APPLE_
 #pragma clang diagnostic ignored "-Wall"
 #include <gl.h>
#else
 //#include <GL/gl.h>
#endif

// #include <stdlib.h>
// #include <stdio.h>
// #include <math.h>
#include <string>
#include <time.h>
// #include <iostream>
#include <fstream>
#include <atomic>
#include <cstring>

#include "./SDK/CHeaders/XPLM/XPLMPlugin.h"
#include "./SDK/CHeaders/XPLM/XPLMProcessing.h"
#include "./SDK/CHeaders/XPLM/XPLMDataAccess.h"
#include "./SDK/CHeaders/XPLM/XPLMUtilities.h"
#include "./SDK/CHeaders/XPLM/XPLMDisplay.h"
#include "./SDK/CHeaders/XPLM/XPLMGraphics.h"

// #include "readerwriterqueue.h"
#include "./include/defs.h"
#include "./include/main.h"


using namespace std;
// using namespace moodycamel;

static void enableLogging(void);
static void disableLogging(void);
static bool openLogFile(void);
static void closeLogFile(void);
static string const currentDateTime(bool useDash);
static void writeFileProlog(string t);
static void writeFileEpilog(void);
static void writeData(double lat, double lon, double alt, const string t);
static void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon);
static void HandleKeyCallback(XPLMWindowID inWindowID, char inKey,
                              XPLMKeyFlags inFlags, char inVirtualKey,
                              void* inRefcon, int losingFocus);
static int HandleMouseCallback(XPLMWindowID inWindowID, int x, int y,
                               XPLMMouseStatus inMouse, void* inRefcon);
static float LoggerCallback(float inElapsedSinceLastCall,
                            float inElapsedTimeSinceLastFlightLoop,
                            int inCounter, void* inRefcon);
static float StatusCheckCallback(float inElapsedSinceLastCall,
                                 float inElapsedTimeSinceLastFlightLoop,
                                 int inCounter, void* inRefcon);

// To define, pass -DVERSION=vX.Y.X when building,
// e.g. in a make file
#ifndef VERSION
#define VERSION "vUNKNOWN"
#endif

// sigh. two levels of macros needed to stringify
// the result of expansion of a macro argument
#define STR(v) "DataRecorder " #v  " " __DATE__ " (jdpoirier@gmail.com)\0"
#define DESC(v) STR(v)

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#pragma message (DESC(VERSION))


static XPLMWindowID gDataRecWindow = NULL;
static atomic<bool> gPluginEnabled(false);
// static atomic<int> gPlaneLoaded(0);

// time interval > 0.0 (no callback) > flight loop frame rate
static atomic<float> gFlCbInterval(0.100f); // 10Hz update rate?

#define WINDOW_WIDTH (180)
#define WINDOW_HEIGHT (30)
static int gRecWinPosX;
static int gRecWinPosY;
static int gLastMouseX;
static int gLastMouseY;

// general & misc
enum {
    PLUGIN_PLANE_ID = 0
    ,DATARECORDER_WINDOW
};

XPLMDataRef gs_dref = NULL;
XPLMDataRef lat_dref = NULL;
XPLMDataRef lon_dref = NULL;
XPLMDataRef alt_dref = NULL;

static atomic<bool> gLogging(false);
static atomic<bool> gFileOpenErr(false);
static atomic<bool> gFlashUI(false);
static ofstream gFd;

XPLMDataRef panel_visible_win_t_dataref;

/**
 *
 */
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc)
{
    LPRINTF("DataRecorder Plugin: XPluginStart\n");
    strcpy(outName, "DataRecorder");
    strcpy(outSig , "jdp.data.recorder");
    strcpy(outDesc, DESC(VERSION));

    gs_dref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    lat_dref = XPLMFindDataRef("sim/flightmodel/position/latitude");
    lon_dref = XPLMFindDataRef("sim/flightmodel/position/longitude");
    alt_dref = XPLMFindDataRef("sim/flightmodel/position/elevation");
    XPLMRegisterFlightLoopCallback(StatusCheckCallback, 5.0, NULL);
    panel_visible_win_t_dataref = XPLMFindDataRef("sim/graphics/view/panel_visible_win_t");
    int top = (int)XPLMGetDataf(panel_visible_win_t_dataref);
    gRecWinPosX = 0;
    gRecWinPosY = top - 150;
    gDataRecWindow = XPLMCreateWindow(gRecWinPosX,                  // left
                                      gRecWinPosY,                  // top
                                      gRecWinPosX+WINDOW_WIDTH,     // right
                                      gRecWinPosY-WINDOW_HEIGHT,    // bottom
                                      true,                         // is visible
                                      DrawWindowCallback,
                                      HandleKeyCallback,
                                      HandleMouseCallback,
                                      (void*)DATARECORDER_WINDOW);  // Refcon
    LPRINTF("DataRecorder Plugin: startup completed\n");
    return PROCESSED_EVENT;
}

/**
 *
 */
bool openLogFile(void)
{
    if (gFd.is_open())
        closeLogFile();

    string t = currentDateTime(true);
    string file = string("DataRecord-") + t + string(".gpx");
    // const char *ptr = file.c_str(); LPRINTF(ptr);

    gFd.open(file, ofstream::app); // creates the file if it doesn't exist
    if (!gFd.is_open()) {
        LPRINTF("DataRecorder Plugin: startup error, unable to open the output file...\n");
        return false;
    }
    writeFileProlog(t);
    return true;
}

/**
 *
 */
void closeLogFile(void)
{
    if (gFd.is_open()) {
        writeFileEpilog();
        gFd.close();
    }
}

/**
 *
 */
void writeFileProlog(string t)
{
    gFd << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    gFd << "<gpx version=\"1.0\">\n";
    gFd << "<metadata>\n";
    gFd << "<time>" << t << "</time>\n";
    gFd << "</metadata>\n";
    gFd << "<trk><name>DataRecorder plugin</name><trkseg>\n";
}

/**
 *
 */
void writeFileEpilog(void)
{
    gFd << "</trkseg></trk>\n";
    gFd << "</gpx>\n";
}

/**
 *
 */
void writeData(double lat, double lon, double alt, const string t)
{
    static double lat_ = 0.0;
    static double lon_ = 0.0;
    static double alt_ = 0.0;

    if (lat == lat_ && lon == lon_ && alt == alt_)
        return;

    lat_ = lat;
    lon_ = lon;
    alt_ = alt;

    // <trkpt lat="46.57608333" lon="8.89241667"><ele>2376.640205</ele></trkpt>
    gFd << "<trkpt lat=\""
        << to_string(lat)
        << "\" lon=\""
        << to_string(lon)
        << "\"><ele>"
        << to_string(alt)
        // << to_string(static_cast<int>(alt))
        // << to_string(static_cast<int>(alt/METERS_PER_FOOT))
        << "</ele><time>"
        << t
        << "</time></trkpt>\n";
}

/**
 * Returns the current date and Zulu time.
 *
 * @return
 *      if useDash: YYYY-MM-DDTHH-MM-SSZ
 *      else:       YYYY-MM-DDTHH:MM:SSZ
 */
string const currentDateTime(bool useDash)
{
    time_t now = time(0);
    string buf(22,  '\0');
    // strftime(buf, sizeof(buf), "%FT%XZ", gmtime(&now));
    // strftime(buf, sizeof(buf), "%FT%XZ", localtime(&now));
    if (useDash)
        strftime((char*)buf.c_str(), buf.length(), "%Y-%m-%dT%H-%M-%SZ", gmtime(&now));
    else
        strftime((char*)buf.c_str(),  buf.length(), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    return buf.substr(0, 20);
}

/**
 *
 */
#define UI_FLASHTIME (20)
#define GRNDSPEED_THRESH (2)
#define MOVING_THRESH (5)
float StatusCheckCallback(float inElapsedSinceLastCall,
                          float inElapsedTimeSinceLastFlightLoop,
                          int inCounter, void* inRefcon)
{
    static int cnt = 0;
    static bool doGrndCheck = true;

    if (!gPluginEnabled.load()) {
        // LPRINTF("DataRecorder Plugin: StatusCheckCallback...\n");
        return 10.0;
    }

    if (!gLogging.load()) {
        if (gFlashUI.load()) {
            cnt += 1;
            if (cnt >= UI_FLASHTIME) {
                cnt = 0;
                gFlashUI.store(false);
            }
        } else if (doGrndCheck) {
            int gs = (int)XPLMGetDataf(gs_dref);
            if (gs > GRNDSPEED_THRESH) {
                cnt += 1;
            } else {
                cnt = 0;
                doGrndCheck = true;
                gFlashUI.store(false);
            }
            if (cnt >= MOVING_THRESH) {
                cnt = 0;
                gFlashUI.store(true);
                doGrndCheck = false;
            }
        }
        return 1.0;
    }

    cnt = 0;
    doGrndCheck = true;
    return 10.0;
}

/**
 *
 */
float LoggerCallback(float inElapsedSinceLastCall,
                     float inElapsedTimeSinceLastFlightLoop,
                     int inCounter, void* inRefcon)
{
    if (!gPluginEnabled.load() || !gLogging.load()) {
        return 0.0;  // disable the callback
    }
    // LPRINTF("DataRecorder Plugin: LoggerCallback writing data...\n");
    writeData(XPLMGetDataf(lat_dref),
                XPLMGetDataf(lon_dref),
                XPLMGetDataf(alt_dref),
                currentDateTime(false));
    return gFlCbInterval.load();
}

/**
 *
 */
void enableLogging(void){
    if (openLogFile()) {
        gLogging.store(true);
        XPLMRegisterFlightLoopCallback(LoggerCallback, -1.0, NULL);
    } else {
        gFileOpenErr.store(true);
    }
}

/**
 *
 */
void disableLogging(void)
{
    gLogging.store(false);
    XPLMUnregisterFlightLoopCallback(LoggerCallback, NULL);
    closeLogFile();
}

/**
 *
 */
PLUGIN_API void XPluginStop(void)
{
    gPluginEnabled.store(false);
    gLogging.store(false);
    XPLMUnregisterFlightLoopCallback(LoggerCallback, NULL);
    closeLogFile();
    XPLMUnregisterFlightLoopCallback(StatusCheckCallback, NULL);
    LPRINTF("DataRecorder Plugin: XPluginStop\n");
}

/**
 *
 */
PLUGIN_API void XPluginDisable(void)
{
    gPluginEnabled.store(false);
    disableLogging();
    LPRINTF("DataRecorder Plugin: XPluginDisable\n");
}

/**
 *
 */
PLUGIN_API int XPluginEnable(void)
{
    gPluginEnabled.store(true);
     LPRINTF("DataRecorder Plugin: XPluginEnable\n");
    return PROCESSED_EVENT;
}

/**
 *
 */
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, long inMsg, void* inParam)
{
    if (inFrom == XPLM_PLUGIN_XPLANE) {
        // size_t inparam = reinterpret_cast<size_t>(inParam);
        switch (inMsg) {
        case XPLM_MSG_PLANE_LOADED:
            // gPlaneLoaded.store();
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_PLANE_LOADED\n");
            break;
        case XPLM_MSG_AIRPORT_LOADED:
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_AIRPORT_LOADED\n");
            break;
        case XPLM_MSG_SCENERY_LOADED:
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_SCENERY_LOADED\n");
            break;
        case XPLM_MSG_AIRPLANE_COUNT_CHANGED:
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_AIRPLANE_COUNT_CHANGED\n");
            break;
        case XPLM_MSG_PLANE_CRASHED:
            // XXX: system state and procedure, what's difference between
            // an unloaded and crashed plane?
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_PLANE_CRASHED\n");
            break;
        case XPLM_MSG_PLANE_UNLOADED:
            // gPlaneLoaded.store();
            // LPRINTF("DataRecorder Plugin: XPluginReceiveMessage XPLM_MSG_PLANE_UNLOADED\n");
            break;
        default:
            // unknown, anything to do?
            break;
        } // switch (inMsg)
    } // if (inFrom == XPLM_PLUGIN_XPLANE)
}

/**
 *
 */
#define TXT_ONOFF_THRESH (10)
#define FILEERR_OFF_THRESH (120)
void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
    // RGB: White [1.0, 1.0, 1.0], Lime Green [0.0, 1.0, 0.0]
    static float datarecorder_color[] = {0.0, 1.0, 0.0};
    static int errCnt = 0;
    static bool msg_on = false;
    static int cnt = 0;
    if (inWindowID != gDataRecWindow)
        return;

    int left;
    int top;
    int right;
    int bottom;

    // XXX: are inWindowIDs our XPLMCreateWindow return pointers
    XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
    XPLMDrawTranslucentDarkBox(left, top, right, bottom);

    string str1;
    switch (reinterpret_cast<size_t>(inRefcon)) {
    case DATARECORDER_WINDOW:
        switch (gLogging.load()) {
        case true:
            str1 = "Logging - Enabled...";
            break;
        case false:
            if (gFileOpenErr.load()) {
                errCnt += 1;
                str1 = "Logging - Error opening file";
                if (errCnt >= FILEERR_OFF_THRESH) {
                    errCnt = 0;
                    gFileOpenErr.store(false);
                }
            } else {
                if (gFlashUI.load()) {
                    cnt += 1;
                    if (msg_on) {
                        str1 = "Logging - Click to enable...";
                    } else {
                        str1 = "Logging - ";
                    }
                    if (cnt >= TXT_ONOFF_THRESH) {
                        cnt = 0;
                        msg_on = msg_on ? false : true;
                    }
                } else {
                    cnt = 0;
                    str1 = "Logging - Click to enable...";
                }
            }
            break;
        }
        XPLMDrawString(datarecorder_color,
                       left+4,
                       top-20,
                       (char*)str1.c_str(),
                       NULL,
                       xplmFont_Basic);
        break;
    default:
        break;
    }
}

/**
 *
 */
void HandleKeyCallback(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                       char inVirtualKey, void* inRefcon, int losingFocus)
{
    if (inWindowID != gDataRecWindow)
        return;
}

/*
 *
 *
 */
int HandleMouseCallback(XPLMWindowID inWindowID, int x, int y,
                        XPLMMouseStatus inMouse, void* inRefcon)
{
    // static int com_changed = COMMS_UNCHANGED;
    static int MouseDownX;
    static int MouseDownY;

    if (inWindowID != gDataRecWindow)
        return IGNORED_EVENT;

    switch (inMouse) {
    case xplm_MouseDown:
        // if ((x >= gRecWinPosX+WINDOW_WIDTH-8) &&
        //     (x <= gRecWinPosX+WINDOW_WIDTH) &&
        //     (y <= gRecWinPosY) && (y >= gRecWinPosY-8)) {
        //         windowCloseRequest = 1;
        //     } else {
                MouseDownX = gLastMouseX = x;
                MouseDownY = gLastMouseY = y;
        // }
        break;
    case xplm_MouseDrag:
        // this event fires while xplm_MouseDown is active
        // and whether or not the window is being dragged
        gRecWinPosX += (x - gLastMouseX);
        gRecWinPosY += (y - gLastMouseY);
        XPLMSetWindowGeometry(gDataRecWindow,
                              gRecWinPosX,
                              gRecWinPosY,
                              gRecWinPosX+WINDOW_WIDTH,
                              gRecWinPosY-WINDOW_HEIGHT);
        gLastMouseX = x;
        gLastMouseY = y;
        break;
    case xplm_MouseUp:
        // Ignore mouse-clicks for a short time
        // when there was a previous open file error.
        if (gFileOpenErr.load())
            break;
        if (MouseDownX == x || MouseDownY == y) {
            switch (gLogging.load()) {
            case true:
                disableLogging();
                break;
            case false:
                enableLogging();
                break;
            }
        }
        break;
    } // switch (inMouse)
    return PROCESSED_EVENT;
}
