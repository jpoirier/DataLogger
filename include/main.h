// Copyright (c) 2014 Joseph D Poirier
// Distributable under the terms of The New BSD License
// that can be found in the LICENSE file.

#ifndef COMMVIEWER_H
#define COMMVIEWER_H


//#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __TESTING__
    #include "XPLMDefs.h"
    PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc);
    PLUGIN_API void XPluginStop(void);
    PLUGIN_API void XPluginDisable(void);
    PLUGIN_API int XPluginEnable(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* COMMVIEW_H */
