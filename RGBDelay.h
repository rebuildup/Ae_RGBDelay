#pragma once

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
    // Use a unique name to avoid collision with After Effects SDK types
    typedef unsigned short RGBDelay_WinReservedType;
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

/* Define PF_TABLE_BITS before including AEFX_ChannelDepthTpl.h */
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "RGBDelay_Strings.h"

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

#define RGBDELAY_AMOUNT_MIN   -100
#define RGBDELAY_AMOUNT_MAX   100
#define RGBDELAY_AMOUNT_DFLT  0

enum {
    RGBDELAY_INPUT = 0,
    RGBDELAY_RED_DELAY,
    RGBDELAY_GREEN_DELAY,
    RGBDELAY_BLUE_DELAY,
    RGBDELAY_NUM_PARAMS
};

enum {
    RED_DELAY_DISK_ID = 1,
    GREEN_DELAY_DISK_ID,
    BLUE_DELAY_DISK_ID
};

// Plugin identification constants
#define RGBDELAY_MATCH_NAME    "361do_RGBDelay"
#define RGBDELAY_CATEGORY       "361do_plugins"
#define RGBDELAY_NAME           "RGBDelay"
#define RGBDELAY_SUPPORT_URL    "https://github.com/rebuildup/Ae_RGBDelay"

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

    DllExport PF_Err PluginDataEntryFunction2(
        PF_PluginDataPtr inPtr,
        PF_PluginDataCB2 inPluginDataCallBackPtr,
        SPBasicSuite *inSPBasicSuitePtr,
        const char *inHostName,
        const char *inHostVersion);
}
