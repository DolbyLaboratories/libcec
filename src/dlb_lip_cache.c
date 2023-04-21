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

#include "dlb_lip_cache.h"
#include "dlb_lip_types.h"

void dlb_cache_init(dlb_latency_cache_t *p_dlb_cache, const bool enabled)
{
    p_dlb_cache->cache_enabled = enabled;
    dlb_cache_clear(p_dlb_cache, true, true);
}

void dlb_cache_clear(dlb_latency_cache_t *p_dlb_cache, const bool clear_audio, const bool clear_video)
{
    if (clear_video)
    {
        for (uint32_t vic = 0; vic < MAX_VICS; ++vic)
        {
            for (uint32_t color_format = 0; color_format < LIP_COLOR_FORMAT_COUNT; ++color_format)
            {
                for (uint32_t hdr_mode = 0; hdr_mode < HDR_MODES_COUNT; ++hdr_mode)
                {
                    p_dlb_cache->video_latencies[vic][color_format][hdr_mode]       = LIP_INVALID_LATENCY;
                    p_dlb_cache->video_latencies_valid[vic][color_format][hdr_mode] = false;
                }
            }
        }
    }

    if (clear_audio)
    {
        for (uint32_t audio_format = 0; audio_format < IEC61937_AUDIO_CODECS; ++audio_format)
        {
            for (uint32_t audio_format_subtype = 0; audio_format_subtype < IEC61937_SUBTYPES; ++audio_format_subtype)
            {
                for (uint32_t ext = 0; ext < MAX_AUDIO_FORMAT_EXTENSIONS; ++ext)
                {
                    p_dlb_cache->audio_latencies[audio_format][audio_format_subtype][ext]       = LIP_INVALID_LATENCY;
                    p_dlb_cache->audio_latencies_valid[audio_format][audio_format_subtype][ext] = false;
                }
            }
        }
    }
}

static bool
dlb_cache_validate_audio_params(dlb_lip_audio_codec_t audio_format, dlb_lip_audio_formats_subtypes_t subtype, uint8_t extension)
{
    return audio_format < IEC61937_AUDIO_CODECS && subtype < IEC61937_SUBTYPES && extension < MAX_AUDIO_FORMAT_EXTENSIONS;
}

bool dlb_cache_get_audio_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_audio_format_t audio_format, uint8_t *latency)
{
    bool ret = false;

    if (p_dlb_cache->cache_enabled)
    {
        if (dlb_cache_validate_audio_params(audio_format.codec, audio_format.subtype, audio_format.ext))
        {
            if (p_dlb_cache->audio_latencies_valid[audio_format.codec][audio_format.subtype][audio_format.ext])
            {
                *latency = p_dlb_cache->audio_latencies[audio_format.codec][audio_format.subtype][audio_format.ext];
                ret      = true;
            }
        }
    }
    return ret;
}

void dlb_cache_set_audio_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_audio_format_t audio_format, uint8_t latency)
{
    if (dlb_cache_validate_audio_params(audio_format.codec, audio_format.subtype, audio_format.ext))
    {
        p_dlb_cache->audio_latencies[audio_format.codec][audio_format.subtype][audio_format.ext]       = latency;
        p_dlb_cache->audio_latencies_valid[audio_format.codec][audio_format.subtype][audio_format.ext] = true;
    }
}

static bool dlb_cache_validate_video_params(dlb_lip_video_format_t video_format)

{
    return video_format.vic < MAX_VICS && video_format.color_format <= LIP_COLOR_FORMAT_COUNT
           && dlb_lip_get_hdr_mode_from_video_format(video_format) != HDR_MODES_COUNT;
}

bool dlb_cache_get_video_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_video_format_t video_format, uint8_t *latency)
{
    bool ret = false;
    if (p_dlb_cache->cache_enabled)
    {
        if (dlb_cache_validate_video_params(video_format))
        {
            if (p_dlb_cache->video_latencies_valid[video_format.vic][video_format.color_format]
                                                  [dlb_lip_get_hdr_mode_from_video_format(video_format)])
            {
                *latency = p_dlb_cache->video_latencies[video_format.vic][video_format.color_format]
                                                       [dlb_lip_get_hdr_mode_from_video_format(video_format)];
                ret = true;
            }
        }
    }
    return ret;
}

void dlb_cache_set_video_latency(dlb_latency_cache_t *p_dlb_cache, dlb_lip_video_format_t video_format, uint8_t latency)
{
    if (dlb_cache_validate_video_params(video_format))
    {
        p_dlb_cache
            ->video_latencies[video_format.vic][video_format.color_format][dlb_lip_get_hdr_mode_from_video_format(video_format)]
            = latency;
        p_dlb_cache->video_latencies_valid[video_format.vic][video_format.color_format]
                                          [dlb_lip_get_hdr_mode_from_video_format(video_format)]
            = true;
    }
}
