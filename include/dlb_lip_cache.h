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
 *  @file       dlb_lip_cache
 *  @brief      LIP latency cache
 *
 */

#pragma once

#include "dlb_lip.h"

#include <stdbool.h>

typedef struct dlb_latency_cache
{
    bool    cache_enabled;
    uint8_t video_latencies[MAX_VICS][LIP_COLOR_FORMAT_COUNT][HDR_MODES_COUNT];
    bool    video_latencies_valid[MAX_VICS][LIP_COLOR_FORMAT_COUNT][HDR_MODES_COUNT];
    uint8_t audio_latencies[IEC61937_AUDIO_CODECS][IEC61937_SUBTYPES][MAX_AUDIO_FORMAT_EXTENSIONS];
    bool    audio_latencies_valid[IEC61937_AUDIO_CODECS][IEC61937_SUBTYPES][MAX_AUDIO_FORMAT_EXTENSIONS];
} dlb_latency_cache_t;

/**
 * @brief Initialize cache store
 */
void dlb_cache_init(dlb_latency_cache_t *p_dlb_cache, const bool enabled);

/**
 * @brief Clear cache
 */
void dlb_cache_clear(dlb_latency_cache_t *p_dlb_cache, const bool clear_audio, const bool clear_video);

/**
 * @brief Return audio latency
 * @return return true if found in cache
 */
bool dlb_cache_get_audio_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_audio_format_t audio_format, uint8_t *latency);

/**
 * @brief Sets audio latency
 */
void dlb_cache_set_audio_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_audio_format_t audio_format, uint8_t latency);

/**
 * @brief Return video latency
 * @return return true if found in cache
 */
bool dlb_cache_get_video_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_video_format_t video_format, uint8_t *latency);

/**
 * @brief Sets video latency
 */
void dlb_cache_set_video_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_video_format_t video_format, uint8_t latency);
