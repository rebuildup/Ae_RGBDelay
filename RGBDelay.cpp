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
    RGBDelay.cpp
*/

#include "RGBDelay.h"
#include "RGBDelay_Strings.h"

#ifdef AE_OS_WIN
#include <Windows.h>
#endif

typedef struct {
    PF_ProgPtr    ref;
    A_long        delay_r;
    A_long        delay_g;
    A_long        delay_b;
    PF_PixelPtr   lastRow;
} rgbDelay_params;

// Function prototypes
static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output);

static PF_Err
GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output);

static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output);

static PF_Err
Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output);

static PF_Err
PreRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_PreRenderExtra* extra);

static PF_Err
SmartRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_SmartRenderExtra* extra);

static PF_Err
UserChangedParam(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    PF_UserChangedParamExtra* extra);

// Plugin's entry function
extern "C" DllExport
PF_Err PluginDataEntryFunction(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB inPluginDataCallBackPtr,
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT(
        inPtr,
        inPluginDataCallBackPtr,
        "RGBDelay", // Name
        "ADBE RGBDelay", // Match name
        "Hotkey lab.", // Category
        AE_RESERVED_INFO); // Reserved info

    return result;
}

// Main entry point for the effect
extern "C" DllExport
PF_Err EffectMain(
    PF_Cmd			cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
        case PF_Cmd_ABOUT:
            err = About(in_data, out_data, params, output);
            break;

        case PF_Cmd_GLOBAL_SETUP:
            err = GlobalSetup(in_data, out_data, params, output);
            break;

        case PF_Cmd_PARAMS_SETUP:
            err = ParamsSetup(in_data, out_data, params, output);
            break;

        case PF_Cmd_RENDER:
            err = Render(in_data, out_data, params, output);
            break;

        case PF_Cmd_SMART_PRE_RENDER:
            err = PreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
            break;

        case PF_Cmd_SMART_RENDER:
            err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
            break;

        case PF_Cmd_USER_CHANGED_PARAM:
            err = UserChangedParam(in_data, out_data, params, output, (PF_UserChangedParamExtra*)extra);
            break;
        }
    }
    catch (PF_Err& thrown_err) {
        err = thrown_err;
    }

    return err;
}

// About dialog function
static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "%s v%d.%d\r%s",
        STR(StrID_Name),
        MAJOR_VERSION,
        MINOR_VERSION,
        STR(StrID_Description));
    return PF_Err_NONE;
}

// Global setup function
static PF_Err
GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);

    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE |
        PF_OutFlag2_SUPPORTS_SMART_RENDER |
        PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

    return PF_Err_NONE;
}

// Setup parameters function
static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);

    // Red Delay Slider
    PF_ADD_SLIDER(STR(StrID_RedDelay_Param_Name),
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_DFLT,
        RED_DELAY_DISK_ID);

    // Green Delay Slider
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_GreenDelay_Param_Name),
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_DFLT,
        GREEN_DELAY_DISK_ID);

    // Blue Delay Slider
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_BlueDelay_Param_Name),
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_MIN,
        RGBDELAY_AMOUNT_MAX,
        RGBDELAY_AMOUNT_DFLT,
        BLUE_DELAY_DISK_ID);

    out_data->num_params = RGBDELAY_NUM_PARAMS;

    return err;
}

// Render function for 8-bit, 16-bit, and 32-bit imagery
static PF_Err
Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    // Get parameter values
    A_long delay_r = params[RGBDELAY_RED_DELAY]->u.sd.value;
    A_long delay_g = params[RGBDELAY_GREEN_DELAY]->u.sd.value;
    A_long delay_b = params[RGBDELAY_BLUE_DELAY]->u.sd.value;

    // Call appropriate pixel processing function depending on bit depth
    if (PF_WORLD_IS_DEEP(output)) {
        // 16-bit processing
        // Note: Implementation for 16-bit would go here
        // For now, just copy source to destination as a placeholder
        err = suites.WorldTransformSuite1()->copy(in_data->effect_ref, &params[RGBDELAY_INPUT]->u.ld, output, NULL, NULL);
    }
    else {
        // 8-bit processing
        // Note: Implementation for 8-bit would go here
        // For now, just copy source to destination as a placeholder
        err = suites.WorldTransformSuite1()->copy(in_data->effect_ref, &params[RGBDELAY_INPUT]->u.ld, output, NULL, NULL);
    }

    return err;
}

// Pre-render function for Smart Render pipeline
static PF_Err
PreRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_PreRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;
    PF_CheckoutResult in_result;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    // Define what we need from the host
    PF_RenderRequest req = {
        0,            // Set flags to 0 instead of PF_RenderFlag_NONE
        0             // Use 0 instead of PF_FIELD_FRAME 
    };

    // Checkout input buffer with correct parameters
    err = extra->cb->checkout_layer(in_data->effect_ref,
        RGBDELAY_INPUT,    // Which layer
        RGBDELAY_INPUT,    // Request param index
        &req,              // Pre-render request
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &in_result);

    if (!err) {
        // Get parameter values for smart rendering
        PF_ParamDef param_red, param_green, param_blue;
        AEFX_CLR_STRUCT(param_red);
        AEFX_CLR_STRUCT(param_green);
        AEFX_CLR_STRUCT(param_blue);

        err = PF_CHECKOUT_PARAM(in_data, RGBDELAY_RED_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &param_red);

        if (!err) {
            err = PF_CHECKOUT_PARAM(in_data, RGBDELAY_GREEN_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &param_green);
        }

        if (!err) {
            err = PF_CHECKOUT_PARAM(in_data, RGBDELAY_BLUE_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &param_blue);
        }

        if (!err) {
            // Create parameters
            rgbDelay_params params;
            params.ref = in_data->effect_ref;
            params.delay_r = param_red.u.sd.value;
            params.delay_g = param_green.u.sd.value;
            params.delay_b = param_blue.u.sd.value;

            // Allocate memory through the AE memory management suite
            PF_Handle params_handle = suites.HandleSuite1()->host_new_handle(sizeof(rgbDelay_params));
            if (params_handle) {
                rgbDelay_params* params_ptr = reinterpret_cast<rgbDelay_params*>(suites.HandleSuite1()->host_lock_handle(params_handle));
                if (params_ptr) {
                    // Copy the data
                    *params_ptr = params;
                    suites.HandleSuite1()->host_unlock_handle(params_handle);

                    // Store the handle in pre_render_data
                    extra->output->pre_render_data = params_handle;
                    extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
                }
                else {
                    suites.HandleSuite1()->host_dispose_handle(params_handle);
                    err = PF_Err_OUT_OF_MEMORY;
                }
            }
            else {
                err = PF_Err_OUT_OF_MEMORY;
            }
        }

        // Cleanup
        if (param_red.u.sd.value) PF_CHECKIN_PARAM(in_data, &param_red);
        if (param_green.u.sd.value) PF_CHECKIN_PARAM(in_data, &param_green);
        if (param_blue.u.sd.value) PF_CHECKIN_PARAM(in_data, &param_blue);
    }

    return err;
}

// Smart render function
static PF_Err
SmartRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_SmartRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    // Get pre-render data handle
    PF_Handle params_handle = reinterpret_cast<PF_Handle>(extra->input->pre_render_data);
    rgbDelay_params* params = NULL;

    // Lock the handle to access data
    if (params_handle) {
        params = reinterpret_cast<rgbDelay_params*>(suites.HandleSuite1()->host_lock_handle(params_handle));
    }

    // Checkout input & output buffers
    PF_EffectWorld* input_worldP = NULL;
    PF_EffectWorld* output_worldP = NULL;

    // Checkout layer
    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, RGBDELAY_INPUT, &input_worldP));

    // Checkout output buffer
    if (!err) {
        ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));
    }

    // Process pixels
    if (!err && input_worldP && output_worldP && params) {
        // Note: Actual pixel processing for RGB delay would go here
        // For now, just copy source to destination as a placeholder
        err = suites.WorldTransformSuite1()->copy(in_data->effect_ref, input_worldP, output_worldP, NULL, NULL);
    }

    // Cleanup - unlock and dispose of the handle
    if (params_handle) {
        if (params) {
            suites.HandleSuite1()->host_unlock_handle(params_handle);
        }
        suites.HandleSuite1()->host_dispose_handle(params_handle);
    }

    return err;
}

// Handler for parameter changes by the user
static PF_Err
UserChangedParam(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    PF_UserChangedParamExtra* extra)
{
    PF_Err err = PF_Err_NONE;

    // You can respond to user parameter changes here if needed

    return err;
}