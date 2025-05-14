/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/*******************************************************************/

/*
    RGBDelay.h
*/

#pragma once

#ifndef RGBDELAY_H
#define RGBDELAY_H

#define PF_DEEP_COLOR_AWARE 1    // make sure we get 16bpc pixels;
// AE_Effect.h checks for this.
#include "AEConfig.h"
#include "entry.h"
#ifdef AE_OS_WIN
#include "string.h"
#endif
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#include "RGBDelay_Strings.h"

#ifdef AE_OS_WIN
#include <Windows.h>
#endif

/* Versioning information */

#define    MAJOR_VERSION    1
#define    MINOR_VERSION    0
#define    BUG_VERSION        0
#define    STAGE_VERSION    PF_Stage_DEVELOP
#define    BUILD_VERSION    1

/* Parameter defaults */
#define    RGBDELAY_AMOUNT_MIN    0
#define    RGBDELAY_AMOUNT_MAX    100
#define    RGBDELAY_AMOUNT_DFLT    50

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

extern "C" {

    DllExport
        PF_Err
        EffectMain(
            PF_Cmd            cmd,
            PF_InData* in_data,
            PF_OutData* out_data,
            PF_ParamDef* params[],
            PF_LayerDef* output,
            void* extra);

}

#endif // RGBDELAY_H