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

/* ------- This is the JACK namespace ------- */

#pragma once

/* On Intel set FZ (Flush to Zero) and DAZ (Denormals Are Zero)
   flags to avoid costly denormals */
#ifdef __SSE__
#include <xmmintrin.h>
#ifdef __SSE2__
#define AVOIDDENORMALS _mm_setcsr(_mm_getcsr() | 0x8040)
#else
#define AVOIDDENORMALS _mm_setcsr(_mm_getcsr() | 0x8000)
#endif
#else
#define AVOIDDENORMALS
#endif

#define NJACKLAT (9) // number of possible latencies

namespace gx_jack
{
  /* latency change confirmation */
  typedef enum {
    kChangeLatency = 1,
    kKeepLatency   = 2
  } GxJackLatencyChange;

 class GxJack {
  private:
      // private vars
      bool                  jack_is_exit;
      bool                  jack_is_down;
      jack_nframes_t        jack_sr;   // jack sample rate
      jack_nframes_t        jack_bs;   // jack buffer size
      GxJackLatencyChange   change_latency;
      // functions
      bool                  gx_start_jack_dialog();
      bool                  gx_start_jack(void* arg);

      /* client callbacks */
      static int  gx_jack_srate_callback      (jack_nframes_t, void*);
      static void gx_jack_shutdown_callback   (void*);
      static int  gx_jack_graph_callback      (void*);
      static int  gx_jack_xrun_callback       (void*);
      static void gx_jack_port_callback       (jack_port_id_t, int, void*);
      static int  gx_jack_buffersize_callback (jack_nframes_t, void*);
      static void gx_jack_portreg_callback    (jack_port_id_t, int, void*);
      static void gx_jack_clientreg_callback  (const char*, int, void*);

      /* processing */
      static int  gx_jack_process             (jack_nframes_t, void*);
      // init static class
      GxJack() {jack_is_exit = false; jack_is_down = false;};
  public:
  /* -------- functions ---------- */
      bool gx_jack_init();
      void gx_jack_callbacks_and_activate();
      void gx_jack_init_port_connection(const string*);
      void gx_set_jack_buffer_size(GtkCheckMenuItem*, gpointer);
      void gx_jack_connection(GtkCheckMenuItem*, gpointer);
      void gx_jack_port_connect(GtkWidget*, gpointer);
      void gx_jack_cleanup();
      

      // getter and setter
      int get_bz()                  { return jack_bs;}
      void set_bz(int x)            { jack_bs = x;}
      int get_sr()                  { return jack_sr;}
      void set_is_jack_exit(bool x) {jack_is_exit = x;}
      bool get_is_jack_exit()       { return jack_is_exit;}
      void set_is_jack_down(bool x) {jack_is_down = x;}
      bool get_is_jack_down()       { return jack_is_down;}
      
      static inline GxJack* instance() {
          static GxJack jack;
          return &jack;
      }

  };
} /* end of jack namespace */
