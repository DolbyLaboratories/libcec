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
 *  @file       dlb_lip.c
 *  @brief      todo
 *
 *  Todo
 */

/* dlb_lip include */
#include "dlb_lip.h"
#include "dlb_lip_cmd_builder.h"
#include "dlb_lip_osa.h"
#include "dlb_lip_types.h"

/* General includes needed for binary */
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const uint32_t LIP_SOURCE_TIMEOUT_MS = 2000;
static const uint32_t LIP_HUB_TIMEOUT_MS    = 1000;
static const uint32_t LIP_INVALID_UUID      = 0xFFFFFFFF;
static const uint32_t LIP_UUID_MASK         = 0xFFFF0000;
static const uint32_t LIP_UUID_VIDEO_MASK   = 0x0000FF00;
static const uint32_t LIP_UUID_AUDIO_MASK   = 0x000000FF;

/* Memory alignment */
#define DLB_LOOSE_SIZE(alignment, size) ((alignment)-1u + (size))

#define NUM_ELEMS(x) (sizeof(x) / sizeof((x)[0]))

static int lip_cec_cmd_received(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command);

typedef struct lip_responses_s
{
    dlb_cec_message_t msg[MAX_UPSTREAM_DEVICES_COUNT];
    unsigned int      valid_messages;
} lip_responses_t;

static const uint8_t IEC_DECODING_DELAY[IEC61937_AUDIO_CODECS] = { [PCM]                    = 0,
                                                                   [IEC61937_AC3]           = 7,
                                                                   [IEC61937_SMPTE_338M]    = 0,
                                                                   [IEC61937_PAUSE_BURST]   = 0,
                                                                   [IEC61937_MPEG1_L1]      = 0,
                                                                   [IEC61937_MEPG1_L2_L3]   = 0,
                                                                   [IEC61937_MPEG2]         = 0,
                                                                   [IEC61937_MPEG2_AAC]     = 0,
                                                                   [IEC61937_MPEG2_L1]      = 0,
                                                                   [IEC61937_MPEG2_L2]      = 0,
                                                                   [IEC61937_MPEG2_L3]      = 0,
                                                                   [IEC61937_DTS_TYPE_I]    = 0,
                                                                   [IEC61937_DTS_TYPE_II]   = 0,
                                                                   [IEC61937_DTS_TYPE_III]  = 0,
                                                                   [IEC61937_ATRAC]         = 0,
                                                                   [IEC61937_ATRAC_2_3]     = 0,
                                                                   [IEC61937_ATRAC_X]       = 0,
                                                                   [IEC61937_DTS_TYPE_IV]   = 0,
                                                                   [IEC61937_WMA_PRO]       = 0,
                                                                   [IEC61937_MPEG2_AAC_LSF] = 0,
                                                                   [IEC61937_MPEG4_AAC]     = 0,
                                                                   [IEC61937_EAC3]          = 47,
                                                                   [IEC61937_MAT]           = 6,
                                                                   [IEC61937_MPEG4]         = 0 };

static void *lip_aligned_ptr(size_t alignment, const void *ptr)
{
    assert((0 < alignment) && (alignment == (alignment & ~(alignment - 1u))));
    return (void *)(((uintptr_t)ptr + alignment - 1U) & ~(alignment - 1U));
}

static const char *lip_state_description(lip_state_t state)
{
    const char *str = NULL;
    switch (state)
    {
    case LIP_INIT:
        str = "LIP_INIT";
        break;
    case LIP_WAIT_FOR_REPLY:
        str = "LIP_WAIT_FOR_REPLY";
        break;
    case LIP_SUPPORTED:
        str = "LIP_SUPPORTED";
        break;
    case LIP_UNSUPPORTED:
        str = "LIP_UNSUPPORTED";
        break;
    default:
        assert(!"Invalid lip_state");
        str = "Invalid lip_state";
        break;
    }
    return str;
}

static uint8_t lip_get_command_min_length(const lip_cec_opcode_t opcode)
{
    uint8_t length = 0;

    switch (opcode)
    {
    case LIP_OPCODE_REQUEST_LIP_SUPPORT:
    {
        length = 4; // VENDOR ID(3) + OPCODE(1)
        break;
    }
    case LIP_OPCODE_UPDATE_UUID:
    case LIP_OPCODE_REPORT_LIP_SUPPORT:
    {
        length = 9; // VENDOR ID(3) + OPCODE(1) + UUID(4) + VERSION(1)
        break;
    }
    case LIP_OPCODE_REQUEST_AV_LATENCY:
    {
        length = 7; // VENDOR ID(3) + OPCODE(1) + VIC(1)  + HDR_MODE(1) + AUDIO_FORMAT(1) + OPTIONAL(AUDIO_EXT(1))
        break;
    }
    case LIP_OPCODE_REPORT_AV_LATENCY:
    {
        length = 6; // VENDOR ID(3) + OPCODE(1) +  VIDEO_LAT(1) + AUDIO_LAT(1)
        break;
    }
    case LIP_OPCODE_REQUEST_AUDIO_LATENCY:
    {
        length = 5; // VENDOR ID(3) + OPCODE(1) +  + AUDIO_FORMAT(1) + OPTIONAL(AUDIO_EXT(1))
        break;
    }
    case LIP_OPCODE_REPORT_AUDIO_LATENCY:
    {
        length = 5; // VENDOR ID(3) + OPCODE(1) +  AUDIO_LAT(1)
        break;
    }
    case LIP_OPCODE_REQUEST_VIDEO_LATENCY:
    {
        length = 6; // VENDOR ID(3) + OPCODE(1) +  VIC(1) + HDR_MODE(1)
        break;
    }
    case LIP_OPCODE_REPORT_VIDEO_LATENCY:
    {
        length = 5; // VENDOR ID(3) + OPCODE(1) +  VID_LAT(1)
        break;
    }
    default:
        length = 0;
        break;
    }

    return length;
}
static bool lip_validate_cmd_vendor_id(const dlb_cec_message_t *const command)
{
    bool ret = false;
    if (command->msg_length >= 3 && // 3 Bytes for VENDOR_ID
        DOLBY_VENDOR_ID[0] == command->data[0] && DOLBY_VENDOR_ID[1] == command->data[1] && DOLBY_VENDOR_ID[2] == command->data[2])
    {
        ret = true;
    }
    return ret;
}

static lip_cec_opcode_t lip_get_command_opcode(const dlb_cec_message_t *const command)
{
    lip_cec_opcode_t lip_opcode = LIP_OPCODES;
    if (command->msg_length >= 4 && // 3 Bytes for VENDOR_ID and 1 for OPCODE
        DOLBY_VENDOR_ID[0] == command->data[0] && DOLBY_VENDOR_ID[1] == command->data[1] && DOLBY_VENDOR_ID[2] == command->data[2])
    {
        lip_opcode = command->data[3];
    }
    switch (lip_opcode)
    {
    case LIP_OPCODE_REQUEST_LIP_SUPPORT:
    case LIP_OPCODE_UPDATE_UUID:
    case LIP_OPCODE_REPORT_LIP_SUPPORT:
    case LIP_OPCODE_REQUEST_AV_LATENCY:
    case LIP_OPCODE_REPORT_AV_LATENCY:
    case LIP_OPCODE_REQUEST_AUDIO_LATENCY:
    case LIP_OPCODE_REPORT_AUDIO_LATENCY:
    case LIP_OPCODE_REQUEST_VIDEO_LATENCY:
    case LIP_OPCODE_REPORT_VIDEO_LATENCY:
    {
        break;
    }
    default:
        lip_opcode = LIP_OPCODES;
        break;
    }

    return lip_opcode;
}

static bool lip_is_video_format_valid(dlb_lip_video_format_t video_format)
{
    bool valid = true;

    if (video_format.vic >= MAX_VICS)
    {
        valid = false;
    }
    else if (video_format.color_format >= LIP_COLOR_FORMAT_COUNT)
    {
        valid = false;
    }
    else if (dlb_lip_get_hdr_mode_from_video_format(video_format) >= HDR_MODES_COUNT)
    {
        valid = false;
    }

    return valid;
}

static bool lip_is_audio_format_valid(dlb_lip_audio_format_t audio_format)
{
    bool valid = true;

    if (audio_format.codec >= IEC61937_AUDIO_CODECS)
    {
        valid = false;
    }
    else if (audio_format.ext >= MAX_AUDIO_FORMAT_EXTENSIONS)
    {
        valid = false;
    }
    else if (audio_format.subtype >= IEC61937_SUBTYPES)
    {
        valid = false;
    }

    return valid;
}

#define lip_log_message(p_dlb_lip, fmt) \
    lip_log_message_internal(p_dlb_lip, "LIP:   [%llu]\t" fmt, dlb_lip_osa_get_time_ms() - (p_dlb_lip ? p_dlb_lip->start_time : 0))
#define lip_log_message_n(p_dlb_lip, fmt, ...) \
    lip_log_message_internal(                  \
        p_dlb_lip, "LIP:   [%llu]\t" fmt, dlb_lip_osa_get_time_ms() - (p_dlb_lip ? p_dlb_lip->start_time : 0), __VA_ARGS__)
static void lip_log_message_internal(dlb_lip_t *const p_dlb_lip, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (p_dlb_lip && p_dlb_lip->callbacks.printf_callback)
    {
        p_dlb_lip->callbacks.printf_callback(p_dlb_lip->callbacks.arg, format, args);
    }
    else
    {
        vprintf(format, args);
    }
    va_end(args);
}

static int lip_transmit_wrapper(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command)
{
    lip_log_message_n(
        p_dlb_lip,
        "transmitting from: %d to %d, size: %d, opcode: 0x%x\n",
        command->initiator,
        command->destination,
        command->msg_length,
        command->opcode);
    p_dlb_lip->opcode_of_last_cmd_sent[command->destination] = lip_get_command_opcode(command);

    return p_dlb_lip->cec_bus.transmit_callback(p_dlb_lip->cec_bus.handle, command);
}

static bool lip_validate_latency(int latency)
{
    bool valid = true;
    if (latency >= LIP_INVALID_LATENCY)
    {
        valid = false;
    }
    else if (latency < 0)
    {
        valid = false;
    }

    return valid;
}

static dlb_lip_status_t lip_get_status(dlb_lip_t *const p_dlb_lip, const bool wait_for_discovery)
{
    dlb_lip_status_t status       = { 0 };
    status.downstream_device_addr = DLB_LOGICAL_ADDR_UNKNOWN;
    for (unsigned int i = 0; i < NUM_ELEMS(status.upstream_devices_addresses); i += 1)
    {
        status.upstream_devices_addresses[i] = DLB_LOGICAL_ADDR_UNKNOWN;
    }

    if (p_dlb_lip)
    {
        if (wait_for_discovery)
        {
            while (p_dlb_lip->state != LIP_SUPPORTED && p_dlb_lip->state != LIP_UNSUPPORTED)
            {
                dlb_lip_osa_wait_condition(
                    &p_dlb_lip->state_updated_condition_var, &p_dlb_lip->critical_section, LIP_OSA_INFINITE_TIMEOUT, NULL);
            }
        }

        for (unsigned int i = 0; i < NUM_ELEMS(status.upstream_devices_addresses); i += 1)
        {
            status.upstream_devices_addresses[i] = DLB_LOGICAL_ADDR_UNKNOWN;

            if (p_dlb_lip->upstream_devices_addresses[i] != DLB_LOGICAL_ADDR_UNKNOWN)
            {
                status.status |= LIP_UPSTREAM_CONNECTED;
                status.upstream_devices_addresses[i] = p_dlb_lip->upstream_devices_addresses[i];
            }
        }
        if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN)
        {
            status.status |= LIP_DOWNSTREAM_CONNECTED;
            status.downstream_device_addr = p_dlb_lip->downstream_device_cfg.logical_addr;
            status.downstream_device_uuid = p_dlb_lip->downstream_device_cfg.uuid;
        }
    }

    return status;
}

static void lip_status_change_callback(dlb_lip_t *const p_dlb_lip)
{
    if (p_dlb_lip->callbacks.status_change_callback)
    {
        p_dlb_lip->callbacks.status_change_callback(p_dlb_lip->callbacks.arg, lip_get_status(p_dlb_lip, false));
    }
}

static uint8_t lip_sum_latencies(dlb_lip_t *const p_dlb_lip, int own_latency, int downstream_latency)
{
    int latency = own_latency + downstream_latency;
    (void)p_dlb_lip;

    if (!lip_validate_latency(own_latency))
    {
        latency = LIP_INVALID_LATENCY;
    }
    else if (!lip_validate_latency(downstream_latency))
    {
        latency = LIP_INVALID_LATENCY;
    }
    else if (!lip_validate_latency(latency))
    {
        latency = LIP_INVALID_LATENCY - 1U;
    }

    return (uint8_t)latency;
}

static bool lip_get_audio_latency_from_cache(
    dlb_lip_t *const       p_dlb_lip,
    dlb_lip_audio_format_t audio_format,
    dlb_lip_audio_format_t audio_format_downstream,
    dlb_lip_latency_type_t mode,
    uint8_t *              audio_latency)
{
    bool          ret                   = false;
    const uint8_t iec_additonal_latency = p_dlb_lip->add_iec_decoding_latency ? IEC_DECODING_DELAY[audio_format.codec] : 0U;
    const uint8_t own_audio_latency     = lip_sum_latencies(
        p_dlb_lip,
        p_dlb_lip->config_params.audio_latencies[audio_format.codec][audio_format.subtype][audio_format.ext],
        iec_additonal_latency);
    uint8_t downstream_audio_latency = LIP_INVALID_LATENCY;
    bool    downstream_latency_valid = dlb_cache_get_audio_latency(
        &p_dlb_lip->downstream_device_cfg.latency_cache, audio_format_downstream, &downstream_audio_latency);

    *audio_latency = LIP_INVALID_LATENCY;

    switch (mode)
    {
    case LIP_OWN_LATENCY:
    {
        *audio_latency = lip_sum_latencies(p_dlb_lip, own_audio_latency, 0);
        ret            = true;
        break;
    }

    case LIP_DOWNSTREAM_LATENCY:
    {
        if (downstream_latency_valid)
        {
            *audio_latency = lip_sum_latencies(p_dlb_lip, 0, downstream_audio_latency);
            ret            = true;
        }
        break;
    }
    case LIP_TOTAL_LATENCY:
    {
        if (downstream_latency_valid)
        {
            *audio_latency = lip_sum_latencies(p_dlb_lip, own_audio_latency, downstream_audio_latency);
            ret            = true;
        }
        break;
    }
    default:
        assert(!"Invalid latency mode");
        *audio_latency = LIP_INVALID_LATENCY;
        break;
    }

    return ret;
}

static uint8_t lip_get_video_latency_from_cache(
    dlb_lip_t *const       p_dlb_lip,
    dlb_lip_video_format_t video_format,
    dlb_lip_latency_type_t mode,
    uint8_t *              video_latency)
{
    bool          ret                      = false;
    uint8_t       downstream_video_latency = LIP_INVALID_LATENCY;
    const uint8_t own_video_latency
        = p_dlb_lip->config_params
              .video_latencies[video_format.vic][video_format.color_format][dlb_lip_get_hdr_mode_from_video_format(video_format)];
    bool downstream_latency_valid
        = dlb_cache_get_video_latency(&p_dlb_lip->downstream_device_cfg.latency_cache, video_format, &downstream_video_latency);

    *video_latency = LIP_INVALID_LATENCY;

    switch (mode)
    {
    case LIP_OWN_LATENCY:
    {
        *video_latency = lip_sum_latencies(p_dlb_lip, own_video_latency, 0);
        ret            = true;
        break;
    }

    case LIP_DOWNSTREAM_LATENCY:
    {
        if (downstream_latency_valid)
        {
            *video_latency = lip_sum_latencies(p_dlb_lip, 0, downstream_video_latency);
            ret            = true;
        }
        break;
    }
    case LIP_TOTAL_LATENCY:
    {
        if (downstream_latency_valid)
        {
            *video_latency = lip_sum_latencies(p_dlb_lip, own_video_latency, downstream_video_latency);
            ret            = true;
        }
        break;
    }
    default:
        assert(!"Invalid latency mode");
        *video_latency = LIP_INVALID_LATENCY;
        break;
    }

    return ret;
}

static bool lip_is_upstream_device_present(dlb_lip_t *const p_dlb_lip)
{
    bool ret = false;
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
    {
        if (p_dlb_lip->upstream_devices_addresses[i] != DLB_LOGICAL_ADDR_UNKNOWN)
        {
            ret = true;
            break;
        }
    }
    return ret;
}

static void lip_add_upstream_device(dlb_lip_t *const p_dlb_lip, const dlb_cec_logical_address_t new_upstream_device)
{
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
    {
        if (p_dlb_lip->upstream_devices_addresses[i] == DLB_LOGICAL_ADDR_UNKNOWN
            || p_dlb_lip->upstream_devices_addresses[i] == new_upstream_device)
        {
            p_dlb_lip->upstream_devices_addresses[i] = new_upstream_device;
            break;
        }
    }
}

static void lip_remove_upstream_device(dlb_lip_t *const p_dlb_lip, const dlb_cec_logical_address_t upstream_device)
{
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
    {
        if (upstream_device == p_dlb_lip->upstream_devices_addresses[i] || upstream_device == DLB_LOGICAL_ADDR_BROADCAST)
        {
            p_dlb_lip->upstream_devices_addresses[i] = DLB_LOGICAL_ADDR_UNKNOWN;
            break;
        }
    }
}

static uint32_t lip_get_timeout_value_ms(dlb_lip_t *p_dlb_lip)
{
    uint32_t timeout_ms = LIP_SOURCE_TIMEOUT_MS;

    if (lip_is_upstream_device_present(p_dlb_lip))
    {
        timeout_ms = LIP_HUB_TIMEOUT_MS;
    }
    return timeout_ms;
}

static uint32_t lip_get_uuid(dlb_lip_t *const p_dlb_lip)
{
    uint32_t merged_uuid = p_dlb_lip->config_params.uuid;

    if (p_dlb_lip->downstream_device_cfg.uuid != LIP_INVALID_UUID)
    {
        if (p_dlb_lip->callbacks.merge_uuid_callback)
        {
            merged_uuid = p_dlb_lip->callbacks.merge_uuid_callback(
                p_dlb_lip->callbacks.arg, p_dlb_lip->config_params.uuid, p_dlb_lip->downstream_device_cfg.uuid);
        }
    }
    return merged_uuid;
}

static bool lip_is_request_answered(dlb_lip_t *const p_dlb_lip, dlb_cec_logical_address_t source)
{
    return p_dlb_lip->pending_requests.messages[source].state == LIP_MESSAGE_ABORT_RECEIVED
           || p_dlb_lip->pending_requests.messages[source].state == LIP_MESSAGE_ANSWER_RECEIVED;
}

static bool lip_is_request_pending(dlb_lip_t *const p_dlb_lip, dlb_cec_logical_address_t source)
{
    return p_dlb_lip->pending_requests.messages[source].state != LIP_MESSAGE_HANDLED;
}
static bool lip_is_any_request_pending(dlb_lip_t *const p_dlb_lip)
{
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
    {
        if (p_dlb_lip->pending_requests.messages[i].state != LIP_MESSAGE_HANDLED)
        {
            return true;
        }
    }
    return false;
}

static bool lip_is_any_pending_request_sent(dlb_lip_t *const p_dlb_lip)
{
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
    {
        if (p_dlb_lip->pending_requests.messages[i].state == LIP_MESSAGE_SENT)
        {
            return true;
        }
    }
    return false;
}
static void lip_wait_for_pending_request(dlb_lip_t *const p_dlb_lip, dlb_cec_logical_address_t source)
{
    uint32_t           timeout_ms      = lip_get_timeout_value_ms(p_dlb_lip);
    unsigned long long elapsed_time_ms = 0;

    while (!lip_is_request_answered(p_dlb_lip, source))
    {
        if (dlb_lip_osa_wait_condition(&p_dlb_lip->pending_requests.cv, &p_dlb_lip->critical_section, timeout_ms, &elapsed_time_ms))
        {
            // break if we timed out
            break;
        }
        timeout_ms = (uint32_t)(timeout_ms > elapsed_time_ms ? (timeout_ms - elapsed_time_ms) : 0U);
    }
}

static dlb_cec_logical_address_t lip_get_addr_of_pending_request_sent(dlb_lip_t *const p_dlb_lip)
{
    dlb_cec_logical_address_t addr = DLB_LOGICAL_ADDR_UNKNOWN;
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
    {
        if (p_dlb_lip->pending_requests.messages[i].state == LIP_MESSAGE_SENT)
        {
            assert(addr == DLB_LOGICAL_ADDR_UNKNOWN);
            addr = (dlb_cec_logical_address_t)i;
        }
    }
    return addr;
}

static void lip_reschedule_timer(dlb_lip_t *const p_dlb_lip)
{
    const unsigned long long current_time = dlb_lip_osa_get_time_ms();
    uint32_t                 timeout_ms   = LIP_OSA_INFINITE_TIMEOUT;

    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
    {
        if (p_dlb_lip->pending_requests.messages[i].state == LIP_MESSAGE_SENT)
        {
            const uint32_t msg_timeout_ms = p_dlb_lip->pending_requests.messages[i].expire_time_ms > current_time
                                                ? (p_dlb_lip->pending_requests.messages[i].expire_time_ms - current_time)
                                                : 0;
            timeout_ms = timeout_ms > msg_timeout_ms ? msg_timeout_ms : timeout_ms;
        }
    }
    if (timeout_ms != LIP_OSA_INFINITE_TIMEOUT)
    {
        p_dlb_lip->callback_id = dlb_lip_osa_set_timer(&p_dlb_lip->timer, timeout_ms);
    }
    else
    {
        dlb_lip_osa_cancel_timer(&p_dlb_lip->timer);
    }
}

static void lip_handle_pending_requests(dlb_lip_t *const p_dlb_lip)
{
    if (lip_is_any_request_pending(p_dlb_lip))
    {
        for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
        {
            if (p_dlb_lip->pending_requests.messages[i].state == LIP_MESSAGE_PENDING)
            {
                unsigned long long expire_time_ms = p_dlb_lip->pending_requests.messages[i].expire_time_ms;
                // Set state to handled
                p_dlb_lip->pending_requests.messages[i].state = LIP_MESSAGE_HANDLED;
                // If message cannot be handled it will be re-added to pending requests
                lip_cec_cmd_received(p_dlb_lip, &p_dlb_lip->pending_requests.messages[i].msg);
                // Restore correct expire_time
                p_dlb_lip->pending_requests.messages[i].expire_time_ms = expire_time_ms;
            }
        }
    }
}

static void lip_reply_for_pending_cmd_received(
    dlb_lip_t *const                      p_dlb_lip,
    dlb_cec_logical_address_t             source,
    const dlb_lip_pending_message_state_t new_state)
{
    p_dlb_lip->pending_requests.messages[source].state = new_state;
    lip_handle_pending_requests(p_dlb_lip);
    lip_reschedule_timer(p_dlb_lip);
    dlb_lip_osa_broadcast_condition(&p_dlb_lip->pending_requests.cv);
}

static int lip_transmit_request_lip_support(dlb_lip_t *const p_dlb_lip, const dlb_cec_logical_address_t parent_logical_address)
{
    dlb_cec_message_t command = { 0 };

    dlb_lip_build_request_lip_support(&command, p_dlb_lip->cec_bus.logical_address, parent_logical_address);

    return lip_transmit_wrapper(p_dlb_lip, &command);
}

static int lip_transmit_report_lip_support(
    dlb_lip_t *const                p_dlb_lip,
    const dlb_cec_logical_address_t destination,
    const uint32_t                  uuid,
    const bool                      update_uuid)
{
    int               ret     = 0;
    dlb_cec_message_t command = { 0 };

    dlb_lip_build_report_lip_support_cmd(
        &command, p_dlb_lip->cec_bus.logical_address, destination, LIP_PROTOCOL_VERSION, uuid, update_uuid);

    ret = lip_transmit_wrapper(p_dlb_lip, &command);

    return ret;
}

static bool lip_handle_report_lip_support(
    dlb_lip_t *const               p_dlb_lip,
    const dlb_cec_message_t *const command,
    lip_responses_t *              responses,
    const bool                     update_uuid)
{
    bool transmit                                                           = false;
    bool send_update_uuid[NUM_ELEMS(p_dlb_lip->upstream_devices_addresses)] = { 0 };

    const uint8_t  protocol_version = command->data[4];
    const uint32_t uuid
        = ((uint32_t)command->data[5] << 24) | (command->data[6] << 16) | (command->data[7] << 8) | command->data[8];

    (void)protocol_version;

    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
    {
        send_update_uuid[i] = update_uuid;
    }

    if (p_dlb_lip->config_params.downstream_device_addr != command->initiator)
    {
        lip_log_message_n(
            p_dlb_lip,
            "GOT LIP_OPCODE_REPORT_LIP_SUPPORT initiator(0x%x) doesn't match configuration(0x%x), ignore it\n",
            command->initiator,
            p_dlb_lip->config_params.downstream_device_addr);
    }
    else if (!update_uuid && p_dlb_lip->state == LIP_SUPPORTED)
    {
        lip_log_message(p_dlb_lip, "GOT LIP_OPCODE_REPORT_LIP_SUPPORT but STATE == SUPPORTED, ignore it\n");
    }
    else if (update_uuid && p_dlb_lip->state != LIP_SUPPORTED)
    {
        lip_log_message(p_dlb_lip, "GOT UPDATE UUID BUT LIP STATE != SUPPORTED, ignore it\n");
    }
    else
    {
        lip_log_message(p_dlb_lip, "Got LIP_OPCODE_REPORT_LIP_SUPPORT: setting state to LIP_SUPPORTED\n");

        if (uuid != p_dlb_lip->downstream_device_cfg.uuid)
        {
            uint32_t bytes_read = 0;

            // Store cache for old UUID
            if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN
                && p_dlb_lip->callbacks.store_cache_callback)
            {
                p_dlb_lip->callbacks.store_cache_callback(
                    p_dlb_lip->callbacks.arg,
                    p_dlb_lip->downstream_device_cfg.uuid,
                    &p_dlb_lip->downstream_device_cfg.latency_cache,
                    sizeof(p_dlb_lip->downstream_device_cfg.latency_cache));
            }

            // Try to read cache for new UUID
            if (p_dlb_lip->callbacks.read_cache_callback)
            {
                bytes_read = p_dlb_lip->callbacks.read_cache_callback(
                    p_dlb_lip->callbacks.arg,
                    uuid,
                    &p_dlb_lip->downstream_device_cfg.latency_cache,
                    sizeof(p_dlb_lip->downstream_device_cfg.latency_cache));
            }
            // Clear cache if we failed to read whole cache data
            if (bytes_read != sizeof(p_dlb_lip->downstream_device_cfg.latency_cache))
            {
                bool clear_audio_cache = false;
                bool clear_video_cache = false;
                if ((uuid & LIP_UUID_MASK) != (p_dlb_lip->downstream_device_cfg.uuid & LIP_UUID_MASK))
                {
                    // UUID change clear both audio and video caches
                    clear_audio_cache = true;
                    clear_video_cache = true;
                }
                if ((uuid & LIP_UUID_VIDEO_MASK) != (p_dlb_lip->downstream_device_cfg.uuid & LIP_UUID_VIDEO_MASK))
                {
                    // Video rendering mode change - clear video cache
                    clear_video_cache = true;
                }
                if ((uuid & LIP_UUID_AUDIO_MASK) != (p_dlb_lip->downstream_device_cfg.uuid & LIP_UUID_AUDIO_MASK))
                {
                    // Audio rendering mode change - clear audio cache
                    clear_audio_cache = true;
                }

                dlb_cache_clear(&p_dlb_lip->downstream_device_cfg.latency_cache, clear_audio_cache, clear_video_cache);
            }
        }

        p_dlb_lip->downstream_device_cfg.logical_addr = command->initiator;
        p_dlb_lip->downstream_device_cfg.uuid         = uuid;
        p_dlb_lip->state                              = LIP_SUPPORTED;

        // Try to anwser all pending requests
        for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
        {
            if (lip_is_request_pending(p_dlb_lip, i))
            {
                // Try to anwser pending command
                if (lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[i].msg) == LIP_OPCODE_REQUEST_LIP_SUPPORT)
                {
                    p_dlb_lip->pending_requests.messages[i].state = LIP_MESSAGE_HANDLED;
                    send_update_uuid[i]                           = false;
                    // Add new upstream logical address
                    lip_add_upstream_device(p_dlb_lip, p_dlb_lip->pending_requests.messages[i].msg.initiator);
                }
            }
        }

        lip_reschedule_timer(p_dlb_lip);
        lip_status_change_callback(p_dlb_lip);

        if (lip_is_upstream_device_present(p_dlb_lip))
        {
            const uint32_t merged_uuid = lip_get_uuid(p_dlb_lip);

            for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
            {
                if (p_dlb_lip->upstream_devices_addresses[i] != DLB_LOGICAL_ADDR_UNKNOWN)
                {
                    lip_log_message_n(
                        p_dlb_lip, "Sending LIP_OPCODE_UPDATE_UUID to %d\n", p_dlb_lip->upstream_devices_addresses[i]);

                    dlb_lip_build_report_lip_support_cmd(
                        &responses->msg[responses->valid_messages],
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->upstream_devices_addresses[i],
                        LIP_PROTOCOL_VERSION,
                        merged_uuid,
                        send_update_uuid[i]);
                    responses->valid_messages += 1;
                }
            }

            transmit = true;
        }
        if (!update_uuid)
        {
            p_dlb_lip->thread_signaled = true;
            dlb_lip_osa_signal_condition(&p_dlb_lip->condition_var);
        }
    }
    return transmit;
}

static bool
lip_handle_request_lip_support(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command, lip_responses_t *responses)
{
    bool transmit = false;

    lip_log_message_n(p_dlb_lip, "Got LIP_OPCODE_REQUEST_LIP_SUPPORT: current state %s\n", lip_state_description(p_dlb_lip->state));

    switch (p_dlb_lip->state)
    {
    case LIP_SUPPORTED:
    {
        // Reply with LIP_OPCODE_REPORT_LIP_SUPPORT
        const uint32_t uuid = lip_get_uuid(p_dlb_lip);

        // Add upstream logical address
        lip_add_upstream_device(p_dlb_lip, command->initiator);
        lip_status_change_callback(p_dlb_lip);

        lip_log_message_n(p_dlb_lip, "Sending LIP_OPCODE_REPORT_LIP_SUPPORT to %d\n", command->initiator);

        dlb_lip_build_report_lip_support_cmd(
            &responses->msg[responses->valid_messages],
            p_dlb_lip->cec_bus.logical_address,
            command->initiator,
            LIP_PROTOCOL_VERSION,
            uuid,
            false);
        responses->valid_messages += 1;
        transmit = true;
        break;
    }
    default:
    {
        // LIP status is not determined yet - buffer command and reply later
        if (lip_is_request_pending(p_dlb_lip, command->initiator))
        {
            lip_log_message(p_dlb_lip, "Got new request, but old request is still pending - ignoring pending req\n");
        }
        p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_PENDING;
        p_dlb_lip->pending_requests.messages[command->initiator].msg   = *command;
        p_dlb_lip->pending_requests.messages[command->initiator].expire_time_ms
            = dlb_lip_osa_get_time_ms() + lip_get_timeout_value_ms(p_dlb_lip);
        break;
    }
    }

    return transmit;
}

static bool lip_handle_request_av_latency(
    dlb_lip_t *const               p_dlb_lip,
    const dlb_cec_message_t *const command,
    lip_responses_t *              responses,
    bool                           force_ask_downstream)
{
    bool                   transmit     = false;
    dlb_lip_video_format_t video_format = { 0 };
    dlb_lip_audio_format_t audio_format = { 0 };

    video_format.vic = command->data[4];
    get_hdr_mode_from_value_value(command->data[5], &video_format);
    audio_format.codec = (dlb_lip_audio_codec_t)(command->data[6]);

    if (command->msg_length >= 8)
    {
        audio_format.subtype = (dlb_lip_audio_formats_subtypes_t)(command->data[7] & 0x3);
        audio_format.ext     = command->data[7] >> 2;
    }
    if (!lip_is_audio_format_valid(audio_format))
    {
        transmit = true;
        lip_build_abort_cec_command(
            &responses->msg[responses->valid_messages],
            p_dlb_lip->cec_bus.logical_address,
            command->initiator,
            DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
            DLB_CEC_ABORT_REASON_INVALID_OPERAND);
        responses->valid_messages += 1;
        lip_log_message(p_dlb_lip, "Invalid audio format\n");
    }
    else if (!lip_is_video_format_valid(video_format))
    {
        transmit = true;
        lip_build_abort_cec_command(
            &responses->msg[responses->valid_messages],
            p_dlb_lip->cec_bus.logical_address,
            command->initiator,
            DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
            DLB_CEC_ABORT_REASON_INVALID_OPERAND);
        responses->valid_messages += 1;
        lip_log_message(p_dlb_lip, "Invalid video format\n");
    }
    else
    {
        const bool video_renderer            = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) == LIP_VIDEO_RENDERER;
        const bool audio_renderer            = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) == LIP_AUDIO_RENDERER;
        const bool downstream_device_present = p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN;
        const bool ask_downstream_device_for_video      = (!video_renderer && downstream_device_present) || force_ask_downstream;
        const bool ask_downstream_device_for_audio      = (!audio_renderer && downstream_device_present) || force_ask_downstream;
        bool       v_cache_hit                          = false;
        bool       a_cache_hit                          = false;
        dlb_lip_latency_type_t       video_latency_type = (ask_downstream_device_for_video) ? LIP_TOTAL_LATENCY : LIP_OWN_LATENCY;
        dlb_lip_latency_type_t       audio_latency_type = (ask_downstream_device_for_audio) ? LIP_TOTAL_LATENCY : LIP_OWN_LATENCY;
        uint8_t                      audio_latency      = 0;
        uint8_t                      video_latency      = 0;
        const dlb_lip_audio_format_t audio_format_downstream = (p_dlb_lip->config_params.audio_transcoding && !force_ask_downstream)
                                                                   ? p_dlb_lip->config_params.audio_transcoding_format
                                                                   : audio_format;

        v_cache_hit = lip_get_video_latency_from_cache(p_dlb_lip, video_format, video_latency_type, &video_latency);
        a_cache_hit = lip_get_audio_latency_from_cache(
            p_dlb_lip, audio_format, audio_format_downstream, audio_latency_type, &audio_latency);

        if (v_cache_hit && a_cache_hit)
        {
            lip_log_message_n(p_dlb_lip, "%s: cache hit - reply with cached values\n", __func__);

            dlb_lip_build_report_av_latency_cmd(
                &responses->msg[responses->valid_messages],
                p_dlb_lip->cec_bus.logical_address,
                command->initiator,
                video_latency,
                audio_latency);
            responses->valid_messages += 1;
            transmit = true;
        }
        else
        {
            lip_log_message_n(
                p_dlb_lip,
                "%s: cache miss(video_hit=%d audio_hit=%d) adding request to pending list \n",
                __func__,
                v_cache_hit,
                a_cache_hit);
            // Ask downstream device - cache command
            if (lip_is_request_pending(p_dlb_lip, command->initiator))
            {
                lip_log_message(p_dlb_lip, "Got new request, but old request is still pending - ignoring pending req\n");
            }
            p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_PENDING;
            p_dlb_lip->pending_requests.messages[command->initiator].msg   = *command;
            p_dlb_lip->pending_requests.messages[command->initiator].expire_time_ms
                = dlb_lip_osa_get_time_ms() + lip_get_timeout_value_ms(p_dlb_lip);

            if (!lip_is_any_pending_request_sent(p_dlb_lip))
            {
                lip_log_message_n(p_dlb_lip, "%s: Sending pending request\n", __func__);
                p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_SENT;
                p_dlb_lip->req_video_format                                    = video_format;
                p_dlb_lip->req_audio_format                                    = audio_format;
                lip_reschedule_timer(p_dlb_lip);

                if (!v_cache_hit && !a_cache_hit)
                {
                    // Ask for both audio and video
                    dlb_lip_build_request_av_latency(
                        &responses->msg[responses->valid_messages],
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->downstream_device_cfg.logical_addr,
                        p_dlb_lip->req_video_format,
                        audio_format_downstream);
                    responses->valid_messages += 1;
                    transmit = true;
                }
                else if (!v_cache_hit)
                {
                    // We are AVR ask TV for video latency
                    dlb_lip_build_request_video_latency(
                        &responses->msg[responses->valid_messages],
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->downstream_device_cfg.logical_addr,
                        p_dlb_lip->req_video_format);
                    responses->valid_messages += 1;
                    transmit = true;
                }
                else if (!a_cache_hit)
                {
                    dlb_lip_build_request_audio_latency(
                        &responses->msg[responses->valid_messages],
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->downstream_device_cfg.logical_addr,
                        audio_format_downstream);
                    responses->valid_messages += 1;
                    transmit = true;
                }
            }
        }
    }

    return transmit;
}

int dlb_lip_get_av_latency(
    dlb_lip_t *const             p_dlb_lip,
    const dlb_lip_video_format_t video_format,
    const dlb_lip_audio_format_t audio_format,
    uint8_t *const               video_latency,
    uint8_t *const               audio_latency)
{
    int ret = 1;

    if (video_latency && audio_latency && lip_is_audio_format_valid(audio_format) && lip_is_video_format_valid(video_format))
    {
        *video_latency = LIP_INVALID_LATENCY;
        *audio_latency = LIP_INVALID_LATENCY;
        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

        if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN)
        {
            bool request_aborted = false;
            do
            {
                const bool video_latency_valid
                    = lip_get_video_latency_from_cache(p_dlb_lip, video_format, LIP_DOWNSTREAM_LATENCY, video_latency);
                const bool audio_latency_valid = lip_get_audio_latency_from_cache(
                    p_dlb_lip, audio_format, audio_format, LIP_DOWNSTREAM_LATENCY, audio_latency);
                dlb_cec_message_t command = { 0 };
                lip_responses_t   responses;
                bool              transmit        = false;
                bool              wait_for_anwser = false;

                ret = (video_latency_valid && audio_latency_valid) ? 0 : 1;
                if (ret == 0)
                {
                    break;
                }
                responses.valid_messages = 0;

                if (lip_is_request_pending(p_dlb_lip, p_dlb_lip->cec_bus.logical_address))
                {
                    // If there is another command pending wait until it's served
                    dlb_lip_osa_wait_condition(
                        &p_dlb_lip->pending_requests.cv, &p_dlb_lip->critical_section, lip_get_timeout_value_ms(p_dlb_lip), NULL);
                    continue;
                }

                dlb_lip_build_request_av_latency(
                    &command,
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->downstream_device_cfg.logical_addr,
                    video_format,
                    audio_format);

                // Handle command 'send' by ourself
                transmit = lip_handle_request_av_latency(p_dlb_lip, &command, &responses, true);
                if (transmit)
                {
                    if (lip_get_command_opcode(&responses.msg[0]) != LIP_OPCODE_REPORT_AV_LATENCY)
                    {
                        // Cache miss, send request to downstream device and wait for reply
                        if (lip_transmit_wrapper(p_dlb_lip, &responses.msg[0]) == 0)
                        {
                            wait_for_anwser = true;
                        }
                        else
                        {
                            request_aborted = true;
                        }
                    }
                }
                else
                {
                    // It was added to pending list wait for anwser
                    wait_for_anwser = true;
                }
                if (wait_for_anwser)
                {
                    lip_wait_for_pending_request(p_dlb_lip, p_dlb_lip->cec_bus.logical_address);
                    request_aborted = p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state
                                              == LIP_MESSAGE_ANSWER_RECEIVED
                                          ? false
                                          : true;
                    if (request_aborted)
                    {
                        lip_log_message_n(
                            p_dlb_lip,
                            "Waiting for pending request failed(state=%d)!\n",
                            p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state);
                    }

                    p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state = LIP_MESSAGE_HANDLED;
                }

            } while (ret == 1 && request_aborted == false);
        }
        else
        {
            lip_log_message(
                p_dlb_lip, "Unknown logical address of downstream device, message LIP_OPCODE_REQUEST_AV_LATENCY not sent!\n");
        }
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }

    return ret;
}

static bool
lip_handle_report_av_latency(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command, lip_responses_t *responses)
{
    const uint8_t                   video_latency_in     = command->data[4];
    const uint8_t                   audio_latency_in     = command->data[5];
    const dlb_cec_logical_address_t pending_msg_src_addr = lip_get_addr_of_pending_request_sent(p_dlb_lip);
    bool                            transmit             = false;

    dlb_cache_set_video_latency(&p_dlb_lip->downstream_device_cfg.latency_cache, p_dlb_lip->req_video_format, video_latency_in);
    dlb_cache_set_audio_latency(&p_dlb_lip->downstream_device_cfg.latency_cache, p_dlb_lip->req_audio_format, audio_latency_in);

    if (pending_msg_src_addr != DLB_LOGICAL_ADDR_UNKNOWN)
    {
        const lip_cec_opcode_t opcode  = lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg);
        bool                   handled = false;

        if (opcode == LIP_OPCODE_REQUEST_VIDEO_LATENCY || opcode == LIP_OPCODE_REQUEST_AUDIO_LATENCY
            || opcode == LIP_OPCODE_REQUEST_AV_LATENCY)
        {
            if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator != p_dlb_lip->cec_bus.logical_address)
            {
                const dlb_lip_latency_type_t audio_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_latency_type_t video_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_audio_format_t audio_format_downstream = p_dlb_lip->config_params.audio_transcoding
                                                                           ? p_dlb_lip->config_params.audio_transcoding_format
                                                                           : p_dlb_lip->req_audio_format;
                uint8_t video_latency = LIP_INVALID_LATENCY;
                uint8_t audio_latency = LIP_INVALID_LATENCY;

                handled = true;

                lip_get_video_latency_from_cache(p_dlb_lip, p_dlb_lip->req_video_format, video_latency_type, &video_latency);
                lip_get_audio_latency_from_cache(
                    p_dlb_lip, p_dlb_lip->req_audio_format, audio_format_downstream, audio_latency_type, &audio_latency);

                dlb_lip_build_report_av_latency_cmd(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                    video_latency,
                    audio_latency);
                responses->valid_messages += 1;
                transmit = true;
            }
        }
        lip_reply_for_pending_cmd_received(
            p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ANSWER_RECEIVED);
    }

    return transmit;
}

static bool lip_handle_request_audio_latency(
    dlb_lip_t *const               p_dlb_lip,
    const dlb_cec_message_t *const command,
    lip_responses_t *              responses,
    bool                           force_ask_downstream)
{
    bool                   transmit     = false;
    dlb_lip_audio_format_t audio_format = { 0 };
    audio_format.codec                  = (dlb_lip_audio_codec_t)(command->data[4]);

    if (command->msg_length >= 6)
    {
        audio_format.subtype = (dlb_lip_audio_formats_subtypes_t)(command->data[5] & 0x3);
        audio_format.ext     = command->data[5] >> 2;
    }

    if (!lip_is_audio_format_valid(audio_format))
    {
        transmit = true;
        lip_build_abort_cec_command(
            &responses->msg[responses->valid_messages],
            p_dlb_lip->cec_bus.logical_address,
            command->initiator,
            DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
            DLB_CEC_ABORT_REASON_INVALID_OPERAND);
        responses->valid_messages += 1;
        lip_log_message(p_dlb_lip, "Invalid audio format\n");
    }
    else
    {
        const bool audio_renderer            = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) == LIP_AUDIO_RENDERER;
        const bool downstream_device_present = p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN;
        bool       ask_downstream_device     = (!audio_renderer && downstream_device_present) || force_ask_downstream;
        const dlb_lip_latency_type_t audio_latency_type      = (ask_downstream_device) ? LIP_TOTAL_LATENCY : LIP_OWN_LATENCY;
        bool                         cache_hit               = false;
        uint8_t                      audio_latency           = 0;
        const dlb_lip_audio_format_t audio_format_downstream = (p_dlb_lip->config_params.audio_transcoding && !force_ask_downstream)
                                                                   ? p_dlb_lip->config_params.audio_transcoding_format
                                                                   : audio_format;

        cache_hit = lip_get_audio_latency_from_cache(
            p_dlb_lip, audio_format, audio_format_downstream, audio_latency_type, &audio_latency);

        if (cache_hit)
        {
            dlb_lip_build_report_audio_latency_cmd(
                &responses->msg[responses->valid_messages], p_dlb_lip->cec_bus.logical_address, command->initiator, audio_latency);
            responses->valid_messages += 1;
            transmit = true;
        }
        else
        {
            p_dlb_lip->req_audio_format = audio_format;

            // cache command
            if (lip_is_request_pending(p_dlb_lip, command->initiator))
            {
                lip_log_message(p_dlb_lip, "Got new request, but old request is still pending - ignoring pending req\n");
            }
            p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_PENDING;
            p_dlb_lip->pending_requests.messages[command->initiator].msg   = *command;
            p_dlb_lip->pending_requests.messages[command->initiator].expire_time_ms
                = dlb_lip_osa_get_time_ms() + lip_get_timeout_value_ms(p_dlb_lip);

            if (!lip_is_any_pending_request_sent(p_dlb_lip))
            {
                p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_SENT;

                lip_reschedule_timer(p_dlb_lip);

                dlb_lip_build_request_audio_latency(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->downstream_device_cfg.logical_addr,
                    audio_format_downstream);
                responses->valid_messages += 1;
                transmit = true;
            }
        }
    }

    return transmit;
}

int dlb_lip_get_audio_latency(dlb_lip_t *const p_dlb_lip, const dlb_lip_audio_format_t audio_format, uint8_t *const audio_latency)
{
    int ret = 1;

    if (audio_latency && lip_is_audio_format_valid(audio_format))
    {
        *audio_latency = LIP_INVALID_LATENCY;

        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

        if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN)
        {
            bool request_aborted = false;
            do
            {
                const bool cache_hit = lip_get_audio_latency_from_cache(
                    p_dlb_lip, audio_format, audio_format, LIP_DOWNSTREAM_LATENCY, audio_latency);
                dlb_cec_message_t command = { 0 };
                lip_responses_t   responses;
                bool              transmit        = false;
                bool              wait_for_anwser = false;
                responses.valid_messages          = 0;
                ret                               = cache_hit ? 0 : 1;
                if (ret == 0)
                {
                    break;
                }

                if (lip_is_request_pending(p_dlb_lip, p_dlb_lip->cec_bus.logical_address))
                {
                    // If there is another command pending wait until it's served
                    dlb_lip_osa_wait_condition(
                        &p_dlb_lip->pending_requests.cv, &p_dlb_lip->critical_section, lip_get_timeout_value_ms(p_dlb_lip), NULL);
                    continue;
                }

                dlb_lip_build_request_audio_latency(
                    &command, p_dlb_lip->cec_bus.logical_address, p_dlb_lip->downstream_device_cfg.logical_addr, audio_format);

                // Handle command 'send' by ourself
                transmit = lip_handle_request_audio_latency(p_dlb_lip, &command, &responses, true);
                if (transmit)
                {
                    if (lip_get_command_opcode(&responses.msg[0]) != LIP_OPCODE_REPORT_AUDIO_LATENCY)
                    {
                        // Cache miss, send request to downstream device and wait for reply
                        if (lip_transmit_wrapper(p_dlb_lip, &responses.msg[0]) == 0)
                        {
                            wait_for_anwser = true;
                        }
                        else
                        {
                            request_aborted = true;
                        }
                    }
                }
                else
                {
                    // It was added to pending list wait for anwser
                    wait_for_anwser = true;
                }
                if (wait_for_anwser)
                {
                    lip_wait_for_pending_request(p_dlb_lip, p_dlb_lip->cec_bus.logical_address);
                    request_aborted = p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state
                                              == LIP_MESSAGE_ANSWER_RECEIVED
                                          ? false
                                          : true;
                    p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state = LIP_MESSAGE_HANDLED;
                }
            } while (ret == 1 && request_aborted == false);
        }
        else
        {
            lip_log_message(
                p_dlb_lip, "Unknown logical address of downstream device, message LIP_OPCODE_REQUEST_AUDIO_LATENCY not sent!\n");
        }
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }

    return ret;
}

static bool
lip_handle_report_audio_latency(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command, lip_responses_t *responses)
{
    const dlb_cec_logical_address_t pending_msg_src_addr = lip_get_addr_of_pending_request_sent(p_dlb_lip);
    bool                            transmit             = false;
    dlb_cache_set_audio_latency(&p_dlb_lip->downstream_device_cfg.latency_cache, p_dlb_lip->req_audio_format, command->data[4]);

    // Report upstream
    if (pending_msg_src_addr != DLB_LOGICAL_ADDR_UNKNOWN)
    {
        const lip_cec_opcode_t opcode = lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg);

        if (opcode == LIP_OPCODE_REQUEST_VIDEO_LATENCY)
        {
            lip_log_message(
                p_dlb_lip,
                "Pending CMD is LIP_OPCODE_REQUEST_VIDEO_LATENCY but we recived "
                "LIP_OPCODE_REPORT_AUDIO_LATENCY");
            lip_reply_for_pending_cmd_received(p_dlb_lip, pending_msg_src_addr, LIP_MESSAGE_HANDLED);
        }
        else if (opcode == LIP_OPCODE_REQUEST_AUDIO_LATENCY)
        {
            bool handled = false;
            if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator != p_dlb_lip->cec_bus.logical_address)
            {
                const dlb_lip_latency_type_t audio_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_audio_format_t audio_format_downstream = p_dlb_lip->config_params.audio_transcoding
                                                                           ? p_dlb_lip->config_params.audio_transcoding_format
                                                                           : p_dlb_lip->req_audio_format;
                uint8_t audio_latency = LIP_INVALID_LATENCY;
                lip_get_audio_latency_from_cache(
                    p_dlb_lip, p_dlb_lip->req_audio_format, audio_format_downstream, audio_latency_type, &audio_latency);
                handled  = true;
                transmit = true;
                dlb_lip_build_report_audio_latency_cmd(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                    audio_latency);
                responses->valid_messages += 1;
            }
            lip_reply_for_pending_cmd_received(
                p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ANSWER_RECEIVED);
        }
        else if (opcode == LIP_OPCODE_REQUEST_AV_LATENCY)
        {
            bool handled = false;

            if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator != p_dlb_lip->cec_bus.logical_address)
            {
                const dlb_lip_latency_type_t audio_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_latency_type_t video_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_audio_format_t audio_format_downstream = p_dlb_lip->config_params.audio_transcoding
                                                                           ? p_dlb_lip->config_params.audio_transcoding_format
                                                                           : p_dlb_lip->req_audio_format;
                uint8_t audio_latency = LIP_INVALID_LATENCY;
                uint8_t video_latency = LIP_INVALID_LATENCY;

                lip_get_audio_latency_from_cache(
                    p_dlb_lip, p_dlb_lip->req_audio_format, audio_format_downstream, audio_latency_type, &audio_latency);
                lip_get_video_latency_from_cache(p_dlb_lip, p_dlb_lip->req_video_format, video_latency_type, &video_latency);

                dlb_lip_build_report_av_latency_cmd(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                    video_latency,
                    audio_latency);
                responses->valid_messages += 1;
                transmit = true;
                handled  = true;
            }
            lip_reply_for_pending_cmd_received(
                p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ANSWER_RECEIVED);
        }
    }
    return transmit;
}

static bool lip_handle_request_video_latency(
    dlb_lip_t *const               p_dlb_lip,
    const dlb_cec_message_t *const command,
    lip_responses_t *              responses,
    bool                           force_ask_downstream)
{
    bool                   transmit     = false;
    dlb_lip_video_format_t video_format = { 0 };

    video_format.vic = command->data[4];
    get_hdr_mode_from_value_value(command->data[5], &video_format);

    if (!lip_is_video_format_valid(video_format))
    {
        transmit = true;
        lip_build_abort_cec_command(
            &responses->msg[responses->valid_messages],
            p_dlb_lip->cec_bus.logical_address,
            command->initiator,
            DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
            DLB_CEC_ABORT_REASON_INVALID_OPERAND);
        responses->valid_messages += 1;
        lip_log_message(p_dlb_lip, "Invalid video format\n");
    }
    else
    {
        const bool video_renderer            = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) == LIP_VIDEO_RENDERER;
        const bool downstream_device_present = p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN;

        const bool                   ask_downstream_device = (!video_renderer && downstream_device_present) || force_ask_downstream;
        const dlb_lip_latency_type_t video_latency_type    = ask_downstream_device ? LIP_TOTAL_LATENCY : LIP_OWN_LATENCY;
        bool                         cache_hit             = false;
        uint8_t                      video_latency         = LIP_INVALID_LATENCY;

        p_dlb_lip->req_video_format = video_format;
        cache_hit = lip_get_video_latency_from_cache(p_dlb_lip, p_dlb_lip->req_video_format, video_latency_type, &video_latency);

        if (cache_hit)
        {
            dlb_lip_build_report_video_latency_cmd(
                &responses->msg[responses->valid_messages], p_dlb_lip->cec_bus.logical_address, command->initiator, video_latency);
            responses->valid_messages += 1;
            transmit = true;
        }
        else
        {
            // cache command
            if (lip_is_request_pending(p_dlb_lip, command->initiator))
            {
                lip_log_message(p_dlb_lip, "Got new request, but old request is still pending - ignoring pending req\n");
            }
            p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_PENDING;
            p_dlb_lip->pending_requests.messages[command->initiator].msg   = *command;
            p_dlb_lip->pending_requests.messages[command->initiator].expire_time_ms
                = dlb_lip_osa_get_time_ms() + lip_get_timeout_value_ms(p_dlb_lip);

            if (!lip_is_any_pending_request_sent(p_dlb_lip))
            {
                p_dlb_lip->pending_requests.messages[command->initiator].state = LIP_MESSAGE_SENT;

                lip_reschedule_timer(p_dlb_lip);

                // We are AVR ask TV for video latency
                dlb_lip_build_request_video_latency(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->downstream_device_cfg.logical_addr,
                    p_dlb_lip->req_video_format);
                responses->valid_messages += 1;
                transmit = true;
            }
        }
    }

    return transmit;
}

int dlb_lip_get_video_latency(dlb_lip_t *const p_dlb_lip, const dlb_lip_video_format_t video_format, uint8_t *const video_latency)
{
    int ret = 1;

    if (video_latency && lip_is_video_format_valid(video_format))
    {
        *video_latency = LIP_INVALID_LATENCY;

        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

        if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN)
        {
            bool request_aborted = false;

            do
            {
                dlb_cec_message_t command = { 0 };
                lip_responses_t   responses;
                bool cache_hit = lip_get_video_latency_from_cache(p_dlb_lip, video_format, LIP_DOWNSTREAM_LATENCY, video_latency);
                bool transmit  = false;
                bool wait_for_anwser     = false;
                responses.valid_messages = 0;

                ret = cache_hit ? 0 : 1;
                if (ret == 0)
                {
                    break;
                }

                if (lip_is_request_pending(p_dlb_lip, p_dlb_lip->cec_bus.logical_address))
                {
                    // If there is another command pending wait until it's served
                    dlb_lip_osa_wait_condition(
                        &p_dlb_lip->pending_requests.cv, &p_dlb_lip->critical_section, lip_get_timeout_value_ms(p_dlb_lip), NULL);
                    continue;
                }

                dlb_lip_build_request_video_latency(
                    &command, p_dlb_lip->cec_bus.logical_address, p_dlb_lip->downstream_device_cfg.logical_addr, video_format);

                // Handle command 'send' by ourself
                transmit = lip_handle_request_video_latency(p_dlb_lip, &command, &responses, true);
                if (transmit)
                {
                    if (lip_get_command_opcode(&responses.msg[0]) != LIP_OPCODE_REPORT_VIDEO_LATENCY)
                    {
                        // Cache miss, send request to downstream device and wait for reply
                        if (lip_transmit_wrapper(p_dlb_lip, &responses.msg[0]) == 0)
                        {
                            wait_for_anwser = true;
                        }
                        else
                        {
                            request_aborted = true;
                        }
                    }
                }
                else
                {
                    // It was added to pending list wait for anwser
                    wait_for_anwser = true;
                }
                if (wait_for_anwser)
                {
                    lip_wait_for_pending_request(p_dlb_lip, p_dlb_lip->cec_bus.logical_address);
                    request_aborted = p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state
                                              == LIP_MESSAGE_ANSWER_RECEIVED
                                          ? false
                                          : true;
                    p_dlb_lip->pending_requests.messages[p_dlb_lip->cec_bus.logical_address].state = LIP_MESSAGE_HANDLED;
                }
            } while (ret == 1 && request_aborted == false);
        }
        else
        {
            lip_log_message(
                p_dlb_lip, "Unknown logical address of downstream device, message LIP_OPCODE_REQUEST_VIDEO_LATENCY not sent!\n");
        }
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }

    return ret;
}

static bool
lip_handle_report_video_latency(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command, lip_responses_t *responses)
{
    const dlb_cec_logical_address_t pending_msg_src_addr = lip_get_addr_of_pending_request_sent(p_dlb_lip);
    const uint8_t                   video_latency_in     = command->data[4];
    bool                            transmit             = false;

    dlb_cache_set_video_latency(&p_dlb_lip->downstream_device_cfg.latency_cache, p_dlb_lip->req_video_format, video_latency_in);

    if (pending_msg_src_addr != DLB_LOGICAL_ADDR_UNKNOWN)
    {
        const lip_cec_opcode_t opcode = lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg);

        if (opcode == LIP_OPCODE_REQUEST_VIDEO_LATENCY)
        {
            bool handled = false;
            if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator != p_dlb_lip->cec_bus.logical_address)
            {
                const dlb_lip_latency_type_t video_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;

                uint8_t video_latency = LIP_INVALID_LATENCY;
                lip_get_video_latency_from_cache(p_dlb_lip, p_dlb_lip->req_video_format, video_latency_type, &video_latency);

                dlb_lip_build_report_video_latency_cmd(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                    video_latency);
                responses->valid_messages += 1;
                transmit = true;
                handled  = true;
            }
            lip_reply_for_pending_cmd_received(
                p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ANSWER_RECEIVED);
        }
        else if (opcode == LIP_OPCODE_REQUEST_AUDIO_LATENCY)
        {
            lip_log_message(
                p_dlb_lip,
                "Pending CMD is LIP_OPCODE_REQUEST_AUDIO_LATENCY but we recived "
                "LIP_OPCODE_REPORT_VIDEO_LATENCY\n");
            lip_reply_for_pending_cmd_received(p_dlb_lip, pending_msg_src_addr, LIP_MESSAGE_HANDLED);
        }
        else if (opcode == LIP_OPCODE_REQUEST_AV_LATENCY)
        {
            bool handled = false;
            if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator != p_dlb_lip->cec_bus.logical_address)
            {
                const dlb_lip_latency_type_t audio_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_AUDIO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                const dlb_lip_latency_type_t video_latency_type
                    = (p_dlb_lip->config_params.render_mode & LIP_VIDEO_RENDERER) ? LIP_OWN_LATENCY : LIP_TOTAL_LATENCY;
                dlb_lip_audio_format_t audio_format_downstream = p_dlb_lip->config_params.audio_transcoding
                                                                     ? p_dlb_lip->config_params.audio_transcoding_format
                                                                     : p_dlb_lip->req_audio_format;

                uint8_t audio_latency = LIP_INVALID_LATENCY;
                uint8_t video_latency = LIP_INVALID_LATENCY;
                lip_get_audio_latency_from_cache(
                    p_dlb_lip, p_dlb_lip->req_audio_format, audio_format_downstream, audio_latency_type, &audio_latency);
                lip_get_video_latency_from_cache(p_dlb_lip, p_dlb_lip->req_video_format, video_latency_type, &video_latency);

                dlb_lip_build_report_av_latency_cmd(
                    &responses->msg[responses->valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                    video_latency,
                    audio_latency);
                responses->valid_messages += 1;
                transmit = true;
                handled  = true;
            }
            lip_reply_for_pending_cmd_received(
                p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ANSWER_RECEIVED);
        }
    }

    return transmit;
}

static bool lip_can_handle_opcode_in_current_state(dlb_lip_t *const p_dlb_lip, lip_cec_opcode_t lip_opcode)
{
    bool ret = false;

    switch (lip_opcode)
    {
    case LIP_OPCODE_REPORT_LIP_SUPPORT:
    case LIP_OPCODE_REQUEST_LIP_SUPPORT:
    {
        // Can't handle those command in LIP_UNSUPPORTED state
        ret = p_dlb_lip->state != LIP_UNSUPPORTED;
        break;
    }

    case LIP_OPCODE_UPDATE_UUID:
    case LIP_OPCODE_REQUEST_AV_LATENCY:
    case LIP_OPCODE_REPORT_AV_LATENCY:
    case LIP_OPCODE_REQUEST_AUDIO_LATENCY:
    case LIP_OPCODE_REPORT_AUDIO_LATENCY:
    case LIP_OPCODE_REQUEST_VIDEO_LATENCY:
    case LIP_OPCODE_REPORT_VIDEO_LATENCY:
    {
        // Can handle those commands only in LIP_SUPPORTED state
        ret = p_dlb_lip->state == LIP_SUPPORTED;
        break;
    }
    default:
    {
        ret = false;
        break;
    }
    }
    return ret;
}

static bool lip_handle_feature_abort(
    dlb_lip_t *const               p_dlb_lip,
    const dlb_cec_message_t *const command,
    lip_responses_t *              responses,
    int *                          message_consumed)
{
    bool transmit = false;

    if (command->msg_length >= 1 && command->data[0] == DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID)
    {
        const lip_cec_opcode_t lip_opcode = p_dlb_lip->opcode_of_last_cmd_sent[command->initiator];

        switch (lip_opcode)
        {
        case LIP_OPCODE_REQUEST_LIP_SUPPORT:
        {
            if (p_dlb_lip->state == LIP_WAIT_FOR_REPLY)
            {
                *message_consumed          = 1;
                p_dlb_lip->thread_signaled = true;
                dlb_lip_osa_signal_condition(&p_dlb_lip->condition_var);
            }
            break;
        }
        case LIP_OPCODE_REQUEST_VIDEO_LATENCY:
        case LIP_OPCODE_REQUEST_AUDIO_LATENCY:
        case LIP_OPCODE_REQUEST_AV_LATENCY:
        {
            const dlb_cec_logical_address_t pending_msg_src_addr = lip_get_addr_of_pending_request_sent(p_dlb_lip);
            const lip_cec_opcode_t          lip_pending_opcode
                = pending_msg_src_addr != DLB_LOGICAL_ADDR_UNKNOWN
                      ? lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg)
                      : LIP_OPCODES;
            if (lip_pending_opcode == LIP_OPCODE_REQUEST_AV_LATENCY || lip_pending_opcode == LIP_OPCODE_REQUEST_AUDIO_LATENCY
                || lip_pending_opcode == LIP_OPCODE_REQUEST_VIDEO_LATENCY)
            {
                bool handled      = false;
                *message_consumed = 1;

                if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator == p_dlb_lip->cec_bus.logical_address)
                {
                    // Handled internaly by dlb_lip_get_{a/v}_latency
                }
                else
                {
                    lip_build_abort_cec_command(
                        &responses->msg[responses->valid_messages],
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                        DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                        DLB_CEC_ABORT_REASON_REFUSED);

                    responses->valid_messages += 1;
                    transmit = true;
                    handled  = true;
                }
                lip_reply_for_pending_cmd_received(
                    p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ABORT_RECEIVED);
            }
            break;
        }
        default:
            break;
        }
    }
    return transmit;
}

static int lip_cec_cmd_received(dlb_lip_t *const p_dlb_lip, const dlb_cec_message_t *const command)
{
    lip_responses_t responses;
    bool            transmit         = false;
    int             message_consumed = 0;
    responses.valid_messages         = 0;

    switch (command->opcode)
    {
    case DLB_CEC_OPCODE_FEATURE_ABORT:
    {
        transmit = lip_handle_feature_abort(p_dlb_lip, command, &responses, &message_consumed);
        break;
    }
    case DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID:
    {
        if (lip_validate_cmd_vendor_id(command))
        {
            const lip_cec_opcode_t lip_opcode = lip_get_command_opcode(command);

            message_consumed = 1;

            if (lip_opcode == LIP_OPCODES)
            {
                lip_log_message_n(p_dlb_lip, "Got unknown LIP opcode(%x)\n", lip_opcode);

                lip_build_abort_cec_command(
                    &responses.msg[responses.valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    command->initiator,
                    DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                    DLB_CEC_ABORT_REASON_UNRECOGNIZED_OPCODE);
                responses.valid_messages += 1;
                transmit = true;
            }
            else if (command->destination == DLB_LOGICAL_ADDR_BROADCAST || command->initiator == DLB_LOGICAL_ADDR_BROADCAST)
            {
                lip_log_message_n(
                    p_dlb_lip,
                    "LIP commands shouldn't be broadcasted! CMD=%u initiator=%u destination=%u\n",
                    lip_opcode,
                    command->initiator,
                    command->destination);
            }
            else if (command->destination != p_dlb_lip->cec_bus.logical_address)
            {
                // Message is not addressed to us, skip it
                lip_log_message_n(
                    p_dlb_lip,
                    "Message is not addressed to us(dest:%x own address:%x\n",
                    command->destination,
                    p_dlb_lip->cec_bus.logical_address);
            }
            else if (command->msg_length < lip_get_command_min_length(lip_opcode))
            {
                lip_log_message_n(
                    p_dlb_lip,
                    "Invalid command length for opcode %u, got: %u, but expected: %u\n",
                    lip_opcode,
                    command->msg_length,
                    lip_get_command_min_length(lip_opcode));

                lip_build_abort_cec_command(
                    &responses.msg[responses.valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    command->initiator,
                    DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                    DLB_CEC_ABORT_REASON_INVALID_OPERAND);
                responses.valid_messages += 1;
                transmit = true;
            }
            else if (!lip_can_handle_opcode_in_current_state(p_dlb_lip, lip_opcode))
            {
                lip_log_message_n(
                    p_dlb_lip, "Can't handle opcode %u in %s state\n", lip_opcode, lip_state_description(p_dlb_lip->state));

                lip_build_abort_cec_command(
                    &responses.msg[responses.valid_messages],
                    p_dlb_lip->cec_bus.logical_address,
                    command->initiator,
                    DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                    DLB_CEC_ABORT_REASON_NOT_IN_CORRECT_MODE_TO_RESPOND);
                responses.valid_messages += 1;
                transmit = true;
            }
            else
            {
                switch (lip_opcode)
                {
                case LIP_OPCODE_REPORT_LIP_SUPPORT:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REPORT_LIP_SUPPORT received\n");
                    transmit = lip_handle_report_lip_support(p_dlb_lip, command, &responses, false);
                    break;
                }
                case LIP_OPCODE_UPDATE_UUID:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_UPDATE_UUID received\n");
                    transmit = lip_handle_report_lip_support(p_dlb_lip, command, &responses, true);
                    break;
                }
                case LIP_OPCODE_REQUEST_LIP_SUPPORT:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REQUEST_LIP_SUPPORT received, answering\n");
                    transmit = lip_handle_request_lip_support(p_dlb_lip, command, &responses);
                    break;
                }
                case LIP_OPCODE_REQUEST_AV_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REQUEST_AV_LATENCY received\n");
                    transmit = lip_handle_request_av_latency(p_dlb_lip, command, &responses, false);
                    break;
                }
                case LIP_OPCODE_REPORT_AV_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REPORT_AV_LATENCY received\n");
                    transmit = lip_handle_report_av_latency(p_dlb_lip, command, &responses);
                    break;
                }
                case LIP_OPCODE_REQUEST_AUDIO_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REQUEST_AUDIO_LATENCY received\n");
                    transmit = lip_handle_request_audio_latency(p_dlb_lip, command, &responses, false);
                    break;
                }
                case LIP_OPCODE_REPORT_AUDIO_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REPORT_AUDIO_LATENCY received\n");
                    transmit = lip_handle_report_audio_latency(p_dlb_lip, command, &responses);
                    break;
                }
                case LIP_OPCODE_REQUEST_VIDEO_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REQUEST_VIDEO_LATENCY received\n");
                    transmit = lip_handle_request_video_latency(p_dlb_lip, command, &responses, false);
                    break;
                }
                case LIP_OPCODE_REPORT_VIDEO_LATENCY:
                {
                    lip_log_message(p_dlb_lip, "Command callback: LIP_OPCODE_REPORT_VIDEO_LATENCY received\n");
                    transmit = lip_handle_report_video_latency(p_dlb_lip, command, &responses);
                    break;
                }
                default:
                {
                    lip_log_message_n(p_dlb_lip, "Got unknown LIP opcode(%x)\n", lip_opcode);
                    assert(!"Unhandled LIP opcode");
                    break;
                }
                }
            }
        }
        break;
    }

    default:
        transmit = false;
        break;
    }

    if (transmit)
    {
        for (unsigned int i = 0; i < responses.valid_messages; i += 1)
        {
            if (lip_transmit_wrapper(p_dlb_lip, &responses.msg[i]))
            {
                lip_log_message(p_dlb_lip, "Message transmit failed\n");
            }
        }
    }

    return message_consumed;
}

static int dlb_lip_cec_cmd_received(void *cb_param, const dlb_cec_message_t *const command)
{
    dlb_lip_t *const p_dlb_lip = (dlb_lip_t *const)cb_param;
    int              ret       = 0;
    dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

    ret = lip_cec_cmd_received(p_dlb_lip, command);

    dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);

    return ret;
}

static bool lip_is_running(dlb_lip_t *const p_dlb_lip)
{
    return p_dlb_lip->is_running;
}

#if defined(_MSC_VER)
static void
#else
static void *
#endif
dlb_lip_request_thread(void *data)
{
    dlb_lip_t *const p_dlb_lip        = (dlb_lip_t *const)data;
    uint32_t         timeout_ms       = 0;
    bool             transmit_success = false;

    dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

    while (lip_is_running(p_dlb_lip))
    {
        int         timed_out  = 0;
        lip_state_t prev_state = p_dlb_lip->state;
        while (timed_out == 0 && p_dlb_lip->thread_signaled == false && lip_is_running(p_dlb_lip))
        {
            unsigned long long elapsed_time_ms = 0;
            timed_out
                = dlb_lip_osa_wait_condition(&p_dlb_lip->condition_var, &p_dlb_lip->critical_section, timeout_ms, &elapsed_time_ms);
            if (timeout_ms != LIP_OSA_INFINITE_TIMEOUT)
            {
                timeout_ms = (uint32_t)((elapsed_time_ms > timeout_ms) ? 0 : timeout_ms - elapsed_time_ms);
            }
        }
        p_dlb_lip->thread_signaled = false;

        if (!lip_is_running(p_dlb_lip))
        {
            break;
        }
        switch (p_dlb_lip->state)
        {
        case LIP_INIT:
        {
            // Source device should check LIP support of downstream devices
            int isSourceDevice = p_dlb_lip->config_params.downstream_device_addr != DLB_LOGICAL_ADDR_UNKNOWN;

            if (isSourceDevice)
            {
                lip_log_message(p_dlb_lip, "Sending LIP_OPCODE_REQUEST_LIP_SUPPORT\n");

                p_dlb_lip->state = LIP_WAIT_FOR_REPLY;
                timeout_ms       = lip_get_timeout_value_ms(p_dlb_lip);

                transmit_success
                    = lip_transmit_request_lip_support(p_dlb_lip, p_dlb_lip->config_params.downstream_device_addr) == 0;
            }
            else
            {
                p_dlb_lip->state = LIP_SUPPORTED;
                timeout_ms       = 0;
            }

            break;
        }
        case LIP_WAIT_FOR_REPLY:
        {
            if (transmit_success && p_dlb_lip->cec_bus.logical_address == DLB_LOGICAL_ADDR_TV
                && p_dlb_lip->config_params.downstream_device_addr == DLB_LOGICAL_ADDR_AUDIOSYSTEM)
            {
                p_dlb_lip->state                    = LIP_SUPPORTED;
                p_dlb_lip->add_iec_decoding_latency = true;
            }
            else
            {
                p_dlb_lip->state = LIP_UNSUPPORTED;
            }
            timeout_ms = 0;
            break;
        }
        case LIP_SUPPORTED:
        {
            if (lip_is_any_request_pending(p_dlb_lip))
            {
                for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
                {
                    // Try to anwser pending command
                    if (lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[i].msg) == LIP_OPCODE_REQUEST_LIP_SUPPORT)
                    {
                        lip_responses_t responses;
                        responses.valid_messages = 0;

                        p_dlb_lip->pending_requests.messages[i].state = LIP_MESSAGE_HANDLED;
                        if (lip_handle_request_lip_support(p_dlb_lip, &p_dlb_lip->pending_requests.messages[i].msg, &responses))
                        {
                            for (unsigned int j = 0; j < responses.valid_messages; j += 1)
                            {
                                lip_transmit_wrapper(p_dlb_lip, &responses.msg[j]);
                            }
                        }
                    }
                }
            }

            timeout_ms = LIP_OSA_INFINITE_TIMEOUT;
            break;
        }
        case LIP_UNSUPPORTED:
        {
            if (lip_is_any_request_pending(p_dlb_lip))
            {
                for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
                {
                    if (lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[i].msg) == LIP_OPCODE_REQUEST_LIP_SUPPORT)
                    {
                        dlb_cec_message_t response                    = { 0 };
                        p_dlb_lip->pending_requests.messages[i].state = LIP_MESSAGE_HANDLED;

                        lip_build_abort_cec_command(
                            &response,
                            p_dlb_lip->cec_bus.logical_address,
                            p_dlb_lip->pending_requests.messages[i].msg.initiator,
                            DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                            DLB_CEC_ABORT_REASON_REFUSED);
                        lip_transmit_wrapper(p_dlb_lip, &response);

                        lip_log_message_n(
                            p_dlb_lip,
                            "New state is LIP_UNSUPPORTED, reply with feature abort to 0x%x\n",
                            p_dlb_lip->pending_requests.messages[i].msg.initiator);
                    }
                }
            }

            timeout_ms = LIP_OSA_INFINITE_TIMEOUT;
            break;
        }
        default:
        {
            assert(!"dlb_lip_request_thread: unhandled case");
            p_dlb_lip->state = LIP_INIT;
            break;
        }
        }
        if (p_dlb_lip->state == LIP_SUPPORTED || p_dlb_lip->state == LIP_UNSUPPORTED)
        {
            dlb_lip_osa_signal_condition(&p_dlb_lip->state_updated_condition_var);
        }
        lip_log_message_n(
            p_dlb_lip,
            "LIP state change(%s): %s -> %s\n",
            timed_out ? "timeout" : "signaled",
            lip_state_description(prev_state),
            lip_state_description(p_dlb_lip->state));
    }
    dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
#if !defined(_MSC_VER)
    return NULL;
#endif
}

static int dlb_lip_timer_callback(void *arg, uint32_t callback_id)
{
    dlb_lip_t *const  p_dlb_lip = (dlb_lip_t *const)arg;
    dlb_cec_message_t response  = { 0 };
    bool              transmit  = false;
    bool              ret       = 0;

    if (dlb_lip_osa_try_enter_critial_section(&p_dlb_lip->critical_section))
    {
        if (p_dlb_lip->callback_id == callback_id)
        {
            dlb_cec_logical_address_t pending_msg_src_addr = lip_get_addr_of_pending_request_sent(p_dlb_lip);
            if (pending_msg_src_addr != DLB_LOGICAL_ADDR_UNKNOWN)
            {
                const lip_cec_opcode_t opcode
                    = lip_get_command_opcode(&p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg);
                bool handled = false;

                if (p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator == p_dlb_lip->cec_bus.logical_address)
                {
                    lip_log_message_n(p_dlb_lip, "Timeout, no reply within %u miliseconds\n", lip_get_timeout_value_ms(p_dlb_lip));
                }
                else if (
                    opcode == LIP_OPCODE_REQUEST_LIP_SUPPORT || opcode == LIP_OPCODE_REQUEST_VIDEO_LATENCY
                    || opcode == LIP_OPCODE_REQUEST_AUDIO_LATENCY || opcode == LIP_OPCODE_REQUEST_AV_LATENCY)
                {
                    // 4.2 To avoid deadlocks, it is recommended to cancel the pending request by sending a <feature abort> [0xA0]
                    // if no answer comes within 2 seconds.
                    lip_build_abort_cec_command(
                        &response,
                        p_dlb_lip->cec_bus.logical_address,
                        p_dlb_lip->pending_requests.messages[pending_msg_src_addr].msg.initiator,
                        DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID,
                        DLB_CEC_ABORT_REASON_REFUSED);

                    lip_log_message_n(
                        p_dlb_lip,
                        "Timeout, no reply within %u miliseconds transmitting feature abort from: %d to %d\n",
                        lip_get_timeout_value_ms(p_dlb_lip),
                        response.initiator,
                        response.destination);

                    transmit = true;
                    handled  = true;
                }
                else
                {
                    lip_log_message_n(p_dlb_lip, "Timeout, not handled LIP opcode %u \n", opcode);
                }
                lip_reply_for_pending_cmd_received(
                    p_dlb_lip, pending_msg_src_addr, handled ? LIP_MESSAGE_HANDLED : LIP_MESSAGE_ABORT_RECEIVED);
            }
        }
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }
    else
    {
        ret = 1;
    }

    if (transmit)
    {
        if (lip_transmit_wrapper(p_dlb_lip, &response))
        {
            lip_log_message(p_dlb_lip, "Message transmit failed\n");
        }
    }
    return ret;
}

size_t dlb_lip_query_memory()
{
    size_t mem_size;

    /* dlb_lip context */
    mem_size = DLB_LOOSE_SIZE(sizeof(void *), sizeof(dlb_lip_t));

    return mem_size;
}

static void lip_init_defaults(dlb_lip_t *const p_dlb_lip)
{
    p_dlb_lip->state                              = LIP_INIT;
    p_dlb_lip->is_running                         = true;
    p_dlb_lip->thread_signaled                    = false;
    p_dlb_lip->downstream_device_cfg.logical_addr = DLB_LOGICAL_ADDR_UNKNOWN;
    p_dlb_lip->downstream_device_cfg.uuid         = LIP_INVALID_UUID;
    p_dlb_lip->callback_id                        = LIP_INVALID_CALLBACK_ID;
    p_dlb_lip->add_iec_decoding_latency           = false;
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
    {
        p_dlb_lip->upstream_devices_addresses[i] = DLB_LOGICAL_ADDR_UNKNOWN;
    }
    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->opcode_of_last_cmd_sent); i += 1)
    {
        p_dlb_lip->opcode_of_last_cmd_sent[i] = LIP_OPCODES;
    }
}

dlb_lip_t *dlb_lip_open(
    uint8_t *const                       p_mem,
    const dlb_lip_config_params_t *const init_params,
    const dlb_lip_callbacks_t            callbacks,
    const dlb_cec_bus_t *const           cec_bus)
{
    const dlb_cec_message_t empty_msg    = { 0 };
    dlb_lip_t *             p_dlb_lip    = NULL;
    uint8_t *               p_static_mem = NULL;
    bool                    error        = false;

    do
    {
        if (!p_mem)
        {
            lip_log_message(p_dlb_lip, "p_mem cannot be NULL\n");
            break;
        }
        if (!init_params)
        {
            lip_log_message(p_dlb_lip, "init_params cannot be NULL\n");
            break;
        }
        if (!cec_bus)
        {
            lip_log_message(p_dlb_lip, "cec_bus cannot be NULL\n");
            break;
        }
        if (!cec_bus->register_callback)
        {
            lip_log_message(p_dlb_lip, "cec_bus.register_callback cannot be NULL\n");
            break;
        }
        if (!cec_bus->transmit_callback)
        {
            lip_log_message(p_dlb_lip, "cec_bus.transmit_callback cannot be NULL\n");
            break;
        }
        if (cec_bus->logical_address == DLB_LOGICAL_ADDR_BROADCAST || cec_bus->logical_address == DLB_LOGICAL_ADDR_UNKNOWN)
        {
            lip_log_message(p_dlb_lip, "Invalid CEC logical_address\n");
            break;
        }
        if (init_params->audio_transcoding && !lip_is_audio_format_valid(init_params->audio_transcoding_format))
        {
            lip_log_message(p_dlb_lip, "Audio transcoding is enabled but selected format is invalid\n");
            break;
        }
        if (!callbacks.merge_uuid_callback)
        {
            lip_log_message(p_dlb_lip, "merge_uuid_callback cannot be NULL\n");
            break;
        }
        p_static_mem = p_mem;

        /* Initialize codec detection context */
        memset(p_static_mem, 0, DLB_LOOSE_SIZE(sizeof(void *), sizeof(dlb_lip_t)));
        p_dlb_lip = (dlb_lip_t *const)lip_aligned_ptr(sizeof(void *), p_static_mem);

        dlb_lip_osa_init_critial_section(&p_dlb_lip->critical_section);
        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

        p_dlb_lip->callbacks     = callbacks;
        p_dlb_lip->config_params = *init_params;
        lip_init_defaults(p_dlb_lip);

        dlb_cache_init(&p_dlb_lip->downstream_device_cfg.latency_cache, true);
        dlb_lip_osa_init_cond_var(&p_dlb_lip->condition_var);
        dlb_lip_osa_init_cond_var(&p_dlb_lip->state_updated_condition_var);
        dlb_lip_osa_init_cond_var(&p_dlb_lip->pending_requests.cv);
        for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->pending_requests.messages); i += 1)
        {
            p_dlb_lip->pending_requests.messages[i].msg            = empty_msg;
            p_dlb_lip->pending_requests.messages[i].state          = LIP_MESSAGE_HANDLED;
            p_dlb_lip->pending_requests.messages[i].expire_time_ms = 0;
        }

        p_dlb_lip->cec_bus = *cec_bus;
        p_dlb_lip->cec_bus.register_callback(p_dlb_lip->cec_bus.handle, dlb_lip_cec_cmd_received, p_dlb_lip);

        if (dlb_lip_osa_init_timer(&p_dlb_lip->timer, dlb_lip_timer_callback, p_dlb_lip))
        {
            lip_log_message(p_dlb_lip, "unable to initilize timer!\n");
            error = true;
            break;
        }

        if (error == false && dlb_lip_osa_start_thread(&p_dlb_lip->request_thread, dlb_lip_request_thread, (void *)p_dlb_lip))
        {
            lip_log_message(p_dlb_lip, "unable to start thread!\n");
            error = true;
            break;
        }
        p_dlb_lip->start_time = dlb_lip_osa_get_time_ms();
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);

    } while (false);

    if (error)
    {
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
        dlb_lip_osa_delete_critial_section(&p_dlb_lip->critical_section);
        dlb_lip_osa_delete_cond_var(&p_dlb_lip->condition_var);
        dlb_lip_osa_delete_cond_var(&p_dlb_lip->state_updated_condition_var);
        p_dlb_lip = NULL;
    }

    return p_dlb_lip;
}

void dlb_lip_close(dlb_lip_t *const p_dlb_lip)
{
    // Delete timer
    dlb_lip_osa_delete_timer(&p_dlb_lip->timer);

    // Request thread to finish
    dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);
    p_dlb_lip->is_running      = false;
    p_dlb_lip->thread_signaled = true;
    dlb_lip_osa_signal_condition(&p_dlb_lip->condition_var);
    dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);

    // Wait for thread to finish
    dlb_lip_osa_join_thread(&p_dlb_lip->request_thread);

    // Store cache
    if (p_dlb_lip->downstream_device_cfg.logical_addr != DLB_LOGICAL_ADDR_UNKNOWN && p_dlb_lip->callbacks.store_cache_callback)
    {
        p_dlb_lip->callbacks.store_cache_callback(
            p_dlb_lip->callbacks.arg,
            p_dlb_lip->downstream_device_cfg.uuid,
            &p_dlb_lip->downstream_device_cfg.latency_cache,
            sizeof(p_dlb_lip->downstream_device_cfg.latency_cache));
    }

    // Destroy the mutex object.
    dlb_lip_osa_delete_critial_section(&p_dlb_lip->critical_section);
    // Destroy the conditional variable.
    dlb_lip_osa_delete_cond_var(&p_dlb_lip->condition_var);
    dlb_lip_osa_delete_cond_var(&p_dlb_lip->state_updated_condition_var);
}

dlb_lip_status_t dlb_lip_get_status(dlb_lip_t *const p_dlb_lip, const bool wait_for_discovery)
{
    dlb_lip_status_t status       = { 0 };
    status.downstream_device_addr = DLB_LOGICAL_ADDR_UNKNOWN;
    for (unsigned int i = 0; i < NUM_ELEMS(status.upstream_devices_addresses); i += 1)
    {
        status.upstream_devices_addresses[i] = DLB_LOGICAL_ADDR_UNKNOWN;
    }

    if (p_dlb_lip)
    {
        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);

        status = lip_get_status(p_dlb_lip, wait_for_discovery);

        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }

    return status;
}

int dlb_lip_set_config(
    dlb_lip_t *const                     p_dlb_lip,
    const dlb_lip_config_params_t *const init_params,
    const bool                           force_discovery,
    const dlb_cec_logical_address_t      remove_upstream_device)
{
    int ret = 1;

    assert(p_dlb_lip);

    if (p_dlb_lip)
    {
        dlb_lip_osa_enter_critial_section(&p_dlb_lip->critical_section);
        if (init_params || force_discovery || (remove_upstream_device != DLB_LOGICAL_ADDR_UNKNOWN))
        {
            bool discovery        = force_discovery;
            bool update_uuid      = false;
            bool parameters_valid = true;

            dlb_cec_logical_address_t upstream_devices[MAX_UPSTREAM_DEVICES_COUNT] = { 0 };

            lip_remove_upstream_device(p_dlb_lip, remove_upstream_device);
            memcpy(upstream_devices, p_dlb_lip->upstream_devices_addresses, sizeof(p_dlb_lip->upstream_devices_addresses));

            if (init_params)
            {
                bool latency_change = false;
                if (p_dlb_lip->config_params.uuid != init_params->uuid)
                {
                    update_uuid = true;
                }
                if (memcmp(
                        init_params->audio_latencies,
                        p_dlb_lip->config_params.audio_latencies,
                        sizeof(init_params->audio_latencies)))
                {
                    latency_change = true;
                }
                if (memcmp(
                        init_params->video_latencies,
                        p_dlb_lip->config_params.video_latencies,
                        sizeof(init_params->video_latencies)))
                {
                    latency_change = true;
                }
                if (p_dlb_lip->config_params.downstream_device_addr != init_params->downstream_device_addr)
                {
                    discovery = true;
                }
                if (p_dlb_lip->config_params.render_mode != init_params->render_mode)
                {
                    if (!update_uuid)
                    {
                        lip_log_message(p_dlb_lip, "ERROR: Render mode change without UUID change!\n");
                        parameters_valid = false;
                    }
                }
                if (p_dlb_lip->config_params.audio_transcoding != init_params->audio_transcoding)
                {
                    if (!update_uuid)
                    {
                        lip_log_message(p_dlb_lip, "ERROR: Audio transcoding  change without UUID change!\n");
                        parameters_valid = false;
                    }
                }
                else if (p_dlb_lip->config_params.audio_transcoding)
                {
                    if (memcmp(
                            &p_dlb_lip->config_params.audio_transcoding_format,
                            &init_params->audio_transcoding_format,
                            sizeof(p_dlb_lip->config_params.audio_transcoding_format)))
                    {
                        if (!update_uuid)
                        {
                            lip_log_message(p_dlb_lip, "ERROR: Audio transcoding format change without UUID change!\n");
                            parameters_valid = false;
                        }
                    }
                }
                if (latency_change && !update_uuid)
                {
                    lip_log_message(p_dlb_lip, "ERROR: Latency change without UUID change!\n");
                    parameters_valid = false;
                }
                else
                {
                    p_dlb_lip->config_params = *init_params;
                }
            }
            if (parameters_valid)
            {
                ret = 0;

                memcpy(p_dlb_lip->upstream_devices_addresses, upstream_devices, sizeof(p_dlb_lip->upstream_devices_addresses));

                if (discovery)
                {
                    // Start new discovery process, if we set new UUID it will be sent upstream during discovery process
                    lip_init_defaults(p_dlb_lip);
                    // Restore upstream device cleared by dlb_lip_init_defaults
                    memcpy(p_dlb_lip->upstream_devices_addresses, upstream_devices, sizeof(p_dlb_lip->upstream_devices_addresses));
                    // Clear cache
                    dlb_cache_clear(&p_dlb_lip->downstream_device_cfg.latency_cache, true, true);
                    // Wake up Thread
                    p_dlb_lip->thread_signaled = true;
                    dlb_lip_osa_signal_condition(&p_dlb_lip->condition_var);
                }
                if (lip_is_upstream_device_present(p_dlb_lip) && update_uuid)
                {
                    for (unsigned int i = 0; i < NUM_ELEMS(p_dlb_lip->upstream_devices_addresses); i += 1)
                    {
                        // Send LIP_OPCODE_UPDATE_UUID
                        if (p_dlb_lip->upstream_devices_addresses[i] != DLB_LOGICAL_ADDR_UNKNOWN)
                        {
                            ret = lip_transmit_report_lip_support(
                                p_dlb_lip, p_dlb_lip->upstream_devices_addresses[i], lip_get_uuid(p_dlb_lip), true);
                        }
                    }
                }
            }
        }
        dlb_lip_osa_leave_critial_section(&p_dlb_lip->critical_section);
    }

    return ret;
}
