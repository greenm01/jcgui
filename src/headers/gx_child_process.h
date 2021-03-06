/*
 * Copyright (C) 2009 Hermann Meyer and James Warden
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * --------------------------------------------------------------------------
 */

/* ------- This is the Child Processes namespace ------- */

#pragma once

//#define NUM_OF_CHILD_PROC (2)
#define NO_PID (-1)


#define JCONV_IDX   (0)

namespace gx_child_process
{
  /* --------------- function declarations -------------- */

  class ChildProcess {
    private:
      pid_t    child_pid[2];
      FILE*    jconv_stream;
      FILE*    gx_popen(const char*, const char*, const int);
      int      gx_pclose(FILE*, const int);
      pid_t    gx_find_child_pid(const char*);
      bool     gx_lookup_pid(const pid_t);
      ChildProcess() {child_pid[0]=NO_PID; child_pid[1]=NO_PID;};
    public:
      void     gx_start_stop_jconv(GtkWidget*, gpointer);
      int      gx_terminate_child_procs();
      static inline ChildProcess* instance() {
          static ChildProcess child;
          return &child;
      }
  };
  /* -------------------------------------------------------------------------- */
} /* end of gx_child_process namespace */
