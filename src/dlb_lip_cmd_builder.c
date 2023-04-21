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
 *  @file       dlb_lip_cmd_builder
 *  @brief      LIP command builder
 *
 */

#include "dlb_lip_cmd_builder.h"

#include <assert.h>

static void dlb_lip_set_dolby_vendor_id(dlb_cec_message_t *command)
{
    assert(command);
    command->data[command->msg_length++] = DOLBY_VENDOR_ID[0];
    command->data[command->msg_length++] = DOLBY_VENDOR_ID[1];
    command->data[command->msg_length++] = DOLBY_VENDOR_ID[2];
}

static void lip_init_cec_command(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination)
{
    assert(command);

    command->initiator   = initiator;
    command->destination = destination;
    command->opcode      = DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID;
    command->msg_length  = 0;

    dlb_lip_set_dolby_vendor_id(command);
}

void lip_build_abort_cec_command(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_cec_opcode_t          opcode,
    const dlb_cec_abort_reason_t    reason)
{
    command->initiator                   = initiator;
    command->destination                 = destination;
    command->opcode                      = DLB_CEC_OPCODE_FEATURE_ABORT;
    command->msg_length                  = 0;
    command->data[command->msg_length++] = opcode;
    command->data[command->msg_length++] = reason;
}

void dlb_lip_build_request_lip_support(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination)
{
    lip_init_cec_command(command, initiator, destination);

    // CEC_LIP_OPCODE_REQUEST_LIP_SUPPORT
    command->data[command->msg_length++] = LIP_OPCODE_REQUEST_LIP_SUPPORT;
}

void dlb_lip_build_request_av_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_video_format_t    video_format,
    const dlb_lip_audio_format_t    audio_format)
{
    lip_init_cec_command(command, initiator, destination);

    // LIP_OPCODE_REQUEST_AV_LATENCY
    command->data[command->msg_length++] = LIP_OPCODE_REQUEST_AV_LATENCY;

    // Video format
    command->data[command->msg_length++] = video_format.vic;

    // hdr_mode
    command->data[command->msg_length++] = get_value_from_hdr_mode(video_format);

    // Audio codec
    command->data[command->msg_length++] = audio_format.codec;

    // Audio extension is optional
    if (audio_format.ext != 0 || audio_format.subtype != IEC61937_SUBTYPE_0)
    {
        command->data[command->msg_length++] = (uint8_t)(audio_format.ext << 2 | audio_format.subtype);
    }
}

void dlb_lip_build_request_audio_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_audio_format_t    audio_format)
{
    lip_init_cec_command(command, initiator, destination);

    // LIP_OPCODE_REQUEST_AUDIO_LATENCY
    command->data[command->msg_length++] = LIP_OPCODE_REQUEST_AUDIO_LATENCY;

    // Audio codec
    command->data[command->msg_length++] = audio_format.codec;

    // Audio extension is optional
    if (audio_format.ext != 0 || audio_format.subtype != IEC61937_SUBTYPE_0)
    {
        command->data[command->msg_length++] = (uint8_t)(audio_format.ext << 2 | audio_format.subtype);
    }
}

void dlb_lip_build_request_video_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_video_format_t    video_format)
{
    lip_init_cec_command(command, initiator, destination);

    // CEC_LIP_OPCODE_REQUEST_LIP_SUPPORT
    command->data[command->msg_length++] = LIP_OPCODE_REQUEST_VIDEO_LATENCY;

    // Video format
    command->data[command->msg_length++] = video_format.vic;

    // hdr_mode
    command->data[command->msg_length++] = get_value_from_hdr_mode(video_format);
}

void dlb_lip_build_report_lip_support_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   version,
    const uint32_t                  uuid,
    const bool                      update_uuid)
{
    lip_init_cec_command(command, initiator, destination);

    // CEC_LIP_OPCODE_REPORT_LIP_SUPPORT
    command->data[command->msg_length++] = update_uuid ? LIP_OPCODE_UPDATE_UUID : LIP_OPCODE_REPORT_LIP_SUPPORT;

    // Protocol version
    command->data[command->msg_length++] = version;

    // UUID
    command->data[command->msg_length++] = (uuid >> 24) & 0xFF;
    command->data[command->msg_length++] = (uuid >> 16) & 0xFF;
    command->data[command->msg_length++] = (uuid >> 8) & 0xFF;
    command->data[command->msg_length++] = uuid & 0xFF;
}

void dlb_lip_build_report_av_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   video_latency,
    const uint8_t                   audio_latency)
{
    lip_init_cec_command(command, initiator, destination);

    // LIP_OPCODE_REPORT_AV_LATENCY
    command->data[command->msg_length++] = LIP_OPCODE_REPORT_AV_LATENCY;
    // Current Video Latency
    command->data[command->msg_length++] = video_latency;
    // Current Audio Latency
    command->data[command->msg_length++] = audio_latency;
}

void dlb_lip_build_report_audio_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   audio_latency)
{
    lip_init_cec_command(command, initiator, destination);

    // LIP_OPCODE_REPORT_AUDIO_LATENCY
    command->data[command->msg_length++] = LIP_OPCODE_REPORT_AUDIO_LATENCY;
    // Current Audio Latency
    command->data[command->msg_length++] = audio_latency;
}

void dlb_lip_build_report_video_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   video_latency)
{
    lip_init_cec_command(command, initiator, destination);

    // LIP_OPCODE_REPORT_VIDEO_LATENCY
    command->data[command->msg_length++] = LIP_OPCODE_REPORT_VIDEO_LATENCY;
    // Current Video Latency
    command->data[command->msg_length++] = video_latency;
}
