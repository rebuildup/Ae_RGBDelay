#pragma once

#ifndef RGBDELAY_H
#define RGBDELAY_H

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
    typedef unsigned short PixelType;
    #include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "RGBDelay_Strings.h"

/* Versioning information */
#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

#define DELAY_MIN   -100
#define DELAY_MAX   100
#define DELAY_DFLT  0

enum {
    RGBDELAY_INPUT = 0,
    RGBDELAY_RED_X,
    RGBDELAY_RED_Y,
    RGBDELAY_GREEN_X,
    RGBDELAY_GREEN_Y,
    RGBDELAY_BLUE_X,
    RGBDELAY_BLUE_Y,
    RGBDELAY_NUM_PARAMS
};

enum {
    RED_X_DISK_ID = 1,
    RED_Y_DISK_ID,
    GREEN_X_DISK_ID,
    GREEN_Y_DISK_ID,
    BLUE_X_DISK_ID,
    BLUE_Y_DISK_ID
};

extern "C" {
    DllExport
    PF_Err
    EffectMain(
        PF_Cmd          cmd,
        PF_InData       *in_data,
        PF_OutData      *out_data,
        PF_ParamDef     *params[],
        PF_LayerDef     *output,
        void            *extra);
}

#endif // RGBDELAY_H
