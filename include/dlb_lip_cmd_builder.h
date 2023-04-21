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

#pragma once

#include "dlb_lip_cec_bus.h"
#include "dlb_lip_types.h"

void lip_build_abort_cec_command(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_cec_opcode_t          opcode,
    const dlb_cec_abort_reason_t    reason);

void dlb_lip_build_request_lip_support(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination);

void dlb_lip_build_request_av_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_video_format_t    video_format,
    const dlb_lip_audio_format_t    audio_format);

void dlb_lip_build_request_audio_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_audio_format_t    audio_format);

void dlb_lip_build_request_video_latency(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const dlb_lip_video_format_t    video_format);

void dlb_lip_build_report_lip_support_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   version,
    const uint32_t                  uuid,
    const bool                      update_uuid);

void dlb_lip_build_report_av_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   video_latency,
    const uint8_t                   audio_latency);

void dlb_lip_build_report_audio_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   audio_latency);

void dlb_lip_build_report_video_latency_cmd(
    dlb_cec_message_t *             command,
    const dlb_cec_logical_address_t initiator,
    const dlb_cec_logical_address_t destination,
    const uint8_t                   video_latency);
