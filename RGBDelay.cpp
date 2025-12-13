#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "Param_Utils.h"
#include "AE_Macros.h"
#include "String_Utils.h"
#include "RGBDelay.h"
#include <algorithm>
#include <stdio.h>

#define RGBDELAY_STAGE_VERSION 0
#define RGBDELAY_BUILD_VERSION 0

// �p�����[�^�Z�b�g�A�b�v

static PF_Err GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return err;
}

static PF_Err ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);

    PF_ADD_SLIDER("Red", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -1, RED_DELAY_DISK_ID);
    PF_ADD_SLIDER("Green", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -2, GREEN_DELAY_DISK_ID);
    PF_ADD_SLIDER("Blue", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -3, BLUE_DELAY_DISK_ID);

    out_data->num_params = RGBDELAY_NUM_PARAMS;
    return err;
}

// �����_�����O����
static PF_Err Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* outputP)
{
    PF_Err err = PF_Err_NONE;

    // �p�����[�^�擾
    A_long red_delay = params[RGBDELAY_RED_DELAY]->u.sd.value;
    A_long green_delay = params[RGBDELAY_GREEN_DELAY]->u.sd.value;
    A_long blue_delay = params[RGBDELAY_BLUE_DELAY]->u.sd.value;

    // �f�B���C�����̃K�[�h�i�͈͓��ɃN�����v�j
    A_long red_time = in_data->current_time - red_delay * in_data->time_step;
    A_long green_time = in_data->current_time - green_delay * in_data->time_step;
    A_long blue_time = in_data->current_time - blue_delay * in_data->time_step;

    // �t���[���͈̔͂��`�F�b�N
    A_long max_time = in_data->total_time - in_data->time_step;
    if (red_time < 0) red_time = 0;
    if (red_time > max_time) red_time = max_time;
    if (green_time < 0) green_time = 0;
    if (green_time > max_time) green_time = max_time;
    if (blue_time < 0) blue_time = 0;
    if (blue_time > max_time) blue_time = max_time;

    struct CheckedOutSource {
        A_long time{};
        PF_ParamDef param{};
        PF_Boolean checked_out{ FALSE };
    };

    CheckedOutSource sources[3]{};
    int sources_count = 0;

    const PF_LayerDef* input_ld = &params[RGBDELAY_INPUT]->u.ld;
    const PF_LayerDef* red_ld = nullptr;
    const PF_LayerDef* green_ld = nullptr;
    const PF_LayerDef* blue_ld = nullptr;

    auto resolve_layer_at_time = [&](A_long t, const PF_LayerDef** out_ld) -> PF_Err {
        if (t == in_data->current_time) {
            *out_ld = input_ld;
            return PF_Err_NONE;
        }

        for (int i = 0; i < sources_count; i++) {
            if (sources[i].time == t) {
                *out_ld = &sources[i].param.u.ld;
                return PF_Err_NONE;
            }
        }

        if (sources_count >= 3) {
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }

        sources[sources_count].time = t;
        AEFX_CLR_STRUCT(sources[sources_count].param);
        ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_INPUT, t, in_data->time_step, in_data->time_scale, &sources[sources_count].param));
        if (!err) {
            sources[sources_count].checked_out = TRUE;
            *out_ld = &sources[sources_count].param.u.ld;
            sources_count++;
        }

        return err;
    };

    ERR(resolve_layer_at_time(red_time, &red_ld));
    if (!err) {
        ERR(resolve_layer_at_time(green_time, &green_ld));
    }
    if (!err) {
        ERR(resolve_layer_at_time(blue_time, &blue_ld));
    }

    // Only render if all checkouts succeeded
    if (!err && red_ld && green_ld && blue_ld) {
        // 16bit����
        if (PF_WORLD_IS_DEEP(outputP)) {
        // 16bit����
        for (A_long y = 0; y < outputP->height; y++) {
            PF_Pixel16* out_pixel = (PF_Pixel16*)((char*)outputP->data + y * outputP->rowbytes);
            PF_Pixel16* r_pixel = (PF_Pixel16*)((char*)red_ld->data + y * red_ld->rowbytes);
            PF_Pixel16* g_pixel = (PF_Pixel16*)((char*)green_ld->data + y * green_ld->rowbytes);
            PF_Pixel16* b_pixel = (PF_Pixel16*)((char*)blue_ld->data + y * blue_ld->rowbytes);

            for (A_long x = 0; x < outputP->width; x++) {
                out_pixel[x].red = r_pixel[x].red;
                out_pixel[x].green = g_pixel[x].green;
                out_pixel[x].blue = b_pixel[x].blue;
                out_pixel[x].alpha = MAX(r_pixel[x].alpha, MAX(g_pixel[x].alpha, b_pixel[x].alpha));
            }
        }
    }
    else {
        // 8bit�����i�����̃R�[�h�j
        for (A_long y = 0; y < outputP->height; y++) {
            PF_Pixel* out_pixel = (PF_Pixel*)((char*)outputP->data + y * outputP->rowbytes);
            PF_Pixel* r_pixel = (PF_Pixel*)((char*)red_ld->data + y * red_ld->rowbytes);
            PF_Pixel* g_pixel = (PF_Pixel*)((char*)green_ld->data + y * green_ld->rowbytes);
            PF_Pixel* b_pixel = (PF_Pixel*)((char*)blue_ld->data + y * blue_ld->rowbytes);

            for (A_long x = 0; x < outputP->width; x++) {
                out_pixel[x].red = r_pixel[x].red;
                out_pixel[x].green = g_pixel[x].green;
                out_pixel[x].blue = b_pixel[x].blue;
                int alpha_sum = r_pixel[x].alpha + g_pixel[x].alpha + b_pixel[x].alpha;
                out_pixel[x].alpha = (A_u_char)(alpha_sum > 255 ? 255 : alpha_sum);
            }
        }
    }
    }  // End of render block

    // �`�F�b�N�C���i�`�F�b�N�A�E�g�����̂݁j
    for (int i = sources_count - 1; i >= 0; i--) {
        if (sources[i].checked_out) {
            PF_CHECKIN_PARAM(in_data, &sources[i].param);
        }
    }
    
    return err;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "RGBDelay",         // Name
        "361do RGBDelay",    // Match Name
        "361do_plugins",        // Category
        AE_RESERVED_INFO,   // Reserved Info
        "EffectMain",       // Entry point
        "https://github.com/rebuildup/Ae_RGBDelay" // Support URL
    );

    return result;
}

extern "C" DllExport
PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;

    switch (cmd) {
    case PF_Cmd_ABOUT: {
        const char* info =
            "RGBDelay v0.1.0 (Beta)\n"
            "Copyright (C) 2024 Tsuyoshi Okumura/Hotkey ltd.\n"
            "All Rights Reserved.\n"
            "\n"
            "This software is provided \"as is\" without warranty of any kind.\n"
            "Use at your own risk.\n";
        PF_SPRINTF(out_data->return_msg, "%s", info);
        break;
    }
    case PF_Cmd_GLOBAL_SETUP:
        err = GlobalSetup(in_data, out_data, params, output);
        break;
    case PF_Cmd_PARAMS_SETUP:
        err = ParamsSetup(in_data, out_data, params, output);
        break;
    case PF_Cmd_RENDER:
        err = Render(in_data, out_data, params, output);
        break;
    }

    return err;
}
