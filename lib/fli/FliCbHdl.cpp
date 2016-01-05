/******************************************************************************
* Copyright (c) 2015/16 Potential Ventures Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright
*      notice, this list of conditions and the following disclaimer in the
*      documentation and/or other materials provided with the distribution.
*    * Neither the name of Potential Ventures Ltd
*      names of its contributors may be used to endorse or promote products
*      derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL POTENTIAL VENTURES LTD BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include "FliImpl.h"

/**
 * @name    cleanup callback
 * @brief   Called while unwinding after a GPI callback
 *
 * We keep the process but de-sensitise it
 *
 * NB need a way to determine if should leave it sensitised, hmmm...
 *
 */
int FliProcessCbHdl::cleanup_callback(void)
{
    if (m_sensitised) {
        mti_Desensitize(m_proc_hdl);
    }
    m_sensitised = false;
    return 0;
}

FliTimedCbHdl::FliTimedCbHdl(GpiImplInterface *impl,
                             uint64_t time_ps) : GpiCbHdl(impl),
                                                 FliProcessCbHdl(impl),
                                                 m_time_ps(time_ps)
{
    m_proc_hdl = mti_CreateProcessWithPriority(NULL, handle_fli_callback, (void *)this, MTI_PROC_IMMEDIATE);
}

int FliTimedCbHdl::arm_callback(void)
{
    mti_ScheduleWakeup(m_proc_hdl, m_time_ps);
    m_sensitised = true;
    set_call_state(GPI_PRIMED);
    return 0;
}

int FliTimedCbHdl::cleanup_callback(void)
{
    switch (get_call_state()) {
    case GPI_PRIMED:
        /* Issue #188: Work around for modelsim that is harmless to othes too,
           we tag the time as delete, let it fire then do not pass up
           */
        LOG_DEBUG("Not removing PRIMED timer %p", m_time_ps);
        set_call_state(GPI_DELETE);
        return 0;
    case GPI_CALL:
        LOG_DEBUG("Not removing CALL timer yet %p", m_time_ps);
        set_call_state(GPI_DELETE);
        return 0;
    case GPI_DELETE:
        LOG_DEBUG("Removing Postponed DELETE timer %p", m_time_ps);
        break;
    default:
        break;
    }
    FliProcessCbHdl::cleanup_callback();
    FliImpl* impl = (FliImpl*)m_impl;
    impl->cache.put_timer(this);
    return 0;
}

int FliSignalCbHdl::arm_callback(void)
{
    if (NULL == m_proc_hdl) {
        LOG_DEBUG("Creating a new process to sensitise to signal %s", mti_GetSignalName(m_sig_hdl));
        m_proc_hdl = mti_CreateProcess(NULL, handle_fli_callback, (void *)this);
    }

    if (!m_sensitised) {
        mti_Sensitize(m_proc_hdl, m_sig_hdl, MTI_EVENT);
        m_sensitised = true;
    }
    set_call_state(GPI_PRIMED);
    return 0;
}

int FliSimPhaseCbHdl::arm_callback(void)
{
    if (NULL == m_proc_hdl) {
        LOG_DEBUG("Creating a new process to sensitise with priority %d", m_priority);
        m_proc_hdl = mti_CreateProcessWithPriority(NULL, handle_fli_callback, (void *)this, m_priority);
    }

    if (!m_sensitised) {
        mti_ScheduleWakeup(m_proc_hdl, 0);
        m_sensitised = true;
    }
    set_call_state(GPI_PRIMED);
    return 0;
}

FliSignalCbHdl::FliSignalCbHdl(GpiImplInterface *impl,
                               FliSignalObjHdl *sig_hdl,
                               unsigned int edge) : GpiCbHdl(impl),
                                                    FliProcessCbHdl(impl),
                                                    GpiValueCbHdl(impl, sig_hdl, edge)
{
    m_sig_hdl = m_signal->get_handle<mtiSignalIdT>();
}
