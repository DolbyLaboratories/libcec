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
 *  @file       dlb_lip.h
 *  @brief
 *
 *  Example API usage:
 *      Open/Close LIP connection:
 *          p_dlb_lip = dlb_lip_open();
 *          ...
 *          if (p_dlb_lip)
 *          {
 *              dlb_lip_close(p_dlb_lip);
 *          }
 *          // This is the simplest fully working example for sink devices. LIP library will automatically answer to all LIP
 * requests.
 *
 *
 *      Query downstream latency:
 *          p_dlb_lip = dlb_lip_open();
 *          if (p_dlb_lip)
 *          {
 *              ...
 *              s = dlb_lip_get_status(p_dlb_lip, true);
 *              if (s.status & LIP_DOWNSTREAM_CONNECTED)
 *              {
 *                  if (dlb_lip_get_av_latency(p_dlb_lip, video_format, audio_format, v_latency, a_latency) == 0)
 *                  {
 *                      //adjust latency according to v_latency and a_latency
 *                  }
 *              }
 *              ...
 *              dlb_lip_close(p_dlb_lip);
 *          }
 *
 *
 *      Update parameters at runtime:
 *          p_dlb_lip = dlb_lip_open();
 *          if (p_dlb_lip)
 *          {
 *              ...
 *              dlb_lip_set_config(p_dlb_lip, new_params, false, DLB_LOGICAL_ADDR_UNKNOWN);
 *               ...
 *              dlb_lip_close(p_dlb_lip);
 *          }
 *
 *
 *      HDMI hotplug(new downstream device in network):
 *          p_dlb_lip = dlb_lip_open();
 *          if (p_dlb_lip)
 *          {
 *              ...
 *              // HDMI hotplug event
 *              // new_params - with updated logical_address_map
 *              dlb_lip_set_config(p_dlb_lip, new_params, true, DLB_LOGICAL_ADDR_UNKNOWN);
 *              ...
 *              dlb_lip_close(p_dlb_lip);
 *          }
 *
 *
 *      HDMI hotplug(SRC upstream device disappeared from the network):
 *          p_dlb_lip = dlb_lip_open();
 *          if (p_dlb_lip)
 *          {
 *              ...
 *              // HDMI hotplug event
 *              dlb_lip_set_config(p_dlb_lip, NULL, false, DLB_LOGICAL_ADDR_PLAYBACK_DEVICE1);
 *              ...
 *              dlb_lip_close(p_dlb_lip);
 *          }
 *
 *
 *      Rendering mode change:
 *          p_dlb_lip = dlb_lip_open();
 *          if (p_dlb_lip)
 *          {
 *              ...
 *              // Rendering mode change
 *              // new_params - with updated uuid and new latency values
 *              dlb_lip_set_config(p_dlb_lip, NULL, false, DLB_LOGICAL_ADDR_UNKNOWN);
 *              ...
 *              dlb_lip_close(p_dlb_lip);
 *          }
 */

#pragma once

#ifndef DLB_LIP_H
#define DLB_LIP_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h> /* uint16_t */
#include <stdlib.h> /* size_t */

#include <dlb_lip_cec_bus.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @{ */
/**
 * @name Library version info
 */
#define DLB_LIP_LIB_V_API (1)  /**< @brief API version. */
#define DLB_LIP_LIB_V_FCT (0)  /**< @brief Functional change. */
#define DLB_LIP_LIB_V_MTNC (0) /**< @brief Maintenance release. */
/* @} */

#define MAX_VICS (219U)
#define MAX_AUDIO_FORMAT_EXTENSIONS (32U)
#define MAX_UPSTREAM_DEVICES_COUNT (16U)
static const uint8_t LIP_INVALID_LATENCY  = 255;
static const uint8_t LIP_PROTOCOL_VERSION = 0x00;

typedef struct dlb_lip_s dlb_lip_t;

typedef enum dlb_lip_color_format_type_e
{
    LIP_COLOR_FORMAT_HDR_STATIC,   // (CTA-861-G, Sec. 7.5.13)
    LIP_COLOR_FORMAT_HDR_DYNAMIC,  // (CTA-861-G Table 47)
    LIP_COLOR_FORMAT_DOLBY_VISION, // Dolby Vision Vendor-Specific Video Data Block (VSVDB)

    LIP_COLOR_FORMAT_COUNT
} dlb_lip_color_format_type_t;

/* Data type as defined in CTA-861-G, Sec. 7.5.13 Table 85) */
typedef enum dlb_lip_hdr_static_e
{
    LIP_HDR_STATIC_SDR,
    LIP_HDR_STATIC_HDR,
    LIP_HDR_STATIC_SMPTE_ST_2084,
    LIP_HDR_STATIC_HLG,

    LIP_HDR_STATIC_COUNT
} dlb_lip_hdr_static_t;

/* Data type as defined in CTA-861-G Table 47 */
typedef enum dlb_lip_hdr_dynamic_e
{
    LIP_HDR_DYNAMIC_SMPTE_ST_2094_10,
    LIP_HDR_DYNAMIC_ETSI_TS_103_433,
    LIP_HDR_DYNAMIC_ITU_T_H265,
    LIP_HDR_DYNAMIC_SMPTE_ST_2094_40,

    LIP_HDR_DYNAMIC_COUNT
} dlb_lip_hdr_dynamic_t;

/* Data types as defined in Dolby Vision Vendor-Specific Video Data Block (VSVDB) */
typedef enum dlb_lip_dolby_vision_e
{
    LIP_HDR_DOLBY_VISION_SINK_LED,
    LIP_HDR_DOLBY_VISION_SOURCE_LED,

    LIP_HDR_DOLBY_VISION_COUNT
} dlb_lip_dolby_vision_t;

#define MAX_OF_3(v0, v1, v2) \
    (((int)v0 > (int)v1) ? (((int)v0 > (int)v2) ? (int)v0 : (int)v2) : (((int)v1 > (int)v2) ? (int)v1 : (int)v2))
#define HDR_MODES_COUNT MAX_OF_3(LIP_HDR_STATIC_COUNT, LIP_HDR_DYNAMIC_COUNT, LIP_HDR_DOLBY_VISION_COUNT)

/* Data types as defined in IEC 61937-2, Table 2, values bits 0-4 of Pc (except for value 0). */
typedef enum dlb_lip_audio_codec_e
{
    PCM                    = 0,
    IEC61937_AC3           = 1,
    IEC61937_SMPTE_338M    = 2,
    IEC61937_PAUSE_BURST   = 3,
    IEC61937_MPEG1_L1      = 4,
    IEC61937_MEPG1_L2_L3   = 5,
    IEC61937_MPEG2         = 6,
    IEC61937_MPEG2_AAC     = 7,
    IEC61937_MPEG2_L1      = 8,
    IEC61937_MPEG2_L2      = 9,
    IEC61937_MPEG2_L3      = 10,
    IEC61937_DTS_TYPE_I    = 11,
    IEC61937_DTS_TYPE_II   = 12,
    IEC61937_DTS_TYPE_III  = 13,
    IEC61937_ATRAC         = 14,
    IEC61937_ATRAC_2_3     = 15,
    IEC61937_ATRAC_X       = 16,
    IEC61937_DTS_TYPE_IV   = 17,
    IEC61937_WMA_PRO       = 18,
    IEC61937_MPEG2_AAC_LSF = 19,
    IEC61937_MPEG4_AAC     = 20,
    IEC61937_EAC3          = 21,
    IEC61937_MAT           = 22,
    IEC61937_MPEG4         = 23,

    IEC61937_AUDIO_CODECS = 32
} dlb_lip_audio_codec_t;

/* Data types as defined in IEC 61937-2, Table 2, values bits 5-6 of Pc. */
typedef enum dlb_lip_audio_formats_subtypes_e
{
    IEC61937_SUBTYPE_0,
    IEC61937_SUBTYPE_1,
    IEC61937_SUBTYPE_2,
    IEC61937_SUBTYPE_3,

    IEC61937_SUBTYPES = 4
} dlb_lip_audio_formats_subtypes_t;

/* Subtype and ext matches to [Audio Format Extension] from LIP specification, both are optional, set them to IEC61937_SUBTYPE_0, 0,
 * when not used */
typedef struct dlb_lip_audio_format_s
{
    dlb_lip_audio_codec_t            codec;
    dlb_lip_audio_formats_subtypes_t subtype;
    /* For compressed audio, as per bits[7..12] of audio codec - specific PC field as defined in IEC 61937 - 2. */
    uint8_t ext;
} dlb_lip_audio_format_t;

typedef struct dlb_lip_video_format_s
{
    uint8_t                     vic;
    dlb_lip_color_format_type_t color_format;
    union
    {
        dlb_lip_hdr_static_t   hdr_static;
        dlb_lip_hdr_dynamic_t  hdr_dynamic;
        dlb_lip_dolby_vision_t dolby_vision;
    } hdr_mode;
} dlb_lip_video_format_t;

typedef enum dlb_lip_renderer_e
{
    /* Video renderer reports own video latency to upstream device */
    LIP_VIDEO_RENDERER = 1u << 1,
    /* Audio renderer reports own audio latency to upstream device */
    LIP_AUDIO_RENDERER = 1u << 2,
} dlb_lip_renderer_t;

typedef struct dlb_lip_config_params
{
    /* Logical address of LIP downstream device - should be set to DLB_LOGICAL_ADDR_UNKNOWN if there is no downstream device*/
    dlb_cec_logical_address_t downstream_device_addr;
    /* Device universally unique identifier(top 16 bits) with rendering mode(bottom 16 bits) */
    uint32_t uuid;
    /* any of dlb_lip_renderer_t flags eg. AVR - LIP_AUDIO_RENDERER, TV - LIP_VIDEO_RENDERER| LIP_AUDIO_RENDERER, TV with AVR
     * connected - LIP_VIDEO_RENDERER */
    uint32_t render_mode;
    /* For source devices, when there never will be an upstream device, video latency array should be filled with
     * LIP_INVALID_LATENCY */
    uint8_t video_latencies[MAX_VICS][LIP_COLOR_FORMAT_COUNT][HDR_MODES_COUNT];
    /* For source devices, when there never will be an upstream device, audio latency array should be filled with
     * LIP_INVALID_LATENCY */
    uint8_t audio_latencies[IEC61937_AUDIO_CODECS][IEC61937_SUBTYPES][MAX_AUDIO_FORMAT_EXTENSIONS];
    /* Set to true if device is doing audio transcoding */
    bool audio_transcoding;
    /* Valid only when audio_transcoding is true, defines audio output format */
    dlb_lip_audio_format_t audio_transcoding_format;
} dlb_lip_config_params_t;

typedef enum dlb_lip_connection_status
{
    LIP_DOWNSTREAM_CONNECTED = 1u << 0,
    LIP_UPSTREAM_CONNECTED   = 1u << 1,
} dlb_lip_connection_status_t;

typedef struct dlb_lip_status
{
    uint32_t                  status; // any of dlb_lip_connection_status_t flags
    dlb_cec_logical_address_t downstream_device_addr;
    uint32_t                  downstream_device_uuid;
    dlb_cec_logical_address_t upstream_devices_addresses[MAX_UPSTREAM_DEVICES_COUNT];
} dlb_lip_status_t;

/**
 * @brief Message log function equivalent to vprintf with additional callback parameter
 * @param arg           Callback parameter provided when the callbacks were set up
 * @param format        Format string
 * @va_list             Argument list
 */
typedef int (*printf_callback_t)(void *arg, const char *format, va_list);
/**
 * @brief Store cache callback, called to store cache data in persistent memory
 * @param arg           Callback parameter provided when the callbacks were set up
 * @param uuid          Device identifier whose data we want to store
 * @param cache_data    Pointer to cache data
 * @paramm size         Number of bytes to store
 */
typedef void (*store_cache_callback_t)(void *arg, uint32_t uuid, const void *const cache_data, uint32_t size);
/**
 * @brief Read cache callback, called to read cache data from persistent memory
 * @param arg           Callback parameter provided when the callbacks were set up
 * @param uuid          Device identifier whose data we want to read
 * @param cache_data    Pointer where read data should be stored
 * @paramm size         Number of bytes to read
 * @return Actual number of bytes read
 */
typedef uint32_t (*read_cache_callback_t)(void *arg, uint32_t uuid, void *const cache_data, uint32_t size);
/**
 * @brief New connection status of LIP
 * @param arg           Callback parameter provided when the callbacks were set up
 * @param status        Current connection status
 */
typedef void (*status_change_callback_t)(void *arg, dlb_lip_status_t status);
/**
 * @brief Merge uuid - caller should merge downstream uuid with own uuid and return new combined uuid. Merged uuid will be send
 * to upstream device.
 * @param arg           Callback parameter provided when the callbacks were set up
 * @own_uuid            Our current uuid as set in dlb_lip_open or dlb_lip_set_config
 * @downstream_uuid     Downstream device uuid
 */
typedef uint32_t (*merge_uuid_callback_t)(void *arg, uint32_t own_uuid, uint32_t downstream_uuid);

typedef struct dlb_lip_callbacks
{
    void *                   arg;
    printf_callback_t        printf_callback;
    store_cache_callback_t   store_cache_callback;
    read_cache_callback_t    read_cache_callback;
    status_change_callback_t status_change_callback;
    merge_uuid_callback_t    merge_uuid_callback;
} dlb_lip_callbacks_t;

/**
 * @brief Return the memory needed by the dlb_lip.
 * @return Size in bytes of a memory block.
 */
size_t dlb_lip_query_memory(void);

/**
 * @brief Open and initialize LIP
 * @param init_params       Configuration parameters, cannot be NULL
 * @param callbacks         Callback functions
 * @param cec_bus           CEC bus interface, cannot be NULL
 * @return LIP handle or NULL on error
 */
dlb_lip_t *dlb_lip_open(
    uint8_t *const                       p_mem,
    const dlb_lip_config_params_t *const init_params,
    const dlb_lip_callbacks_t            callbacks,
    const dlb_cec_bus_t *const           cec_bus);

/**
 * @brief Close LIP library
 * @param p_dlb_lip LIP handle
 */
void dlb_lip_close(dlb_lip_t *const p_dlb_lip);

/**
 * @brief Return a/v latency of downstream device.
 * @param p_dlb_lip         LIP handle
 * @param video_format      Video format
 * @param audio_format      Audio format
 * @param video_latency     Pointer where video latency should be stored, cannot be NULL
 * @param audio_latency     Pointer where audio latency should be stored, cannot be NULL
 * @return 0 on success, otherwise returned latency values are invalid
 */
int dlb_lip_get_av_latency(
    dlb_lip_t *const             p_dlb_lip,
    const dlb_lip_video_format_t video_format,
    const dlb_lip_audio_format_t audio_format,
    uint8_t *const               video_latency,
    uint8_t *const               audio_latency);

/**
 * @brief Return cached video latency or query downstream device
 * @param p_dlb_lip         LIP handle
 * @param video_format      Video format
 * @param video_latency     Pointer where video latency should be stored, cannot be NULL
 * @return 0 on success, otherwise returned latency value is invalid
 */
int dlb_lip_get_video_latency(dlb_lip_t *const p_dlb_lip, const dlb_lip_video_format_t video_format, uint8_t *const video_latency);

/**
 * @brief Return cached audio latency or query downstream device
 * @param audio_latency     Pointer where audio latency should be stored, cannot be NULL
 * @param audio_format      Audio format
 * @return 0 on success, otherwise returned latency value is invalid
 */
int dlb_lip_get_audio_latency(dlb_lip_t *const p_dlb_lip, const dlb_lip_audio_format_t audio_format, uint8_t *const audio_latency);

/**
 * @brief Get LIP connection status
 * @param p_dlb_lip             LIP handle
 * @param wait_for_discovery    True to wait for discovery finish
 * @return dlb_lip_status_t
 */
dlb_lip_status_t dlb_lip_get_status(dlb_lip_t *const p_dlb_lip, const bool wait_for_discovery);

/**
 * @brief Set new configuration parameters
 * @param p_dlb_lip         LIP handle
 * @param init_params       Configuration parameters
 * @param force_discovery   True to rediscover new downstream device
 * @param remove_upstream_device
 *      1) logical address of upstream device to remove
 *      2) DLB_LOGICAL_ADDR_BROADCAST removes all upstream devices
 *      3) DLB_LOGICAL_ADDR_UNKNOWN don't remove any upstream devices
 * @return 0 on success
 */
int dlb_lip_set_config(
    dlb_lip_t *const                     p_dlb_lip,
    const dlb_lip_config_params_t *const init_params,
    const bool                           force_discovery,
    const dlb_cec_logical_address_t      remove_upstream_device);

#ifdef __cplusplus
}
#endif

#endif
