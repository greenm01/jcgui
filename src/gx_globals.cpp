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
 *
 *  This is the Jc_Gui global variable definitions for all namespaces
 *
 * --------------------------------------------------------------------------
 */

#include <cstring>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <sndfile.h>
#include <jack/jack.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "Jc_Gui.h"

/* ------------------------------------------------------------------------- */

/* ----- main engine ----- */
namespace gx_engine
  {

    float  checky      = 1.0;
    float* get_frame   = NULL;
    float* get_frame1   = NULL;



    /* number of channels */
    int    gNumInChans;
    int    gNumOutChans;

    float* gInChannel [4];
    float* gOutChannel[4];

    /* latency warning  switch */
    float fwarn_swap;
    float fwarn;

    /* engine init state  */
    bool initialized = false;

    /* buffer ready state */
    bool buffers_ready = false;

    int is_setup = 0;
  }


/* ------------------------------------------------------------------------- */

/* ----- jack namespace ----- */
namespace gx_jack
  {
    const int nIPorts = 4; // mono input
    const int nOPorts = 4; // stereo output + jconv
    int NO_CONNECTION = 1;

    /* variables */
    jack_nframes_t      last_xrun_time;
    jack_nframes_t      jack_sr;   // jack sample rate
    jack_nframes_t      jack_bs;   // jack buffer size
    float               jcpu_load; // jack cpu_load

    jack_client_t*      client ;
    jack_port_t*        output_ports[nOPorts];
    jack_port_t*        input_ports [nIPorts];


    jack_nframes_t      time_is;

    bool                jack_is_down = false;
    bool                jack_is_exit = false;
    GxJackLatencyChange change_latency;

    string client_name  = "Jc_Gui";
    string client_out_graph = "";

    string gx_port_names[] =
    {
      "in_0",
      "in_1",
      "out_0",
      "out_1",
      "out_2",
      "out_3"
    };

  }

/* ------------------------------------------------------------------------- */

/* ----- JConv namespace ----- */
namespace gx_jconv
  {
    /* some global vars */
    float checkbox7;
    GtkWidget* mslider;
    bool jconv_is_running = false;
  }

/* ------------------------------------------------------------------------- */

/* ----- preset namespace ----- */
namespace gx_preset
  {
    /* global var declarations */
    GdkModifierType list_mod[] =
    {
      GDK_NO_MOD_MASK,
      GDK_CONTROL_MASK,
      GDK_MOD1_MASK,
      GdkModifierType(GDK_CONTROL_MASK|GDK_MOD1_MASK)
    };

    const char* preset_accel_path[] =
    {
      "<Jc_Gui>/Load",
      "<Jc_Gui>/Save",
      "<Jc_Gui>/Rename",
      "<Jc_Gui>/Delete"
    };

    const char* preset_menu_name[] =
    {
      "_Load Preset...",
      "_Save Preset...",
      "_Rename Preset...",
      "_Delete Preset..."
    };

    map<GtkMenuItem*, string> preset_list[GX_NUM_OF_PRESET_LISTS];

    string gx_current_preset;
    string old_preset_name;

    GtkWidget* presmenu[GX_NUM_OF_PRESET_LISTS];
    GtkWidget* presMenu[GX_NUM_OF_PRESET_LISTS];

    vector<string> plist;
    bool setting_is_preset = false;

    GCallback preset_action_func[] =
    {
      G_CALLBACK(gx_load_preset),
      G_CALLBACK(gx_save_oldpreset),
      G_CALLBACK(gx_rename_preset_dialog),
      G_CALLBACK(gx_delete_preset_dialog)
    };
  }

/* ------------------------------------------------------------------------- */

/* ----- child process namespace ----- */
namespace gx_child_process
  {
    /* global var declarations */


    FILE*    jconv_stream;
    string   mbg_pidfile;

    pid_t child_pid[1] =
    {
      NO_PID
    };
  }

/* ------------------------------------------------------------------------- */

/* ----- system namespace ----- */
namespace gx_system
  {
    /* variables and constants */
    const int SYSTEM_OK = 0;

    string rcpath;

    const char* jc_gui_dir     = ".Jc_Gui";

    const char* jc_gui_preset  = "jc_guiprerc";

    const char* default_setting  = "0 0 0 0 0 0 0 0 0 0 0 0 0 \n";

    const string gx_pixmap_dir = string(GX_PIXMAPS_DIR) + "/";

    const string gx_user_dir   = string(getenv ("HOME")) + string("/") + string(jc_gui_dir) + "/";

    /* shell variable names */
    const char* shell_var_name[] =
    {
      "JC_GUI2JACK_INPUTS1",
      "JC_GUI2JACK_INPUTS2",
      "JC_GUI2JACK_OUTPUTS1",
      "JC_GUI2JACK_OUTPUTS2"

    };
  }


/* ------------------------------------------------------------------------- */

/* ----- GUI namespace ----- */
namespace gx_gui
  {

    /* global GUI widgets */
    GtkWidget* fWindow;
    GtkWidget* menuh;
    GtkWidget* pb;
    GtkWidget* fbutton;
    GtkWidget* jc_dialog;

    /* wave view widgets */

    GdkPixbuf* ib;
    GdkPixbuf* ibr;
    GdkPixbuf* bbr;
    GdkPixbuf *tribeimage;
    GdkPixbuf *_image;

    /* jack server status icons */
    GtkWidget* gx_jackd_on_image;
    GtkWidget* gx_jackd_off_image;

    /* engine status images */
    GtkWidget* gx_engine_on_image;
    GtkWidget* gx_engine_off_image;

    GtkWidget* gx_engine_item;

    /* some more widgets. Note: change names please! */
    GtkWidget* label1;
    GtkWidget* label6;

    GtkStatusIcon* status_icon;

    int refresh_jack = 200;
    int refresh_ports = 210;

    bool new_wave_view = false;
    /* for level display */
    int meter_falloff = 27; // in dB/sec.
    int meter_display_timeout = 60; // in millisec
    int update_gui = 40; //in millisec

    /* names of port lists (exclude MIDI for now) */
    string port_list_names[NUM_PORT_LISTS] =
    {
      string("AudioIPL"),
      string("AudioIPR"),
      string("AudioOPL"),
      string("AudioOPR")
    };

    /* client port queues */
    class StringComp;

    multimap<string, int, StringComp> gx_client_port_queue;
    multimap<string, int, StringComp> gx_client_port_dequeue;



  }


/* ------------------------------------------------------------------------- */
