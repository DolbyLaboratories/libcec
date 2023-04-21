/******************************************************************************
 * This program is protected under international and U.S. copyright laws as
 * an unpublished work. This program is confidential and proprietary to the
 * copyright owners. Reproduction or disclosure, in whole or in part, or the
 * production of derivative works therefrom without the express permission of
 * the copyright owners is prohibited.
 *
 *                Copyright (C) 2019 by Dolby International AB.
 *                            All rights reserved.
 ******************************************************************************/

/**
 *  @file       dlb_lip_types.h
 *  @brief      LIP types
 *
 */

#pragma once
#include "dlb_lip.h"
#include "dlb_lip_cache.h"
#include "dlb_lip_cec_bus.h"
#include "dlb_lip_osa.h"

#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static const uint8_t DOLBY_VENDOR_ID[] = { 0x00, 0xD0, 0x46 };

typedef enum lip_state
{
    LIP_INIT,
    LIP_WAIT_FOR_REPLY,
    LIP_SUPPORTED,
    LIP_UNSUPPORTED,
} lip_state_t;

typedef enum lip_avr_connection_type
{
    LIP_AVR_UNKNOWN,
    LIP_AVR_ARC,
    LIP_AVR_EARC = LIP_AVR_ARC,
    LIP_AVR_HUB
} lip_avr_connection_type_t;

typedef enum lip_cec_opcode
{
    LIP_OPCODE_REQUEST_LIP_SUPPORT   = 0x10,
    LIP_OPCODE_REPORT_LIP_SUPPORT    = 0x11,
    LIP_OPCODE_REQUEST_AV_LATENCY    = 0x12,
    LIP_OPCODE_REPORT_AV_LATENCY     = 0x13,
    LIP_OPCODE_REQUEST_AUDIO_LATENCY = 0x14,
    LIP_OPCODE_REPORT_AUDIO_LATENCY  = 0x15,
    LIP_OPCODE_REQUEST_VIDEO_LATENCY = 0x16,
    LIP_OPCODE_REPORT_VIDEO_LATENCY  = 0x17,
    LIP_OPCODE_UPDATE_UUID           = 0x18,

    LIP_OPCODES
} lip_cec_opcode_t;

typedef enum dlb_lip_latency_type
{
    LIP_OWN_LATENCY,
    LIP_DOWNSTREAM_LATENCY,
    LIP_TOTAL_LATENCY

} dlb_lip_latency_type_t;

typedef struct dlb_downstream_device_config
{
    dlb_cec_logical_address_t logical_addr; // Downstream device addr - for TV with AVR connected over
                                            // ARC it indicates if AVR supports LIP
    uint32_t            uuid;
    dlb_latency_cache_t latency_cache;
} dlb_downstream_device_config_t;

typedef enum dlb_lip_pending_message_state_e
{
    LIP_MESSAGE_PENDING,
    LIP_MESSAGE_SENT,
    LIP_MESSAGE_ABORT_RECEIVED,
    LIP_MESSAGE_ANSWER_RECEIVED,
    LIP_MESSAGE_HANDLED

} dlb_lip_pending_message_state_t;

typedef struct dlb_lip_pending_message_s
{
    dlb_cec_message_t               msg;
    dlb_lip_pending_message_state_t state;
    unsigned long long              expire_time_ms;
} dlb_lip_pending_message_t;

typedef struct dlb_lip_pending_messages_s
{
    dlb_lip_pending_message_t messages[MAX_UPSTREAM_DEVICES_COUNT];
    dlb_cond_t                cv;
} dlb_lip_pending_messages_s;

struct dlb_lip_s
{
    dlb_cec_bus_t cec_bus;

    // Thread handle
    dlb_thread_t request_thread;
    dlb_mutex_t  critical_section;
    dlb_cond_t   condition_var;
    dlb_cond_t   state_updated_condition_var;
    bool         is_running;
    bool         thread_signaled;

    // lip current state
    lip_state_t state;
    // delayed REQUEST_LIP_SUPPORT
    dlb_lip_pending_messages_s pending_requests;
    // to match with feature abort message
    lip_cec_opcode_t opcode_of_last_cmd_sent[MAX_UPSTREAM_DEVICES_COUNT];

    // Logical address of LIP device
    dlb_cec_logical_address_t upstream_devices_addresses[MAX_UPSTREAM_DEVICES_COUNT];

    // Device configs
    dlb_lip_config_params_t config_params;
    // Downstream device config
    dlb_downstream_device_config_t downstream_device_cfg;

    // callback
    dlb_lip_callbacks_t callbacks;
    // timer
    dlb_lip_osa_timer_t timer;
    uint32_t            callback_id;

    dlb_lip_video_format_t req_video_format;
    dlb_lip_audio_format_t req_audio_format;

    // Apply IEC Decoding latency
    bool add_iec_decoding_latency;

    unsigned long long start_time;
};

static inline uint8_t dlb_lip_get_hdr_mode_from_video_format(dlb_lip_video_format_t video_format)
{
    uint8_t hdr_mode = HDR_MODES_COUNT;
    switch (video_format.color_format)
    {
    case LIP_COLOR_FORMAT_HDR_STATIC:
    {
        hdr_mode = (uint8_t)(
            video_format.hdr_mode.hdr_static >= LIP_HDR_STATIC_COUNT ? (uint8_t)HDR_MODES_COUNT : video_format.hdr_mode.hdr_static);
        break;
    }
    case LIP_COLOR_FORMAT_HDR_DYNAMIC:
    {
        hdr_mode = (uint8_t)(
            video_format.hdr_mode.hdr_dynamic >= LIP_HDR_DYNAMIC_COUNT ? HDR_MODES_COUNT : video_format.hdr_mode.hdr_dynamic);
        break;
    }
    case LIP_COLOR_FORMAT_DOLBY_VISION:
    {
        hdr_mode = (uint8_t)(
            video_format.hdr_mode.dolby_vision >= LIP_HDR_DOLBY_VISION_COUNT ? HDR_MODES_COUNT
                                                                             : video_format.hdr_mode.dolby_vision);
        break;
    }
    default:
    {
        assert(!"Invalid color format");
        break;
    }
    }
    return hdr_mode;
}

typedef enum dlb_lip_hdr_offsets_e
{
    HDR_STATIC_OFFSET   = 0,
    HDR_DYNAMIC_OFFSET  = 64,
    DOLBY_VISION_OFFSET = 128

} dlb_lip_hdr_offsets_t;

typedef enum dlb_lip_hdr_format_vals_e
{
    HDR_STATIC_SDR           = HDR_STATIC_OFFSET + LIP_HDR_STATIC_SDR,
    HDR_STATIC_HDR           = HDR_STATIC_OFFSET + LIP_HDR_STATIC_HDR,
    HDR_STATIC_SMPTE_ST_2084 = HDR_STATIC_OFFSET + LIP_HDR_STATIC_SMPTE_ST_2084,
    HDR_STATIC_HLG           = HDR_STATIC_OFFSET + LIP_HDR_STATIC_HLG,
    /* 4 .. 63 - RESERVED*/
    HDR_DYNAMIC_SMPTE_ST_2094_10 = HDR_DYNAMIC_OFFSET + LIP_HDR_DYNAMIC_SMPTE_ST_2094_10,
    HDR_DYNAMIC_ETSI_TS_103_433  = HDR_DYNAMIC_OFFSET + LIP_HDR_DYNAMIC_ETSI_TS_103_433,
    HDR_DYNAMIC_ITU_T_H265       = HDR_DYNAMIC_OFFSET + LIP_HDR_DYNAMIC_ITU_T_H265,
    HDR_DYNAMIC_SMPTE_ST_2094_40 = HDR_DYNAMIC_OFFSET + LIP_HDR_DYNAMIC_SMPTE_ST_2094_40,
    /* 68 .. 127 - RESERVED*/
    DOLBY_VISION_SINK_LED   = DOLBY_VISION_OFFSET + LIP_HDR_DOLBY_VISION_SINK_LED,
    DOLBY_VISION_SOURCE_LED = DOLBY_VISION_OFFSET + LIP_HDR_DOLBY_VISION_SOURCE_LED
    /* 130 .. 255 - RESERVED*/

} dlb_lip_hdr_format_vals_t;

#if defined(static_assert) || defined(_MSC_VER)
static_assert((HDR_STATIC_HLG - HDR_STATIC_SDR == LIP_HDR_STATIC_COUNT - 1), "HDR_STATIC types mismatch");
static_assert(
    HDR_DYNAMIC_SMPTE_ST_2094_40 - HDR_DYNAMIC_SMPTE_ST_2094_10 == LIP_HDR_DYNAMIC_COUNT - 1,
    "HDR_DYNAMIC types mismatch");
static_assert(DOLBY_VISION_SOURCE_LED - DOLBY_VISION_SINK_LED == LIP_HDR_DOLBY_VISION_COUNT - 1, "DOLBY_VISION types mismatch");
#endif

static inline uint32_t get_hdr_mode_from_value_value(uint8_t value, dlb_lip_video_format_t *video_format)
{
    uint32_t ret = 0;

    switch (value)
    {
    case HDR_STATIC_SDR:
    {
        video_format->color_format        = LIP_COLOR_FORMAT_HDR_STATIC;
        video_format->hdr_mode.hdr_static = LIP_HDR_STATIC_SDR;
        break;
    }
    case HDR_STATIC_HDR:
    {
        video_format->color_format        = LIP_COLOR_FORMAT_HDR_STATIC;
        video_format->hdr_mode.hdr_static = LIP_HDR_STATIC_HDR;
        break;
    }
    case HDR_STATIC_SMPTE_ST_2084:
    {
        video_format->color_format        = LIP_COLOR_FORMAT_HDR_STATIC;
        video_format->hdr_mode.hdr_static = LIP_HDR_STATIC_SMPTE_ST_2084;
        break;
    }
    case HDR_STATIC_HLG:
    {
        video_format->color_format        = LIP_COLOR_FORMAT_HDR_STATIC;
        video_format->hdr_mode.hdr_static = LIP_HDR_STATIC_HLG;
        break;
    }

    case HDR_DYNAMIC_SMPTE_ST_2094_10:
    {
        video_format->color_format         = LIP_COLOR_FORMAT_HDR_DYNAMIC;
        video_format->hdr_mode.hdr_dynamic = LIP_HDR_DYNAMIC_SMPTE_ST_2094_10;
        break;
    }
    case HDR_DYNAMIC_ETSI_TS_103_433:
    {
        video_format->color_format         = LIP_COLOR_FORMAT_HDR_DYNAMIC;
        video_format->hdr_mode.hdr_dynamic = LIP_HDR_DYNAMIC_ETSI_TS_103_433;
        break;
    }
    case HDR_DYNAMIC_ITU_T_H265:
    {
        video_format->color_format         = LIP_COLOR_FORMAT_HDR_DYNAMIC;
        video_format->hdr_mode.hdr_dynamic = LIP_HDR_DYNAMIC_ITU_T_H265;
        break;
    }
    case HDR_DYNAMIC_SMPTE_ST_2094_40:
    {
        video_format->color_format         = LIP_COLOR_FORMAT_HDR_DYNAMIC;
        video_format->hdr_mode.hdr_dynamic = LIP_HDR_DYNAMIC_SMPTE_ST_2094_40;
        break;
    }

    case DOLBY_VISION_SINK_LED:
    {
        video_format->color_format          = LIP_COLOR_FORMAT_DOLBY_VISION;
        video_format->hdr_mode.dolby_vision = LIP_HDR_DOLBY_VISION_SINK_LED;
        break;
    }
    case DOLBY_VISION_SOURCE_LED:
    {
        video_format->color_format          = LIP_COLOR_FORMAT_DOLBY_VISION;
        video_format->hdr_mode.dolby_vision = LIP_HDR_DOLBY_VISION_SOURCE_LED;
        break;
    }

    default:
    {
        video_format->color_format        = LIP_COLOR_FORMAT_COUNT;
        video_format->hdr_mode.hdr_static = LIP_HDR_STATIC_COUNT;
        ret                               = 1;
        break;
    }
    }
    return ret;
}

static inline uint8_t get_value_from_hdr_mode(dlb_lip_video_format_t video_format)
{
    uint8_t value = 0;
    switch (video_format.color_format)
    {
    case LIP_COLOR_FORMAT_HDR_STATIC:
    {
        value = (uint8_t)(video_format.hdr_mode.hdr_static + HDR_STATIC_OFFSET);
        break;
    }
    case LIP_COLOR_FORMAT_HDR_DYNAMIC:
    {
        value = (uint8_t)(video_format.hdr_mode.hdr_dynamic + HDR_DYNAMIC_OFFSET);
        break;
    }
    case LIP_COLOR_FORMAT_DOLBY_VISION:
    {
        value = (uint8_t)(video_format.hdr_mode.dolby_vision + DOLBY_VISION_OFFSET);
        break;
    }
    default:
        assert(!"Invalid color format");
        break;
    }

    return value;
}

#ifdef __cplusplus
}
#endif
