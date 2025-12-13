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

struct RGBDelayIterateRefcon {
    const char* r_base{};
    const char* g_base{};
    const char* b_base{};
    A_long r_rowbytes{};
    A_long g_rowbytes{};
    A_long b_rowbytes{};
    PF_Boolean rg_same{ FALSE };
    PF_Boolean rb_same{ FALSE };
    PF_Boolean gb_same{ FALSE };
};

static PF_Err RGBDelayIterate8(void* refconP, A_long x, A_long y, PF_Pixel* inP, PF_Pixel* outP)
{
    (void)inP;
    const RGBDelayIterateRefcon* rc = reinterpret_cast<const RGBDelayIterateRefcon*>(refconP);

    const PF_Pixel* r = reinterpret_cast<const PF_Pixel*>(rc->r_base + y * rc->r_rowbytes) + x;
    const PF_Pixel* g = rc->rg_same ? r : (reinterpret_cast<const PF_Pixel*>(rc->g_base + y * rc->g_rowbytes) + x);
    const PF_Pixel* b = rc->rb_same ? r : (rc->gb_same ? g : (reinterpret_cast<const PF_Pixel*>(rc->b_base + y * rc->b_rowbytes) + x));

    outP->red = r->red;
    outP->green = g->green;
    outP->blue = b->blue;
    const int alpha_sum = static_cast<int>(r->alpha) + static_cast<int>(g->alpha) + static_cast<int>(b->alpha);
    outP->alpha = static_cast<A_u_char>(alpha_sum > 255 ? 255 : alpha_sum);
    return PF_Err_NONE;
}

static PF_Err RGBDelayIterate16(void* refconP, A_long x, A_long y, PF_Pixel16* inP, PF_Pixel16* outP)
{
    (void)inP;
    const RGBDelayIterateRefcon* rc = reinterpret_cast<const RGBDelayIterateRefcon*>(refconP);

    const PF_Pixel16* r = reinterpret_cast<const PF_Pixel16*>(rc->r_base + y * rc->r_rowbytes) + x;
    const PF_Pixel16* g = rc->rg_same ? r : (reinterpret_cast<const PF_Pixel16*>(rc->g_base + y * rc->g_rowbytes) + x);
    const PF_Pixel16* b = rc->rb_same ? r : (rc->gb_same ? g : (reinterpret_cast<const PF_Pixel16*>(rc->b_base + y * rc->b_rowbytes) + x));

    outP->red = r->red;
    outP->green = g->green;
    outP->blue = b->blue;
    outP->alpha = MAX(r->alpha, MAX(g->alpha, b->alpha));
    return PF_Err_NONE;
}

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
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING | PF_OutFlag2_SUPPORTS_SMART_RENDER;
    return err;
}

static A_long ClampTime(const PF_InData* in_data, A_long t)
{
    const A_long min_time = 0;
    const A_long max_time = in_data->total_time - in_data->time_step;
    if (t < min_time) return min_time;
    if (t > max_time) return max_time;
    return t;
}

static void UnionRect(PF_Rect* dst, const PF_Rect* src)
{
    if (dst->left == dst->right && dst->top == dst->bottom) {
        *dst = *src;
        return;
    }
    dst->left = MIN(dst->left, src->left);
    dst->top = MIN(dst->top, src->top);
    dst->right = MAX(dst->right, src->right);
    dst->bottom = MAX(dst->bottom, src->bottom);
}

static PF_Err SmartPreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* pre)
{
    PF_Err err = PF_Err_NONE;
    (void)out_data;

    PF_ParamDef red_param, green_param, blue_param;
    AEFX_CLR_STRUCT(red_param);
    AEFX_CLR_STRUCT(green_param);
    AEFX_CLR_STRUCT(blue_param);

    A_long red_delay = 0;
    A_long green_delay = 0;
    A_long blue_delay = 0;

    PF_Boolean checked_out_red = FALSE;
    PF_Boolean checked_out_green = FALSE;
    PF_Boolean checked_out_blue = FALSE;

    ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_RED_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &red_param));
    if (!err) checked_out_red = TRUE;
    if (!err) red_delay = red_param.u.sd.value;
    ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_GREEN_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &green_param));
    if (!err) checked_out_green = TRUE;
    if (!err) green_delay = green_param.u.sd.value;
    ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_BLUE_DELAY, in_data->current_time, in_data->time_step, in_data->time_scale, &blue_param));
    if (!err) checked_out_blue = TRUE;
    if (!err) blue_delay = blue_param.u.sd.value;

    if (checked_out_blue) PF_CHECKIN_PARAM(in_data, &blue_param);
    if (checked_out_green) PF_CHECKIN_PARAM(in_data, &green_param);
    if (checked_out_red) PF_CHECKIN_PARAM(in_data, &red_param);
    if (err) return err;

    const A_long red_time = ClampTime(in_data, in_data->current_time - red_delay * in_data->time_step);
    const A_long green_time = ClampTime(in_data, in_data->current_time - green_delay * in_data->time_step);
    const A_long blue_time = ClampTime(in_data, in_data->current_time - blue_delay * in_data->time_step);

    // Build a small unique-time table (max 3) and map each channel to a checkout id (1..count).
    A_long uniq_times[3] = { 0, 0, 0 };
    int uniq_count = 0;
    auto id_for_time = [&](A_long t) -> A_u_char {
        for (int i = 0; i < uniq_count; i++) {
            if (uniq_times[i] == t) return static_cast<A_u_char>(i + 1);
        }
        uniq_times[uniq_count] = t;
        uniq_count++;
        return static_cast<A_u_char>(uniq_count);
    };

    const A_u_char red_id = id_for_time(red_time);
    const A_u_char green_id = id_for_time(green_time);
    const A_u_char blue_id = id_for_time(blue_time);

    PF_RenderRequest req = pre->input->output_request;

    // Compute max_result_rect across all sampled times via "bounds checkouts" (empty request_rect).
    PF_Rect max_rect{0, 0, 0, 0};
    for (int i = 0; i < uniq_count; i++) {
        PF_CheckoutResult bounds_result;
        AEFX_CLR_STRUCT(bounds_result);

        PF_RenderRequest bounds_req = req;
        bounds_req.rect.left = bounds_req.rect.top = bounds_req.rect.right = bounds_req.rect.bottom = 0;

        const A_long bounds_checkout_id = 100 + i;
        ERR(pre->cb->checkout_layer(in_data->effect_ref, RGBDELAY_INPUT, bounds_checkout_id, &bounds_req,
            uniq_times[i], in_data->time_step, in_data->time_scale, &bounds_result));
        if (err) return err;

        UnionRect(&max_rect, &bounds_result.max_result_rect);
    }

    // Pixel checkouts for requested ROI at each unique time.
    for (int i = 0; i < uniq_count; i++) {
        PF_CheckoutResult in_result;
        AEFX_CLR_STRUCT(in_result);

        const A_long checkout_id = i + 1;
        ERR(pre->cb->checkout_layer(in_data->effect_ref, RGBDELAY_INPUT, checkout_id, &req,
            uniq_times[i], in_data->time_step, in_data->time_scale, &in_result));
        if (err) return err;
    }

    // Pack mapping into a single pointer-sized integer.
    const A_u_long packed =
        static_cast<A_u_long>(red_id) |
        (static_cast<A_u_long>(green_id) << 8) |
        (static_cast<A_u_long>(blue_id) << 16) |
        (static_cast<A_u_long>(uniq_count) << 24);
    pre->output->pre_render_data = reinterpret_cast<void*>((intptr_t)packed);

    pre->output->result_rect = req.rect;
    pre->output->max_result_rect = max_rect;

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
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        PF_Rect area{0, 0, outputP->width, outputP->height};

        RGBDelayIterateRefcon rc{};
        rc.r_base = reinterpret_cast<const char*>(red_ld->data);
        rc.g_base = reinterpret_cast<const char*>(green_ld->data);
        rc.b_base = reinterpret_cast<const char*>(blue_ld->data);
        rc.r_rowbytes = red_ld->rowbytes;
        rc.g_rowbytes = green_ld->rowbytes;
        rc.b_rowbytes = blue_ld->rowbytes;
        rc.rg_same = (red_ld == green_ld) ? TRUE : FALSE;
        rc.rb_same = (red_ld == blue_ld) ? TRUE : FALSE;
        rc.gb_same = (green_ld == blue_ld) ? TRUE : FALSE;

        if (PF_WORLD_IS_DEEP(outputP)) {
            // Fast path: all channels sample the same source; current 16-bit math becomes a straight copy.
            if (rc.rg_same && rc.rb_same) {
                err = PF_COPY(const_cast<PF_LayerDef*>(red_ld), outputP, nullptr, nullptr);
            } else {
                err = suites.Iterate16Suite1()->iterate(
                    in_data,
                    0,
                    outputP->height,
                    const_cast<PF_LayerDef*>(red_ld),
                    &area,
                    &rc,
                    RGBDelayIterate16,
                    outputP);
            }
        } else {
            err = suites.Iterate8Suite1()->iterate(
                in_data,
                0,
                outputP->height,
                const_cast<PF_LayerDef*>(red_ld),
                &area,
                &rc,
                RGBDelayIterate8,
                outputP);
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

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;
    (void)out_data;

    const A_u_long packed = static_cast<A_u_long>((intptr_t)extra->input->pre_render_data);
    const A_u_char red_id = static_cast<A_u_char>(packed & 0xFF);
    const A_u_char green_id = static_cast<A_u_char>((packed >> 8) & 0xFF);
    const A_u_char blue_id = static_cast<A_u_char>((packed >> 16) & 0xFF);
    const A_u_char uniq_count = static_cast<A_u_char>((packed >> 24) & 0xFF);

    PF_EffectWorld* output = nullptr;
    ERR(extra->cb->checkout_output(in_data->effect_ref, &output));
    if (err || !output) return err;

    PF_EffectWorld* uniq_worlds[4] = { nullptr, nullptr, nullptr, nullptr };
    for (A_u_char id = 1; id <= uniq_count; id++) {
        ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, id, &uniq_worlds[id]));
        if (err || !uniq_worlds[id]) return err;
    }

    PF_EffectWorld* redW = uniq_worlds[red_id];
    PF_EffectWorld* greenW = uniq_worlds[green_id];
    PF_EffectWorld* blueW = uniq_worlds[blue_id];
    if (!redW || !greenW || !blueW) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_Rect area{0, 0, output->width, output->height};

    RGBDelayIterateRefcon rc{};
    rc.r_base = reinterpret_cast<const char*>(redW->data);
    rc.g_base = reinterpret_cast<const char*>(greenW->data);
    rc.b_base = reinterpret_cast<const char*>(blueW->data);
    rc.r_rowbytes = redW->rowbytes;
    rc.g_rowbytes = greenW->rowbytes;
    rc.b_rowbytes = blueW->rowbytes;
    rc.rg_same = (redW == greenW) ? TRUE : FALSE;
    rc.rb_same = (redW == blueW) ? TRUE : FALSE;
    rc.gb_same = (greenW == blueW) ? TRUE : FALSE;

    if (PF_WORLD_IS_DEEP(output)) {
        if (rc.rg_same && rc.rb_same) {
            err = PF_COPY(redW, output, nullptr, nullptr);
        } else {
            err = suites.Iterate16Suite1()->iterate(
                in_data,
                0,
                output->height,
                redW,
                &area,
                &rc,
                RGBDelayIterate16,
                output);
        }
    } else {
        err = suites.Iterate8Suite1()->iterate(
            in_data,
            0,
            output->height,
            redW,
            &area,
            &rc,
            RGBDelayIterate8,
            output);
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
    case PF_Cmd_SMART_PRE_RENDER:
        err = SmartPreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
        break;
    case PF_Cmd_SMART_RENDER:
        err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
        break;
    }

    return err;
}
