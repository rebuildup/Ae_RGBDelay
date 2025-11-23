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

// Define Smart Render structs locally to avoid missing header issues
typedef struct PF_SmartRenderCallbacks_Local {
    PF_Err (*checkout_layer_pixels)(PF_ProgPtr effect_ref, PF_ParamIndex index, PF_EffectWorld **pixels);
    PF_Err (*checkout_output)(PF_ProgPtr effect_ref, PF_EffectWorld **output);
    PF_Err (*checkin_layer_pixels)(PF_ProgPtr effect_ref, PF_ParamIndex index);
    PF_Err (*is_layer_pixel_data_valid)(PF_ProgPtr effect_ref, PF_ParamIndex index, PF_Boolean *valid);
} PF_SmartRenderCallbacks_Local;

typedef struct PF_SmartRenderExtra_Local {
    PF_SmartRenderCallbacks_Local *cb;
    void *unused;
} PF_SmartRenderExtra_Local;

typedef struct PF_PreRenderInput_Local {
    PF_RenderRequest output_request;
    short bit_depth;
    void *unused;
} PF_PreRenderInput_Local;

typedef struct PF_PreRenderOutput_Local {
    PF_LRect result_rect;
    PF_LRect max_result_rect;
    void *unused;
} PF_PreRenderOutput_Local;

typedef struct PF_PreRenderCallbacks_Local {
    PF_Err (*checkout_layer)(PF_ProgPtr effect_ref, PF_ParamIndex index, PF_ParamIndex req_index, const PF_RenderRequest *req, A_long current_time, A_long time_step, A_u_long time_scale, PF_CheckoutResult *result);
    PF_Err (*checkout_layer_pixels)(PF_ProgPtr effect_ref, PF_ParamIndex index, PF_ParamIndex req_index, const PF_RenderRequest *req, A_long current_time, A_long time_step, A_u_long time_scale, PF_EffectWorld **pixels);
} PF_PreRenderCallbacks_Local;

typedef struct PF_PreRenderExtra_Local {
    PF_PreRenderCallbacks_Local *cb;
    PF_PreRenderInput_Local *input;
    PF_PreRenderOutput_Local *output;
    void *unused;
} PF_PreRenderExtra_Local;

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
