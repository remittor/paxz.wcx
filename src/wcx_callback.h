#pragma once

#include "stdafx.h"

namespace wcx {

// TotalCmd process status:
const int psCancel  = 0;   /* User press CANCEL */
const int psProcess = 1;

const UINT64 tell_size_factor = INT_MAX / 2;


class callback
{
public:
  callback()
  {
    reset();
    memset(&m_fn, 0, sizeof(m_fn));
  }
  
  ~callback()
  {
    //
  }
  
  void reset();
  void set_file_name(LPCWSTR FileName) { m_FileName = FileName; }
  
  int  tell_process_data(UINT64 size, bool forced = false);
  int  tell_skip_file(UINT64 file_size, bool forced);
  
  void set_total_read(UINT64 arc_file_size) { m_total_read = arc_file_size; }
  int  update_top_progressbar(int percent);
  
  void tell_time_reset(int delta = 0) { m_prev_tell_time = GetTickCount() + (DWORD)delta; }
  void set_ProcessDataProc(tProcessDataProcW proc) { m_fn.ProcessDataProcW = proc; }
  void set_ChangeVolProc(tChangeVolProcW proc) { m_fn.ChangeVolProcW = proc; }

protected:
  int  tell_process_data_by_writing(UINT64 size, bool forced);
  int  tell_process_data_by_reading(UINT64 size, bool forced);

  int  tell_process_data2(int percent, int total_percent);

  LPCWSTR   m_FileName;
  DWORD     m_prev_tell_time;
  UINT64    m_tell_size;
  UINT64    m_read_size;         // readed size for current file
  UINT64    m_prev_read_size;
  UINT64    m_total_read;        // for reading mode only
  
  struct {
    tChangeVolProcW     ChangeVolProcW;
    tProcessDataProcW   ProcessDataProcW;
    tPkCryptProcW       PkCryptProcW;
  } m_fn;
};

} /* namespace */
