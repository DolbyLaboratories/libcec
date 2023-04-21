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
 *  @file       dlb_lip_osa.h
 *  @brief      OS abstraction
 *
 */
#pragma once
#include <inttypes.h>
#include <stdbool.h>

#if defined(_MSC_VER)
#include <process.h> /* _beginthread */
#include <windows.h>
typedef HANDLE             dlb_thread_t;
typedef CRITICAL_SECTION   dlb_mutex_t;
typedef CONDITION_VARIABLE dlb_cond_t;
#else
#include <errno.h>
#include <pthread.h>
#define INFINITE (0xFFFFFFFFu)
typedef pthread_t       dlb_thread_t;
typedef pthread_mutex_t dlb_mutex_t;
typedef pthread_cond_t  dlb_cond_t;
#endif

static const uint32_t LIP_INVALID_CALLBACK_ID  = 0xFFFFFFFF;
static const uint32_t LIP_OSA_INFINITE_TIMEOUT = INFINITE;

/**
 * @brief Timer callback function
 */
typedef int (*timer_callback)(void *, uint32_t callback_id);

typedef struct dlb_lip_osa_timer
{
    // Thread handle
    dlb_thread_t   thread;
    dlb_mutex_t    critical_section;
    dlb_cond_t     condition_var;
    bool           is_running;
    timer_callback callback;
    void *         callback_arg;
    uint32_t       timeout_ms;
    uint32_t       callback_id;
} dlb_lip_osa_timer_t;

/**
 * @brief Initialize a critical section.
 */
void dlb_lip_osa_init_critial_section(dlb_mutex_t *p_dlb_mtx);

/**
 * @brief Deletes a critical section.
 */
void dlb_lip_osa_delete_critial_section(dlb_mutex_t *p_dlb_mtx);

/**
 * @brief Initialize a condition variable.
 */
void dlb_lip_osa_init_cond_var(dlb_cond_t *p_dlb_cond);

/**
 * @brief Deletes a condition variable.
 */
void dlb_lip_osa_delete_cond_var(dlb_cond_t *p_dlb_cond);

/**
 * @brief Enter a critical section.
 */
void dlb_lip_osa_enter_critial_section(dlb_mutex_t *p_dlb_mtx);

/**
 * @brief Trye enter a critical section.
 */
bool dlb_lip_osa_try_enter_critial_section(dlb_mutex_t *p_dlb_mtx);

/**
 * @brief Leave a critical section.
 */
void dlb_lip_osa_leave_critial_section(dlb_mutex_t *p_dlb_mtx);

/**
 * @brief Signals condition and wakeup single thread blocked on a condition variable.
 */
void dlb_lip_osa_signal_condition(dlb_cond_t *p_dlb_cond);

/**
 * @brief Signals condition and wakeup all threads blocked on a condition variable.
 */
void dlb_lip_osa_broadcast_condition(dlb_cond_t *p_dlb_cond);

/**
 * @brief Waits for condition to be signaled
 * @return 0 if condition was signaled , 1 in case of timeout
 */
int dlb_lip_osa_wait_condition(
    dlb_cond_t *        p_dlb_cond,
    dlb_mutex_t *       p_dlb_mtx,
    uint32_t            timeout_ms,
    unsigned long long *elapse_time_ms);

/**
 * @brief Start thread
 * @return 0 on success
 */
int dlb_lip_osa_start_thread(
    dlb_thread_t *p_dlb_thread,
#if defined(_MSC_VER)
    void (*func)(void *)
#else
    void *(*func)(void *)
#endif
        ,
    void *arg);

/**
 * @brief Waits for the thread to terminate
 */
void dlb_lip_osa_join_thread(const dlb_thread_t *p_dlb_thread);

/**
 * @brief Creates timer
 * @return 0 on success
 */
int dlb_lip_osa_init_timer(dlb_lip_osa_timer_t *timer, timer_callback callback, void *callback_arg);

/**
 * @brief Starts timer with the specified timeout, cancels any previous timer
   @return callback_id
 */
uint32_t dlb_lip_osa_set_timer(dlb_lip_osa_timer_t *timer, uint32_t timeout_ms);

/**
 * @brief Cancel timer
 */
void dlb_lip_osa_cancel_timer(dlb_lip_osa_timer_t *timer);

/**
 * @brief Delete timer
 */
void dlb_lip_osa_delete_timer(dlb_lip_osa_timer_t *timer);

/**
 * @brief Get time
 */
unsigned long long dlb_lip_osa_get_time_ms(void);
