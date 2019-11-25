#include "wcx_callback.h"


namespace wcx {

void callback::reset()
{
  tell_time_reset(-60*1000);
  m_prev_read_size = 0;
  m_read_size = 0;
  m_tell_size = 0;
  m_total_read = 0;
}

int callback::tell_process_data(UINT64 size, bool forced)
{
  if (m_total_read == 0) {
    return tell_process_data_by_writing(size, forced);
  }
  return tell_process_data_by_reading(size, forced);
}

int callback::tell_process_data_by_writing(UINT64 size, bool forced)
{
  int hr = psProcess;
  UINT64 delta;

  if (size == 0) {
    // process new file
    m_prev_read_size += m_read_size;
    m_read_size = 0;    
    forced = true;
  }
  delta = m_prev_read_size + size - m_tell_size;
  FIN_IF(delta == 0, psProcess);  // skip double telling
  m_read_size = size;
  if (forced || delta >= tell_size_factor || GetTickBetween(m_prev_tell_time, GetTickCount()) > 500) {
    WLOGi(L"%S: \"%s\" size = %I64d (%d) ", __func__, m_FileName, size, (int)delta);
    if (delta > INT_MAX) {
      WLOGi(L"%S: <<<ERROR>>> delta = %I64d > %d ", delta, INT_MAX);
      delta = INT_MAX;   // CRITICAL ERROR !!!
    }
    hr = m_fn.ProcessDataProcW(m_FileName, (INT)(UINT)delta);
    //DWORD tid = GetCurrentThreadId();
    //if (tid != m_thread_id) {
    //  LOGd_IF(m_thread_id, "%s: <<< CHANGE THREAD: 0x%X to 0x%X >>>", __func__, m_thread_id, tid);
    //  m_thread_id = tid;
    //}
    LOGi_IF(hr == psCancel, "%s: ---<<<USER PRESS CANCEL>>>---", __func__);
    tell_time_reset();
    m_tell_size = m_prev_read_size + size;
  }

fin:
  return hr;
}

int callback::tell_skip_file(UINT64 file_size, bool forced)
{
  for (UINT64 s = 0; /*nothing*/ ; s += tell_size_factor) {
    int ret = tell_process_data((s < file_size) ? s : file_size, forced);
    if (ret == psCancel)
      return psCancel;   // user press Cancel
    if (s >= file_size)
      break;
  }
  return psProcess;
}


int callback::tell_process_data_by_reading(UINT64 size, bool forced)
{
  int hr = psProcess;
  if (forced || GetTickBetween(m_prev_tell_time, GetTickCount()) > 500) {
    int progress = 0;
    if (size > 0) {
      if (size < m_total_read) {
        UINT64 x = (size * 100) / m_total_read;
        progress = (int)x;
      } else {
        progress = 100;
      }
    }
    WLOGi(L"%S: \"%s\" total persent = %d ", __func__, m_FileName, progress);
    hr = m_fn.ProcessDataProcW(m_FileName, -progress);
    LOGi_IF(hr == psCancel, "%s: ---<<<USER PRESS CANCEL>>>---", __func__);
    tell_time_reset();
  }
  return hr;
}

int callback::update_top_progressbar(int percent)
{
  int hr = psProcess;
  FIN_IF(!m_fn.ProcessDataProcW, psProcess);
  if (percent > 0) {
    WLOGi(L"%S: \"%s\" percent = %d ", __func__, m_FileName, percent);
    hr = m_fn.ProcessDataProcW(m_FileName, -percent);
    FIN_IF(hr == psCancel, psCancel);  // user press Cancel
  }
fin:
  return hr;
}

int callback::tell_process_data2(int percent, int total_percent)
{
  int hr = psProcess;
  FIN_IF(!m_fn.ProcessDataProcW, psProcess);
  if (percent > 0) {
    WLOGi(L"%S: \"%s\" percent = %d ", __func__, m_FileName, percent);
    hr = m_fn.ProcessDataProcW(m_FileName, -percent);
    tell_time_reset();
    FIN_IF(hr == psCancel, psCancel);  // user press Cancel
  }
  if (total_percent >= 0) {
    WLOGi(L"%S: \"%s\" TOTAL percent = %d ", __func__, m_FileName, total_percent);
    hr = m_fn.ProcessDataProcW(m_FileName, (-1000L) - total_percent);
    tell_time_reset();
    FIN_IF(hr == psCancel, psCancel);  // user press Cancel
  }
fin:
  return hr;
}



} /* namespace */

