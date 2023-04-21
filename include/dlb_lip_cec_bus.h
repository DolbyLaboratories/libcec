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
 *  @file       dlb_lip_cec_bus.h
 *  @brief      LIP cec bus interface
 *
 */

#pragma once
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CEC_BUS_MAX_MSG_LENGTH 64

typedef enum dlb_cec_logical_address
{
    DLB_LOGICAL_ADDR_UNKNOWN           = -1,
    DLB_LOGICAL_ADDR_TV                = 0,
    DLB_LOGICAL_ADDR_RECORDING_DEVICE1 = 1,
    DLB_LOGICAL_ADDR_RECORDING_DEVICE2 = 2,
    DLB_LOGICAL_ADDR_TUNER1            = 3,
    DLB_LOGICAL_ADDR_PLAYBACK_DEVICE1  = 4,
    DLB_LOGICAL_ADDR_AUDIOSYSTEM       = 5,
    DLB_LOGICAL_ADDR_TUNER2            = 6,
    DLB_LOGICAL_ADDR_TUNER3            = 7,
    DLB_LOGICAL_ADDR_PLAYBACK_DEVICE2  = 8,
    DLB_LOGICAL_ADDR_RECORDING_DEVICE3 = 9,
    DLB_LOGICAL_ADDR_TUNER4            = 10,
    DLB_LOGICAL_ADDR_PLAYBACK_DEVICE3  = 11,
    DLB_LOGICAL_ADDR_RESERVED1         = 12,
    DLB_LOGICAL_ADDR_RESERVED2         = 13,
    DLB_LOGICAL_ADDR_FREEUSE           = 14,
    DLB_LOGICAL_ADDR_UNREGISTERED      = 15,
    DLB_LOGICAL_ADDR_BROADCAST         = 15
} dlb_cec_logical_address_t;

typedef enum dlb_cec_opcode
{
    DLB_CEC_OPCODE_FEATURE_ABORT          = 0x00,
    DLB_CEC_OPCODE_VENDOR_COMMAND_WITH_ID = 0xA0,
    DLB_CEC_OPCODE_NONE                   = 0xFD
} dlb_cec_opcode_t;

typedef enum dlb_cec_abort_reason
{
    DLB_CEC_ABORT_REASON_UNRECOGNIZED_OPCODE            = 0,
    DLB_CEC_ABORT_REASON_NOT_IN_CORRECT_MODE_TO_RESPOND = 1,
    DLB_CEC_ABORT_REASON_CANNOT_PROVIDE_SOURCE          = 2,
    DLB_CEC_ABORT_REASON_INVALID_OPERAND                = 3,
    DLB_CEC_ABORT_REASON_REFUSED                        = 4
} dlb_cec_abort_reason_t;

typedef struct dlb_cec_message
{
    dlb_cec_logical_address_t initiator;
    dlb_cec_logical_address_t destination;
    dlb_cec_opcode_t          opcode;
    uint8_t                   data[CEC_BUS_MAX_MSG_LENGTH];
    uint8_t                   msg_length;
} dlb_cec_message_t;

/* User defined CEC bus handle */
typedef struct dlb_cec_bus_handle_s dlb_cec_bus_handle_t;

/**
 * @brief CEC transmit function
 * @param bus_handle    Pointer to bus_handle
 * @param message       Message to transmit over CEC
 * @return int          0 if a message was transmitted successfully, 1 otherwise
 */
typedef int (*transmit_callback_t)(dlb_cec_bus_handle_t *bus_handle, const dlb_cec_message_t *const message);

/**
 * @brief CEC message callback function - should be called when CEC the message is received
 * @param arg           Callback parameter provided when the callback was set up
 * @param message       Pointer to received message
 * @return int          1 if messsage was consumed by LIP, 0 otherwise
 */
typedef int (*message_received_callback_t)(void *arg, const dlb_cec_message_t *message);

/**
 * @brief Register message callback function - registered function should be called every time device receive CEC message
 * @param bus_handle    Pointer to bus_handle
 * @param func          Pointer to message_received_callback_t
 * @param arg           Callback parameter that should be passed back to message_received_callback_t
 */
typedef void (*register_callback_t)(dlb_cec_bus_handle_t *const bus_handle, message_received_callback_t func, void *arg);

/**
 * @brief CEC bus interface
 */
typedef struct dlb_cec_bus
{
    dlb_cec_bus_handle_t *    handle;
    transmit_callback_t       transmit_callback;
    register_callback_t       register_callback;
    dlb_cec_logical_address_t logical_address;
} dlb_cec_bus_t;

#ifdef __cplusplus
}
#endif
