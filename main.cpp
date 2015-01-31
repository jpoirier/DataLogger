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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>

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

static string const currentDateTime(bool useDash);
static void writeFileProlog(string t);
static void writeFileEpilog(void);
static void writeData(double lat, double lon, double alt, const string t);
static void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon);
static void HandleKeyCallback(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                              char inVirtualKey, void* inRefcon, int losingFocus);
static int HandleMouseCallback(XPLMWindowID inWindowID, int x, int y,
                               XPLMMouseStatus inMouse, void* inRefcon);
static float FlightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop,
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
static bool gPluginEnabled = false;
static int gPlaneLoaded = 0;

// time interval > 0 (no callback) > flight loop frame rate
static float gFlCbInterval = (float)0.100;

#define WINDOW_WIDTH (120)
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

XPLMDataRef lat_dref = NULL;
XPLMDataRef lon_dref = NULL;
XPLMDataRef alt_dref = NULL;

static bool gRecording = false;
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

    string t = currentDateTime(true);
    string file = string("DataRecord-") + t + string(".gpx");
    // const char *ptr = file.c_str(); LPRINTF(ptr);
    gFd.open(file, ofstream::app); // creates the file if it doesn't exist
    if (!gFd.is_open()) {
        LPRINTF("DataRecorder Plugin: startup error, unable to open the output file...\n");
        return PROCESSED_EVENT;
    }
    writeFileProlog(t);

    lat_dref = XPLMFindDataRef("sim/flightmodel/position/latitude");
    lon_dref = XPLMFindDataRef("sim/flightmodel/position/longitude");
    alt_dref = XPLMFindDataRef("sim/flightmodel/position/elevation");

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, gFlCbInterval, NULL);

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
    string buf(20,  ' ');
    // strftime(buf, sizeof(buf), "%FT%XZ", gmtime(&now));
    // strftime(buf, sizeof(buf), "%FT%XZ", localtime(&now));
    if (useDash)
        strftime((char*)buf.c_str(), buf.length(), "%Y-%m-%dT%H-%M-%SZ", gmtime(&now));
    else
        strftime((char*)buf.c_str(),  buf.length(), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    return buf;
}

/**
 *
 */
float FlightLoopCallback(float inElapsedSinceLastCall,
                         float inElapsedTimeSinceLastFlightLoop,
                         int inCounter, void* inRefcon)
{
    if (!gPluginEnabled || !gRecording) {
        // LPRINTF("DataRecorder Plugin: recording disabled...\n");
        return 1.0;  // once per second when not recording
    }
    // LPRINTF("DataRecorder Plugin: FlightLoopCallback writing data...\n");
    writeData(XPLMGetDataf(lat_dref),
                XPLMGetDataf(lon_dref),
                XPLMGetDataf(alt_dref),
                currentDateTime(false));
    return gFlCbInterval; // 10Hz update rate?
}

/**
 *
 */
PLUGIN_API void XPluginStop(void)
{
    gPluginEnabled = gRecording = false;
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, NULL);
    if (gFd.is_open()) {
        writeFileEpilog();
        gFd.close();
    }
    LPRINTF("DataRecorder Plugin: XPluginStop\n");
}

/**
 *
 */
PLUGIN_API void XPluginDisable(void)
{
    gPluginEnabled = false;
    LPRINTF("DataRecorder Plugin: XPluginDisable\n");
}

/**
 *
 */
PLUGIN_API int XPluginEnable(void)
{
    gPluginEnabled = true;
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
            gPlaneLoaded = true;
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
            gRecording = false;
            break;
        case XPLM_MSG_PLANE_UNLOADED:
            gPlaneLoaded = false;
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
void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
    static char str1[100];
    // RGB: White [1.0, 1.0, 1.0], Lime Green [0.0, 1.0, 0.0]
    static float datarecorder_color[] = {0.0, 1.0, 0.0};

    if (inWindowID != gDataRecWindow)
        return;

    int left;
    int top;
    int right;
    int bottom;

    // XXX: are inWindowIDs our XPLMCreateWindow return pointers
    XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
    XPLMDrawTranslucentDarkBox(left, top, right, bottom);

    switch (reinterpret_cast<size_t>(inRefcon)) {
    case DATARECORDER_WINDOW:
        LPRINTF("DataRecorder Plugin: DrawWindowCallback...\n");
        sprintf(str1, "Data Recorder: %s", (gRecording ? (char*)"ON " : (char*)"OFF"));
        XPLMDrawString(datarecorder_color,
                       left+4,
                       top-20,
                       str1,
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
        if (MouseDownX == x || MouseDownY == y) {
            gRecording = gRecording ? false : true;
        }
        break;
    } // switch (inMouse)
    return PROCESSED_EVENT;
}
