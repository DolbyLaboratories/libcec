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

#include "dlb_lip_osa.h"
#include <time.h>

#ifdef __MACH__ /* MacOS time support */ 

#include <mach/clock.h>
#include <mach/mach.h>

static void macos_clock_gettime(struct timespec *p_ts)
{
  clock_serv_t clock_service;
  mach_timespec_t mts;
  
  /* Get monotonic time on MacOS */
  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_service);
  clock_get_time(clock_service, &mts);
  mach_port_deallocate(mach_task_self(), clock_service);
  
  p_ts->tv_sec = mts.tv_sec;
  p_ts->tv_nsec = mts.tv_nsec;      
}
#endif

unsigned long long dlb_lip_osa_get_time_ms(void)
{
#if defined(_MSC_VER)
    ULARGE_INTEGER ui;
    FILETIME       ftNow;
    GetSystemTimePreciseAsFileTime(&ftNow);
    ui.LowPart  = ftNow.dwLowDateTime;
    ui.HighPart = ftNow.dwHighDateTime;
    return ui.QuadPart / 10000llu;
#else
    struct timespec tp;
#ifdef __MACH__
    macos_clock_gettime(&tp);
#else    
    clock_gettime(CLOCK_MONOTONIC, &tp);
#endif
    return tp.tv_sec * 1000LLU + tp.tv_nsec / 1000000LLU;
#endif
}

void dlb_lip_osa_init_critial_section(dlb_mutex_t *p_dlb_mtx)
{
#if defined(_MSC_VER)
    InitializeCriticalSection(p_dlb_mtx);
#else
#ifdef USE_RECURISVE_LOCK
    // Use recursive mutex to match windows behaviour
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(p_dlb_mtx, &attr);
#else
    pthread_mutex_init(p_dlb_mtx, NULL);
#endif
#endif
}

void dlb_lip_osa_delete_critial_section(dlb_mutex_t *p_dlb_mtx)
{
#if defined(_MSC_VER)
    DeleteCriticalSection(p_dlb_mtx);
#else
    pthread_mutex_destroy(p_dlb_mtx);
#endif
}

void dlb_lip_osa_init_cond_var(dlb_cond_t *p_dlb_cond)
{
#if defined(_MSC_VER)
    InitializeConditionVariable(p_dlb_cond);
#else
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
#ifndef __MACH__
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
    pthread_cond_init(p_dlb_cond, &attr);
#endif
}

void dlb_lip_osa_delete_cond_var(dlb_cond_t *p_dlb_cond)
{
#if defined(_MSC_VER)
    (void)p_dlb_cond;
#else
    pthread_cond_destroy(p_dlb_cond);
#endif
}

bool dlb_lip_osa_try_enter_critial_section(dlb_mutex_t *p_dlb_mtx)
{
#if defined(_MSC_VER)
    return TryEnterCriticalSection(p_dlb_mtx);
#else
    return pthread_mutex_trylock(p_dlb_mtx) == 0;
#endif
}

void dlb_lip_osa_enter_critial_section(dlb_mutex_t *p_dlb_mtx)
{
#if defined(_MSC_VER)
    EnterCriticalSection(p_dlb_mtx);
#else
    pthread_mutex_lock(p_dlb_mtx);
#endif
}

void dlb_lip_osa_leave_critial_section(dlb_mutex_t *p_dlb_mtx)
{
#if defined(_MSC_VER)
    LeaveCriticalSection(p_dlb_mtx);
#else
    pthread_mutex_unlock(p_dlb_mtx);
#endif
}

void dlb_lip_osa_signal_condition(dlb_cond_t *p_dlb_cond)
{
#if defined(_MSC_VER)
    WakeConditionVariable(p_dlb_cond);
#else
    pthread_cond_signal(p_dlb_cond);
#endif
}

void dlb_lip_osa_broadcast_condition(dlb_cond_t *p_dlb_cond)
{
#if defined(_MSC_VER)
    WakeAllConditionVariable(p_dlb_cond);
#else
    pthread_cond_broadcast(p_dlb_cond);
#endif
}

int dlb_lip_osa_wait_condition(
    dlb_cond_t *        p_dlb_cond,
    dlb_mutex_t *       p_dlb_mtx,
    uint32_t            timeout_ms,
    unsigned long long *elapse_time_ms)
{
    int                ret        = 0;
    unsigned long long start_time = dlb_lip_osa_get_time_ms();
#if defined(_MSC_VER)
    ret = SleepConditionVariableCS(p_dlb_cond, p_dlb_mtx, timeout_ms) == 0;
#else
    if (timeout_ms == LIP_OSA_INFINITE_TIMEOUT)
    {
        ret = pthread_cond_wait(p_dlb_cond, p_dlb_mtx) == ETIMEDOUT;
    }
    else
    {
        struct timespec abstime;
        long            nsec = (timeout_ms % 1000) * 1000000L;
#ifdef __MACH__
	macos_clock_gettime(&abstime);
#else    	
        clock_gettime(CLOCK_MONOTONIC, &abstime);
#endif
        abstime.tv_sec = abstime.tv_sec + timeout_ms / 1000;

        if (nsec + abstime.tv_nsec >= 1000000000L)
        {
            abstime.tv_nsec = abstime.tv_nsec + nsec - 1000000000L;
            abstime.tv_sec  = abstime.tv_sec + 1;
        }

        ret = pthread_cond_timedwait(p_dlb_cond, p_dlb_mtx, &abstime) == ETIMEDOUT;
    }
#endif
    if (elapse_time_ms)
    {
        *elapse_time_ms = dlb_lip_osa_get_time_ms() - start_time;
    }
    return ret;
}

int dlb_lip_osa_start_thread(
    dlb_thread_t *p_dlb_thread,
#if defined(_MSC_VER)
    void (*func)(void *)
#else
    void *(*func)(void *)
#endif
        ,
    void *arg)
{
#if defined(_MSC_VER)
    *p_dlb_thread = (dlb_thread_t)_beginthread(func, 0, arg); // create thread
    return *p_dlb_thread == NULL;
#else
    return pthread_create(p_dlb_thread, NULL, func, arg) != 0;
#endif
}

void dlb_lip_osa_join_thread(const dlb_thread_t *p_dlb_thread)
{
#if defined(_MSC_VER)
    WaitForSingleObject(*p_dlb_thread, INFINITE);
#else
    pthread_join(*p_dlb_thread, NULL);
#endif
}

#if defined(_MSC_VER)
static void
#else
static void *
#endif
dlb_lip_osa_timer_thread(void *arg)
{
    dlb_lip_osa_timer_t *timer               = (dlb_lip_osa_timer_t *)arg;
    uint32_t             callback_id         = LIP_INVALID_CALLBACK_ID;
    bool                 reschedule_callback = false;

    while (timer->is_running)
    {
        int      timed_out  = 0;
        uint32_t timeout_ms = 0;

        dlb_lip_osa_enter_critial_section(&timer->critical_section);
        if (callback_id == timer->callback_id)
        {
            if (reschedule_callback)
            {
                timer->timeout_ms = 1;
            }
            else
            {
                timer->timeout_ms = LIP_OSA_INFINITE_TIMEOUT;
            }
        }
        reschedule_callback = false;
        timeout_ms          = timer->timeout_ms;
        callback_id         = timer->callback_id;

        while (callback_id == timer->callback_id && !timed_out && timer->is_running)
        {
            unsigned long long elapsed_time = 0;

            timed_out = dlb_lip_osa_wait_condition(&timer->condition_var, &timer->critical_section, timeout_ms, &elapsed_time);

            if (!timed_out)
            {
                if (timeout_ms != LIP_OSA_INFINITE_TIMEOUT)
                {
                    timeout_ms = (uint32_t)((elapsed_time > timeout_ms) ? 0 : timeout_ms - elapsed_time);
                }
            }
        }

        if (!timer->is_running || callback_id != timer->callback_id)
        {
            // Ignore time out if we are going to exit thread or callback was canceled in meantime
            timed_out = 0;
        }

        dlb_lip_osa_leave_critial_section(&timer->critical_section);

        if (timed_out)
        {
            reschedule_callback = timer->callback(timer->callback_arg, callback_id) != 0;
        }
    }

#if !defined(_MSC_VER)
    return NULL;
#endif
}

int dlb_lip_osa_init_timer(dlb_lip_osa_timer_t *timer, timer_callback callback, void *callback_arg)
{
    int ret = 0;
    dlb_lip_osa_init_critial_section(&timer->critical_section);
    dlb_lip_osa_enter_critial_section(&timer->critical_section);

    dlb_lip_osa_init_cond_var(&timer->condition_var);
    timer->callback     = callback;
    timer->timeout_ms   = LIP_OSA_INFINITE_TIMEOUT;
    timer->is_running   = true;
    timer->callback_arg = callback_arg;
    timer->callback_id  = LIP_INVALID_CALLBACK_ID;
    if (dlb_lip_osa_start_thread(&timer->thread, dlb_lip_osa_timer_thread, (void *)timer))
    {
        ret = 1;
    }

    dlb_lip_osa_leave_critial_section(&timer->critical_section);

    return ret;
}

uint32_t dlb_lip_osa_set_timer(dlb_lip_osa_timer_t *timer, uint32_t timeout_ms)
{
    uint32_t callback_id = 0;
    dlb_lip_osa_enter_critial_section(&timer->critical_section);

    if (timer->is_running)
    {
        timer->timeout_ms = timeout_ms;
        timer->callback_id += 1;
        if (timer->callback_id == LIP_INVALID_CALLBACK_ID)
        {
            timer->callback_id = 0;
        }
        callback_id = timer->callback_id;
        dlb_lip_osa_signal_condition(&timer->condition_var);
    }

    dlb_lip_osa_leave_critial_section(&timer->critical_section);
    return callback_id;
}

void dlb_lip_osa_cancel_timer(dlb_lip_osa_timer_t *timer)
{
    dlb_lip_osa_enter_critial_section(&timer->critical_section);

    if (timer->is_running)
    {
        timer->timeout_ms = LIP_OSA_INFINITE_TIMEOUT;
        timer->callback_id += 1;
        dlb_lip_osa_signal_condition(&timer->condition_var);
    }

    dlb_lip_osa_leave_critial_section(&timer->critical_section);
}

void dlb_lip_osa_delete_timer(dlb_lip_osa_timer_t *timer)
{
    // Request thread to finish
    dlb_lip_osa_enter_critial_section(&timer->critical_section);
    timer->is_running = false;
    dlb_lip_osa_signal_condition(&timer->condition_var);
    dlb_lip_osa_leave_critial_section(&timer->critical_section);

    // Wait for thread to finish
    dlb_lip_osa_join_thread(&timer->thread);

    // Destroy the mutex object.
    dlb_lip_osa_delete_critial_section(&timer->critical_section);
    // Destroy the conditional variable.
    dlb_lip_osa_delete_cond_var(&timer->condition_var);
}
