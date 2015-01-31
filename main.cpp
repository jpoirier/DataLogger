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

static const string currentDateTime(bool useDash);
static void writeFileProlog(string t);
static void writeFileEpilog(void);
static void writeData(double lat, double lon, double alt, const string t);
static int CommandHandler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
static void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon);
static void HandleKeyCallback(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                              char inVirtualKey, void* inRefcon, int losingFocus);
static int HandleMouseCallback(XPLMWindowID inWindowID, int x, int y,
                               XPLMMouseStatus inMouse, void* inRefcon);
static float FlightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop,
                         int inCounter, void* inRefcon);


// To define, pass -DVERSION=vX.Y.X when building
#ifndef VERSION
#define VERSION "vUNKNOWN"
#endif

// sigh, two levels of macros are needed to stringify
// the result  of expansion of a macro argument
#define STR(v) "DataRecorder " #v  " " __DATE__ " (jdpoirier@gmail.com)\0"
#define DESC(v) STR(v)

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#pragma message (DESC(VERSION))


static XPLMWindowID gDataRecWindow = NULL;
static bool gPluginEnabled = false;
static int gPlaneLoaded = 0;

// FIXME: how to set/change the callback frequency
// -1.0 == every frame, otherwise a time interval
static const float FL_CB_INTERVAL = 0.020;
static bool gPTT_On = false;
// static bool gPilotEdgePlugin = false;

#define WINDOW_WIDTH (120)
#define WINDOW_HEIGHT (30)
static int gRecWinPosX;
static int gRecWinPosY;
static int gLastMouseX;
static int gLastMouseY;

// general & misc
enum {
    PLUGIN_PLANE_ID = 0
    ,CMD_CONTACT_ATC
    ,DATARECORDER_WINDOW
};

// Command Refs
// #define sCONTACT_ATC "sim/operation/contact_atc"
// #define PILOTEDGE_SIG "com.pilotedge.plugin.xplane"

// XPLMDataRef avionics_power_on_dataref;
// XPLMDataRef audio_selection_com1_dataref;
// XPLMDataRef audio_selection_com2_dataref;

// XPLMDataRef pilotedge_rx_status_dataref = NULL;
// XPLMDataRef pilotedge_tx_status_dataref = NULL;
// XPLMDataRef pilotedge_connected_dataref = NULL;

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
    // const char *ptr = file.c_str() ;
    // LPRINTF(ptr);
    // LPRINTF("\n@@@@the file\n");
    gFd.open(file, ofstream::app); // creates the file if it doesn't exist
    if (!gFd.is_open()) {
        LPRINTF("DataRecorder Plugin: startup error, unable to open the output file...\n");
        return PROCESSED_EVENT;
    }
    writeFileProlog(t);

    lat_dref = XPLMFindDataRef("sim/flightmodel/position/latitude");
    lon_dref = XPLMFindDataRef("sim/flightmodel/position/longitude");
    alt_dref = XPLMFindDataRef("sim/flightmodel/position/elevation");  // meters

    // XPLMCommandRef cmd_ref;
    // cmd_ref = XPLMCreateCommand(sCONTACT_ATC, "Contact ATC");
    // XPLMRegisterCommandHandler(cmd_ref,
    //                            CommandHandler,
    //                            CMD_HNDLR_EPILOG,
    //                            (void*)CMD_CONTACT_ATC);

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, FL_CB_INTERVAL, NULL);

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
    gFd << "<gpx version=\"1.1\">\n";
    gFd << "<metadata>\n";
    gFd << "<time>" << t << "</time>\n";
    gFd << "</metadata>\n";
    gFd << "<trk><name>DataRecorder plugin</name><number>1</number><trkseg>\n";
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
    // TODO: elevation probably needs to be in meters
    // <trkpt lat="46.57608333" lon="8.89241667"><ele>2376</ele></trkpt>
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
 * Returns the current date and GMT.
 *
 * @return YYYY-MM-DDTHH:MM:SSZ, e.g. 2015-01-30T18:46:02Z.
 */
const string currentDateTime(bool useDash)
{
    time_t now = time(0);
    char buf[24];
    // strftime(buf, sizeof(buf), "%FT%XZ", gmtime(&now));
    // strftime(buf, sizeof(buf), "%FT%XZ", localtime(&now));
    if (useDash)
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", gmtime(&now));
    else
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
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
        LPRINTF("DataRecorder Plugin: recording disabled...\n");
        return 1.0;
    }
    // LPRINTF("DataRecorder Plugin: FlightLoopCallback writing data...\n");
    writeData(XPLMGetDataf(lat_dref),
                XPLMGetDataf(lon_dref),
                XPLMGetDataf(alt_dref),
                currentDateTime(false));
    return 1.0;
}

/**
 *
 */
int CommandHandler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon)
{
//    if ((gFlCbCnt % PANEL_CHECK_INTERVAL) == 0) {
//    }
//    if (!gPluginEnabled) {
//        return IGNORED_EVENT;
//    }

    switch (reinterpret_cast<size_t>(inRefcon)) {
    case CMD_CONTACT_ATC:
        switch (inPhase) {
        case xplm_CommandBegin:
        case xplm_CommandContinue:
            gPTT_On = true;
            break;
        case xplm_CommandEnd:
            gPTT_On = false;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return IGNORED_EVENT;
}

/**
 *
 */
PLUGIN_API void XPluginStop(void)
{
    gPluginEnabled = false;
    gRecording = false;
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
    // static char str2[100];
    // RGB: White [1.0, 1.0, 1.0], Lime Green [0.0, 1.0, 0.0]
    static float datarecorder_color[] = {0.0, 1.0, 0.0};

    if (inWindowID != gDataRecWindow)
        return;

    int left;
    int top;
    int right;
    int bottom;
    // int rx_status;
    // int tx_status;
    // char* connected;

    // XXX: are inWindowIDs our XPLMCreateWindow return pointers
    XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
    // printf("DataRecorder, gDataRecWindow: %p, inWindowID: %p, left:%d, right:%d, top:%d, bottom:%d\n",
    //     gDataRecWindow, inWindowID, left, right, top, bottom);
    XPLMDrawTranslucentDarkBox(left, top, right, bottom);

    // if (!gPilotEdgePlugin) {
    //     if ((XPLMFindPluginBySignature(PILOTEDGE_SIG)) != XPLM_NO_PLUGIN_ID) {
    //         gPilotEdgePlugin = true;
    //         pilotedge_rx_status_dataref = XPLMFindDataRef("pilotedge/radio/rx_status");
    //         pilotedge_tx_status_dataref = XPLMFindDataRef("pilotedge/radio/tx_status");
    //         pilotedge_connected_dataref = XPLMFindDataRef("pilotedge/status/connected");
    //     }
    // }

    switch (reinterpret_cast<size_t>(inRefcon)) {
    case DATARECORDER_WINDOW:
        // rx_status = (pilotedge_rx_status_dataref ? XPLMGetDatai(pilotedge_rx_status_dataref) : false) ? 1 : 0;
        // tx_status = (pilotedge_tx_status_dataref ? XPLMGetDatai(pilotedge_tx_status_dataref) : false) ? 1 : 0;
        // connected = (pilotedge_connected_dataref ? XPLMGetDatai(pilotedge_connected_dataref) : false) ? (char*)"YES" : (char*)"NO ";
        // sprintf(str1, "[PilotEdge] Connected: %s \t\t\tTX: %d\t\t\tRX: %d",
        //         connected,
        //         tx_status,
        //         rx_status);

        // sprintf(str2,"%s\t\t\tCOM1: %d\t\t\tCOM2: %d",
        //         (char*)(gPTT_On ? "PTT: ON " : "PTT: OFF"),
        //         XPLMGetDatai(audio_selection_com1_dataref),
        //         XPLMGetDatai(audio_selection_com2_dataref));

        // text to window, NULL indicates no word wrap
        // XPLMDrawString(datarecorder_color,
        //                left+4,
        //                top-20,
        //                str1,
        //                NULL,
        //                xplmFont_Basic);

        // XPLMDrawString(datarecorder_color,
        //                left+4,
        //                top-40,
        //                str2,
        //                NULL,
        //                xplmFont_Basic);
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
 #define COMMS_UNCHANGED    (0)
 #define COM1_CHANGED       (1)
 #define COM2_CHANGED       (2)
 #define COMM_UNSELECTED    (0)
 #define COMM_SELECTED      (1)
int HandleMouseCallback(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse, void* inRefcon)
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
        // this event fires while xplm_MouseDown
        // and whether the window is being dragged or not
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
            // int com1 = XPLMGetDatai(audio_selection_com1_dataref);
            // int com2 = XPLMGetDatai(audio_selection_com2_dataref);

            // if (com1 && com2 && com_changed) {
            //     switch (com_changed) {
            //     case COM1_CHANGED:
            //         XPLMSetDatai(audio_selection_com1_dataref, COMM_UNSELECTED);
            //         break;
            //     case COM2_CHANGED:
            //         XPLMSetDatai(audio_selection_com2_dataref, COMM_UNSELECTED);
            //         break;
            //     default:
            //         break;
            //     }
            //     com_changed = COMMS_UNCHANGED;
            // } else if (!com1 && com2) {
            //     com_changed = COM1_CHANGED;
            //     XPLMSetDatai(audio_selection_com1_dataref, COMM_SELECTED);
            // }  else if (com1 && !com2) {
            //     com_changed = COM2_CHANGED;
            //     XPLMSetDatai(audio_selection_com2_dataref, COMM_SELECTED);
            // }
        }
        break;
    } // switch (inMouse)
    return PROCESSED_EVENT;
}
