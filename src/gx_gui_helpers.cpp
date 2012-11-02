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
 * ---------------------------------------------------------------------------
 *
 *    This is the Jc_Gui GUI related functionality
 *
 * ----------------------------------------------------------------------------
 */
#ifndef NJACKLAT
#define NJACKLAT (9)
#endif

#ifndef NUM_PORT_LISTS
#define NUM_PORT_LISTS (4)
#endif

#include <errno.h>

#include <assert.h>
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <jack/jack.h>
#include <sndfile.h>

#include "Jc_Gui.h"

using namespace gx_system;
using namespace gx_child_process;
using namespace gx_preset;


/* -------- helper for level meter display -------- */
inline float
log_meter (float db)
{
  gfloat def = 0.0f; /* Meter deflection %age */

  if (db < -70.0f)
    {
      def = 0.0f;
    }
  else if (db < -60.0f)
    {
      def = (db + 70.0f) * 0.25f;
    }
  else if (db < -50.0f)
    {
      def = (db + 60.0f) * 0.5f + 2.5f;
    }
  else if (db < -40.0f)
    {
      def = (db + 50.0f) * 0.75f + 7.5f;
    }
  else if (db < -30.0f)
    {
      def = (db + 40.0f) * 1.5f + 15.0f;
    }
  else if (db < -20.0f)
    {
      def = (db + 30.0f) * 2.0f + 30.0f;
    }
  else if (db < 6.0f)
    {
      def = (db + 20.0f) * 2.5f + 50.0f;
    }
  else
    {
      def = 115.0f;
    }

  /* 115 is the deflection %age that would be
     when db=6.0. this is an arbitrary
     endpoint for our scaling.
  */

  return def/115.0f;
}

/* --------- calculate power (percent) to decibel -------- */
// Note: could use fast_log10 (see ardour code) to make it faster
inline float power2db(float power)
{
  return  20.*log10(power);
}

/* --------------------------- gx_gui namespace ------------------------ */
namespace gx_gui
  {

    static const float falloff = meter_falloff * meter_display_timeout * 0.001;

    /* --------- menu function triggering engine on/off --------- */
    void gx_engine_switch (GtkWidget* widget, gpointer arg)
    {
      gx_engine::GxEngineState estate =
        (gx_engine::GxEngineState)gx_engine::checky;

      switch (estate)
        {
        case gx_engine::kEngineOn:
          estate = gx_engine::kEngineOff;

          break;

        case gx_engine::kEngineOff:

          estate = gx_engine::kEngineOn;
          break;

        default:
          estate = gx_engine::kEngineOn;
          gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(gx_engine_item), TRUE
          );
        }

      gx_engine::checky = (float)estate;
      gx_refresh_engine_status_display();
    }

    /* -------------- refresh engine status display ---------------- */
    void gx_refresh_engine_status_display()
    {
      gx_engine::GxEngineState estate =
        (gx_engine::GxEngineState)gx_engine::checky;

      string state;

      switch (estate)
        {

        case gx_engine::kEngineOff:
          gtk_widget_show(gx_engine_off_image);
          gtk_widget_hide(gx_engine_on_image);


          gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(gx_engine_item), FALSE
          );
          state = "OFF";
          break;

        case gx_engine::kEngineOn:
        default: // ON
          gtk_widget_show(gx_engine_on_image);
          gtk_widget_hide(gx_engine_off_image);


          gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(gx_gui::gx_engine_item), TRUE
          );
          state = "ON";
        }

      gx_print_info("Engine State: ", state);
    }

    /* ----------------- refresh GX level display function ---------------- */
    gboolean gx_refresh_meter_level(gpointer args)
    {
      if (gx_jack::client && gx_engine::buffers_ready)
        {

          GxMainInterface* gui = GxMainInterface::instance();

          // data holders for meters
          // Note: removed RMS calculation, we will only focus on max peaks
          float max_level  [2];
          (void)memset(max_level,   0, sizeof(max_level));
          float max_jclevel[2];
          (void)memset(max_jclevel, 0, sizeof(max_jclevel));

          static float old_peak_db  [2] = {-INFINITY, -INFINITY};
          static float old_jcpeak_db[2] = {-INFINITY, -INFINITY};

          jack_nframes_t nframes = gx_jack::GxJack::instance()->get_bz();

          // retrieve meter widgets
          GtkWidget* const* meters   = gui->getLevelMeters();
          GtkWidget* const* jcmeters = gui->getJCLevelMeters();

          if (gx_engine::initialized)
            {

              // fill up from engine buffers
              for (int c = 0; c < 2; c++)
                {
                  // Jc_Gui output levels
                  float data[nframes];

                  // jconv output levels
                  float jcdata[nframes];

                  // need to differentiate between channels due to stereo
                  switch (c)
                    {
                    default:
                    case 0:
                    {
                      (void)memcpy(data, gx_engine::get_frame,  sizeof(data));
                      if ((gx_jconv::jconv_is_running) && (gx_engine::is_setup))
                        (void)memcpy(jcdata, gx_engine::gInChannel[2], sizeof(jcdata));
                    }
                    break;

                    case 1:
                    {
                      (void)memcpy(data, gx_engine::get_frame1, sizeof(data));
                      if ((gx_jconv::jconv_is_running) && (gx_engine::is_setup))
                        (void)memcpy(jcdata, gx_engine::gInChannel[3], sizeof(jcdata));
                    }
                    break;
                    }


                  // calculate  max peak
                  for (guint f = 0; f < nframes; f++)
                    {
                      max_level[c] = max(max_level[c], abs(data[f]));

                      if ((gx_jconv::jconv_is_running) && (gx_engine::is_setup))
                        max_jclevel[c] = max(max_jclevel[c], abs(jcdata[f]));
                    }


                  // update meters (consider falloff as well)
                  if (meters[c])
                    {
                      // calculate peak dB and translate into meter
                      float peak_db = -INFINITY;
                      if (max_level[c] > 0.) peak_db = power2db(max_level[c]);

                      // retrieve old meter value and consider falloff
                      if (peak_db < old_peak_db[c])
                        peak_db = old_peak_db[c] - falloff;

                      gtk_fast_meter_set(GTK_FAST_METER(meters[c]), log_meter(peak_db));
                      old_peak_db[c] = max(peak_db, -INFINITY);
                    }


                  if ((gx_jconv::jconv_is_running) && (jcmeters[c]) && (gx_engine::is_setup))
                    {
                      // calculate peak dB and translate into meter
                      float peak_db = -INFINITY;
                      if (max_jclevel[c] > 0.) peak_db = power2db(max_jclevel[c]);

                      // retrieve old meter value and consider falloff
                      if (peak_db < old_jcpeak_db[c])
                        peak_db = old_jcpeak_db[c] - falloff;

                      gtk_fast_meter_set(GTK_FAST_METER(jcmeters[c]), log_meter(peak_db));
                      old_jcpeak_db[c] = max(peak_db, -INFINITY);
                    }
                }
            }
        }


      return TRUE;
    }



    /* -------------- timeout for jconv startup when Jc_Gui init -------------- */
    gboolean gx_check_startup(gpointer args)
    {

      gx_engine::is_setup = 1;
      if (gx_jconv::GxJConvSettings::checkbutton7 == 1)
        {
          ChildProcess::instance()->gx_start_stop_jconv(NULL,NULL);
        }
      return FALSE;

    }

    /* -------------- for thread that checks jackd liveliness -------------- */
    gboolean gx_survive_jack_shutdown(gpointer arg)
    {
      GtkWidget* wd = GxMainInterface::instance()->getJackConnectItem();
      refresh_jack = 2000;


      // return if jack is not down
      if (gx_system_call("pgrep", "jackd", true) == SYSTEM_OK)
        {
          if (gx_jack::GxJack::instance()->get_is_jack_down())
            {
              // let's make sure we get out of here
              if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(wd)))
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(wd), TRUE);

              // revive existing client menus
              GxMainInterface::instance()->initClientPortMaps();
              gx_check_startup(NULL);

              return TRUE;
            }
          //gx_jack::jcpu_load = jack_cpu_load(gx_jack::client);
        }
      else
        {
          // set jack client to NULL
          gx_jack::client = 0;

          // refresh some stuff. Note that it can be executed
          // more than once, no harm here
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(wd), FALSE);
          GxMainInterface::instance()->deleteAllClientPortMaps();

          gx_jconv::GxJConvSettings::checkbutton7 = 0;
          gx_jconv::jconv_is_running = false;
          gx_jack::GxJack::instance()->set_is_jack_down(true);
        }
      return TRUE;
    }

    /* --------- queue up new client ports as they are registered -------- */
    void gx_queue_client_port(const string name,
                              const string type,
                              const int flags)
    {
      // add the port
      gx_client_port_queue.insert(pair<string, int>(name, flags));
    }

    /* --------- dequeue client ports as they are deregistered -------- */
    void gx_dequeue_client_port(const string name,
                                const string type,
                                const int flags)
    {
      // remove the port
      gx_client_port_dequeue.insert(pair<string, int>(name, flags));
    }


    /* ---------------------- monitor jack ports  items ------------------ */
    // we also refresh the connection status of these buttons
    gboolean gx_monitor_jack_ports(gpointer args)
    {
      // get gui instance
      GxMainInterface* gui = GxMainInterface::instance();
      refresh_ports = 2100;

      // don't bother if we are not a valid client or if we are in the middle
      // of deleting stuff
      // if we are off jack or jack is down, delete everything
      if (!gx_jack::client || gx_jack::GxJack::instance()->get_is_jack_down())
        {
          gui->deleteAllClientPortMaps();
          gx_client_port_dequeue.clear();
          gx_client_port_queue.clear();
          return TRUE;
        }

      // if the external client left without "unregistering" its ports
      // (yes, it happens, shame on the devs ...), we catch it here
      if (!gx_jack::client_out_graph.empty())
        {
          gx_client_port_dequeue.clear();
          gui->deleteClientPortMap(gx_jack::client_out_graph);
          gx_jack::client_out_graph = "";
          return TRUE;
        }

      // browse queue of added ports and update if needed
      gui->addClientPorts();

      // browse queue of removed ports and update if needed
      gui->deleteClientPorts();

      // loop over all existing clients
      set<GtkWidget*>::iterator cit = gui->fClientPortMap.begin();
      while (cit != gui->fClientPortMap.end())
        {
          // fetch client port map
          GtkWidget* portmap = *cit;
          GList* list = gtk_container_get_children(GTK_CONTAINER(portmap));

          guint len = g_list_length(list);
          if (len == NUM_PORT_LISTS) // something weird ...
            for (guint i = 0; i < len; i++)
              {
                // fetch client table
                GtkWidget* table = (GtkWidget*)g_list_nth_data(list, i);

                // check port connection status for each port
                gtk_container_foreach(GTK_CONTAINER(table),
                                      gx_refresh_portconn_status,
                                      GINT_TO_POINTER(i));
              }

          // next client
          cit++;
        }

      return TRUE;
    }

    /* --------------- refresh port connection button status -------------- */
    void gx_refresh_portconn_status(GtkWidget* button, gpointer data)
    {
      if (GTK_IS_CHECK_BUTTON(button) == FALSE) return;

      // our ports
      jack_port_t* ports[] =
      {
        gx_jack::input_ports [0],
        gx_jack::input_ports [1],
        gx_jack::output_ports[0],
        gx_jack::output_ports[1]
      };

      int index = GPOINTER_TO_INT(data);

      // fetch port name from widget name
      const string port_name = gtk_widget_get_name(button);
      // delete unaktive clients from portmap
      if (!jack_port_by_name  	(  	gx_jack::client,
                                   port_name.c_str()
                               ) 		)
        {
          string  port = port_name;
          port.erase(port.find(":"));
          gx_jack::client_out_graph = port;

          return;
        }
      // connection to port
      int nconn = jack_port_connected_to(ports[index], port_name.c_str());

      // update status
      if (nconn == 0) // no connection
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);

      else if (nconn > 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),  TRUE);

    }

    /* ----- cycle through client tabs in client portmaps notebook  ------*/
    void gx_cycle_through_client_tabs(GtkWidget* item, gpointer data)
    {
      GxMainInterface* gui = GxMainInterface::instance();

      // current page
      int page = gtk_notebook_get_current_page(gui->getPortMapTabs());

      // next page
      gtk_notebook_next_page(gui->getPortMapTabs());

      // are we reaching the end of the notebook ?
      if (page == gtk_notebook_get_current_page(gui->getPortMapTabs()))
        gtk_notebook_set_current_page(gui->getPortMapTabs(), 0);
    }


    //----menu funktion about
    void gx_show_about( GtkWidget *widget, gpointer data )
    {
      static string about;
      if (about.empty())
        {
          about += "\n\n\n  Jc_Gui ";

          about += GX_VERSION;

          about +=
            "\n  is a Host wrap around Jconvolver"
            "\n  by Fons Adriaensen "
            "\n  it allows to create, save and run"
            "\n  configfiles for impulse response"
            "\n  to use with jconv/jconvolver "
            "\n  if you dont have it installed, look here"
            "\n  http://www.kokkinizita.net/linuxaudio/index.html "
            "\n\n  authors: Hermann Meyer <brummer-@web.de>"
            "\n  authors: James Warden <warjamy@yahoo.com>"
            "\n  home: http://jcgui.sourceforge.net/\n";
        }

      gx_message_popup(about.c_str());
    }


    //----- change the jack buffersize on the fly is still experimental, give a warning
    gint gx_wait_latency_warn()
    {
      GtkWidget* warn_dialog = gtk_dialog_new();
      gtk_window_set_destroy_with_parent(GTK_WINDOW(warn_dialog), TRUE);

      GtkWidget* box     = gtk_vbox_new (0, 4);
      GtkWidget* labelt  = gtk_label_new("\nWARNING\n");
      GtkWidget* labelt1 = gtk_label_new("CHANGING THE JACK_BUFFER_SIZE ON THE FLY \n"
                                         "MAY CAUSE UNPREDICTABLE EFFECTS \n"
                                         "TO OTHER RUNNING JACK APPLICATIONS. \n"
                                         "DO YOU WANT TO PROCEED ?");
      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (labelt1, GTK_STATE_NORMAL, &colorGreen);

      GtkStyle *style1 = gtk_widget_get_style(labelt1);
      pango_font_description_set_size(style1->font_desc, 10*PANGO_SCALE);
      pango_font_description_set_weight(style1->font_desc, PANGO_WEIGHT_BOLD);
      gtk_widget_modify_font(labelt1, style1->font_desc);

      gdk_color_parse("#ffffff", &colorGreen);
      gtk_widget_modify_fg (labelt, GTK_STATE_NORMAL, &colorGreen);
      style1 = gtk_widget_get_style(labelt);
      pango_font_description_set_size(style1->font_desc, 14*PANGO_SCALE);
      pango_font_description_set_weight(style1->font_desc, PANGO_WEIGHT_BOLD);
      gtk_widget_modify_font(labelt, style1->font_desc);

      GtkWidget* button1 =
        gtk_dialog_add_button(GTK_DIALOG (warn_dialog),
                              "Yes", gx_jack::kChangeLatency);

      GtkWidget* button2 =
        gtk_dialog_add_button(GTK_DIALOG (warn_dialog),
                              "No",  gx_jack::kKeepLatency);


      GtkWidget* box1    = gtk_hbox_new (0, 4);
      GtkWidget* box2    = gtk_hbox_new (0, 4);

      GtkWidget* disable_warn = gtk_check_button_new();
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_warn), FALSE);
      g_signal_connect(disable_warn, "clicked",
                       G_CALLBACK(gx_user_disable_latency_warn), NULL);

      GtkWidget * labelt2 =
        gtk_label_new ("Don't bother me again with such a question, "
                       "I know what I am doing");

      gtk_container_add (GTK_CONTAINER(box),  labelt);
      gtk_container_add (GTK_CONTAINER(box),  labelt1);
      gtk_container_add (GTK_CONTAINER(box),  box2);
      gtk_container_add (GTK_CONTAINER(box),  box1);
      gtk_container_add (GTK_CONTAINER(box1), disable_warn);
      gtk_container_add (GTK_CONTAINER(box1), labelt2);
      gtk_container_add (GTK_CONTAINER(GTK_DIALOG(warn_dialog)->vbox), box);

      gtk_widget_modify_fg (labelt2, GTK_STATE_NORMAL, &colorGreen);

      GtkStyle *style = gtk_widget_get_style(labelt2);
      pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
      gtk_widget_modify_font(labelt2, style->font_desc);

      g_signal_connect_swapped(button1, "clicked",
                               G_CALLBACK (gtk_widget_destroy), warn_dialog);
      g_signal_connect_swapped(button2, "clicked",
                               G_CALLBACK (gtk_widget_destroy), warn_dialog);

      gtk_widget_show_all(box);

      return gtk_dialog_run (GTK_DIALOG (warn_dialog));
    }

    // check user's decision to turn off latency change warning
    void gx_user_disable_latency_warn(GtkWidget* wd, gpointer arg)
    {
      GtkToggleButton* button = GTK_TOGGLE_BUTTON(wd);
      gx_engine::fwarn_swap = (int)gtk_toggle_button_get_active(button);
    }

//----- hide the extendend settings slider
    void gx_hide_extended_settings( GtkWidget *widget, gpointer data )
    {
      static bool showit = false;

      if (showit == false)
        {
          gtk_widget_hide(GTK_WIDGET(fWindow));
          showit = true;
        }
      else
        {
          gtk_widget_show(GTK_WIDGET(fWindow));
          showit = false;
        }
    }

    //----- systray menu
    void gx_systray_menu( GtkWidget *widget, gpointer data )
    {
      guint32 tim = gtk_get_current_event_time ();
      gtk_menu_popup (GTK_MENU(menuh),NULL,NULL,NULL,(gpointer) menuh,2,tim);
    }

    //---- choice dialog without text entry
    gint gx_nchoice_dialog_without_entry (
      const char* window_title,
      const char* msg,
      const guint nchoice,
      const char* label[],
      const gint  resp[],
      const gint default_response
    )
    {
      GtkWidget* dialog   = gtk_dialog_new();
      GtkWidget* text_label = gtk_label_new (msg);
      gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), text_label);

      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (text_label, GTK_STATE_NORMAL, &colorGreen);

      GdkColor colorBlack;
      gdk_color_parse("#000000", &colorBlack);
      gtk_widget_modify_bg (dialog, GTK_STATE_NORMAL, &colorBlack);

      GtkStyle* text_style = gtk_widget_get_style(text_label);
      pango_font_description_set_size(text_style->font_desc, 10*PANGO_SCALE);
      pango_font_description_set_weight(text_style->font_desc, PANGO_WEIGHT_BOLD);

      gtk_widget_modify_font(text_label, text_style->font_desc);

      for (guint i = 0; i < nchoice; i++)
        {
          GtkWidget* button =
            gtk_dialog_add_button(GTK_DIALOG (dialog), label[i], resp[i]);

          gdk_color_parse("#555555", &colorBlack);
          gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &colorBlack);

          g_signal_connect_swapped(button, "clicked",  G_CALLBACK (gtk_widget_destroy), dialog);
        }

      // set default
      gtk_dialog_set_has_separator(GTK_DIALOG(dialog), TRUE);
      gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_response);
      gtk_window_set_title(GTK_WINDOW(dialog), window_title);

      gtk_widget_show(text_label);

      //--- run dialog and check response
      gint response = gtk_dialog_run (GTK_DIALOG (dialog));
      return response;
    }

    //---- choice dialog without text entry
    gint gx_choice_dialog_without_entry (
      const char* window_title,
      const char* msg,
      const char* label1,
      const char* label2,
      const gint resp1,
      const gint resp2,
      const gint default_response
    )
    {
      const guint nchoice     = 2;
      const char* labels[]    = {label1, label2};
      const gint  responses[] = {resp1, resp2};

      return gx_nchoice_dialog_without_entry(
               window_title,
               msg,
               nchoice,
               labels,
               responses,
               default_response);
    }

    //---- get text entry from dialog
    void gx_get_text_entry(GtkEntry* entry, string& output)
    {
      if (gtk_entry_get_text(entry)[0])
        output = gtk_entry_get_text(entry);
    }

    //---- choice dialog with text entry
    gint gx_choice_dialog_with_text_entry (
      const char* window_title,
      const char* msg,
      const char* label1,
      const char* label2,
      const gint resp1,
      const gint resp2,
      const gint default_response,
      GCallback func
    )
    {
      GtkWidget *dialog, *button1, *button2;
      dialog  = gtk_dialog_new();
      button1 = gtk_dialog_add_button(GTK_DIALOG (dialog), label1, resp1);
      button2 = gtk_dialog_add_button(GTK_DIALOG (dialog), label2, resp2);

      GtkWidget* text_label = gtk_label_new (msg);
      gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), text_label);


      GtkWidget* gtk_entry = gtk_entry_new_with_max_length(32);
      gtk_entry_set_text(GTK_ENTRY(gtk_entry), "");
      gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), gtk_entry);

      g_signal_connect_swapped (button1, "clicked",  G_CALLBACK (func), gtk_entry);
      //    g_signal_connect_swapped (button2, "clicked",  G_CALLBACK (gtk_widget_destroy), dialog);

      gtk_dialog_set_has_separator(GTK_DIALOG(dialog), TRUE);
      gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_response);
      gtk_entry_set_activates_default(GTK_ENTRY(gtk_entry), TRUE);
      GTK_BOX(GTK_DIALOG(dialog)->action_area)->spacing = 4;

      // some display style
      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (text_label, GTK_STATE_NORMAL, &colorGreen);

      GdkColor colorBlack;
      gdk_color_parse("#000000", &colorBlack);
      gtk_widget_modify_bg (dialog, GTK_STATE_NORMAL, &colorBlack);

      GtkStyle* text_style = gtk_widget_get_style(text_label);
      pango_font_description_set_size(text_style->font_desc, 10*PANGO_SCALE);
      pango_font_description_set_weight(text_style->font_desc, PANGO_WEIGHT_BOLD);

      gtk_widget_modify_font(text_label, text_style->font_desc);

      gdk_color_parse("#555555", &colorBlack);
      gtk_widget_modify_bg (button1, GTK_STATE_NORMAL, &colorBlack);

      gdk_color_parse("#555555", &colorBlack);
      gtk_widget_modify_bg (button2, GTK_STATE_NORMAL, &colorBlack);

      // display extra stuff
      gtk_widget_show (text_label);
      gtk_widget_show (gtk_entry);
      gtk_window_set_title(GTK_WINDOW(dialog), window_title);

      // run the dialog and wait for response
      gint response = gtk_dialog_run (GTK_DIALOG(dialog));

      if (dialog) gtk_widget_destroy(dialog);

      return response;
    }


    //---- popup warning
    int gx_message_popup(const char* msg)
    {
      // check msg validity
      if (!msg)
        {
          gx_print_warning("Message Popup",
                           string("warning message does not exist"));
          return -1;
        }

      // build popup window
      GtkWidget *about;
      GtkWidget *label;
      GtkWidget *ok_button;

      about = gtk_dialog_new();
      ok_button  = gtk_button_new_from_stock(GTK_STOCK_OK);

      label = gtk_label_new (msg);

      GtkStyle *style = gtk_widget_get_style(label);

      pango_font_description_set_size(style->font_desc, 10*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);

      gtk_widget_modify_font(label, style->font_desc);

      gtk_label_set_selectable(GTK_LABEL(label), TRUE);

      gtk_container_add (GTK_CONTAINER (GTK_DIALOG(about)->vbox), label);

      GTK_BOX(GTK_DIALOG(about)->action_area)->spacing = 3;
      gtk_container_add (GTK_CONTAINER (GTK_DIALOG(about)->action_area), ok_button);

      g_signal_connect_swapped (ok_button, "clicked",
                                G_CALLBACK (gtk_widget_destroy), about);

      g_signal_connect(label, "expose-event", G_CALLBACK(box1_expose), NULL);
      gtk_widget_show (ok_button);
      gtk_widget_show (label);
      return gtk_dialog_run (GTK_DIALOG(about));
    }

    /* show jack portmap window  */
    void gx_show_portmap_window (GtkWidget* widget, gpointer arg)
    {
      GxMainInterface* gui = GxMainInterface::instance();
      if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE)
        {
          if (!gui->fClientPortMap.empty())
            {
              // get main window position (north east gravity)
              gint x,  y;
              gint wx, wy;
              gtk_window_get_position(GTK_WINDOW(fWindow), &x, &y);
              gtk_window_get_size(GTK_WINDOW(fWindow), &wx, &wy);

              // set position of port map window (north west gravity)
              gtk_window_move(gui->getPortMapWindow(), x+wx, y);
              gtk_widget_set_size_request(GTK_WIDGET(gui->getPortMapWindow()),
                                          480, 200);

              gtk_widget_show(GTK_WIDGET(gui->getPortMapWindow()));
            }
          else
            {
              gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), FALSE);
              gx_print_warning("Show Port Maps",
                               "No Clients to show, jackd either down or we are disconnected");
            }
        }
      else
        {
          gtk_widget_hide(GTK_WIDGET(gui->getPortMapWindow()));
          gtk_widget_grab_focus(fWindow);
        }
    }

    /* show jack portmap window  */
    void gx_hide_portmap_window (GtkWidget* widget, gpointer arg)
    {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), FALSE);
    }

    /* meter button release   */
    void gx_meter_button_release(GdkEventButton* ev, gpointer arg)
    {
      if (ev->button == 1)
        {
          cerr << " button event " << endl;
          GxMainInterface* gui = GxMainInterface::instance();

          GtkWidget* const*  meters = gui->getLevelMeters();
          GtkWidget* const* jmeters = gui->getJCLevelMeters();

          for (int i = 0; i < 2; i++)
            {
              if (meters[i])
                gtk_fast_meter_clear(GTK_FAST_METER(meters[i]));
              if (jmeters[i])
                gtk_fast_meter_clear(GTK_FAST_METER(jmeters[i]));
            }
        }
    }

    /* Update all user items reflecting zone z */
    gboolean gx_update_all_gui(gpointer)
    {
      update_gui = 1000;
      gx_ui::GxUI::updateAllGuis();
      return TRUE;
    }

    void gx_skin_color(cairo_pattern_t *pat)
    {

      cairo_pattern_add_color_stop_rgb (pat, 0, 0.2, 0.2, 0.3);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);


    }



    gboolean box1_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;
      cairo_pattern_t *pat;

      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+5;
      double y0      = wi->allocation.y+5;
      double rect_width  = wi->allocation.width-10;
      double rect_height = wi->allocation.height-10;
      double radius = 38.;

      double x1,y1;

      x1=x0+rect_width;
      y1=y0+rect_height;

      cairo_move_to  (cr, x0, y0 + radius);
      cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
      cairo_line_to (cr, x1 - radius, y0);
      cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
      cairo_line_to (cr, x1 , y1 - radius);
      cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
      cairo_line_to (cr, x0 + radius, y1);
      cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);

      cairo_close_path (cr);

      pat = cairo_pattern_create_linear (0, y0, 0, y1);

      cairo_pattern_add_color_stop_rgba (pat, 1, 0., 0., 0., 0.8);
      cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 0.4);
      cairo_set_source (cr, pat);
      //cairo_rectangle(cr, x0,y0, rect_width, rect_height);
      cairo_fill_preserve (cr);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.8);
      cairo_set_line_width (cr, 9.0);
      cairo_stroke (cr);


      cairo_move_to  (cr, x0, y0 + radius);
      cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
      cairo_line_to (cr, x1 - radius, y0);
      cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
      cairo_line_to (cr, x1 , y1 - radius);
      cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
      cairo_line_to (cr, x0 + radius, y1);
      cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);

      cairo_close_path (cr);


      cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
      cairo_set_line_width (cr, 1.0);
      cairo_stroke (cr);
      cairo_pattern_destroy (pat);
      cairo_destroy(cr);

      return FALSE;
    }


    gboolean box3_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;


      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-11;

      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width+100,  rect_height, 0.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);
      gx_skin_color(pat);
      cairo_set_source (cr, pat);
      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-1);
      cairo_fill (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);

      return FALSE;
    }

    gboolean box4_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;

      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-2;
      //GdkPixbuf *tribeimage = gdk_pixbuf_new_from_xpm_data(tribe_xpm);
      //GdkPixbuf * _image = gdk_pixbuf_scale_simple(tribeimage,rect_width,rect_height,GDK_INTERP_HYPER);


      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width-10,  rect_height, 20.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 0.5, 0.005, 0.005, 0.05);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.02, 0.02, 0.03);
      // gx_skin_color(pat);

      cairo_set_source (cr, pat);

      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-1);
      cairo_fill (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);

      /*  gdk_draw_pixbuf(GDK_DRAWABLE(wi->window), gdk_gc_new(GDK_DRAWABLE(wi->window)),
                            _image, 0, 0,
                            x0, y0, rect_width,rect_height,
                            GDK_RGB_DITHER_NORMAL, 0, 0);
      */
      return FALSE;
    }

    gboolean box5_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;


      /* create a cairo black arc to given widget */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-2;

      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width+100,  rect_height, 0.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);
      gx_skin_color(pat);

      cairo_set_source (cr, pat);

      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-1);
      cairo_fill (cr);

      cairo_move_to (cr, x0+10, y0 + (rect_height*0.5));
      //cairo_line_to (cr, x , y+h);
      cairo_curve_to (cr, x0+30,y0 + (rect_height*0.005), x0+50, y0 + (rect_height*0.995), x0+70, y0 + (rect_height*0.5));
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_set_line_width (cr, 1.0);
      cairo_stroke (cr);
      cairo_move_to (cr, x0+10, y0 + (rect_height*0.5));
      cairo_line_to (cr, x0+75 , y0 + (rect_height*0.5));
      cairo_move_to (cr, x0+10, y0 + (rect_height*0.2));
      cairo_line_to (cr, x0+10 , y0 + (rect_height*0.8));

      cairo_set_source_rgb (cr, 0.2, 0.8, 0.2);
      cairo_set_line_width (cr, 1.0);
      cairo_stroke (cr);

      cairo_pattern_destroy (pat);

      /*  cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_save (cr);
      cairo_translate (cr, x0 + rect_width / 2., y0 + rect_height / 2.);
      cairo_scale (cr, rect_width / 2., rect_height / 2.);
      cairo_arc (cr, 0., 0., 1., 0., 2 * M_PI);
      cairo_restore (cr);
      cairo_fill (cr);  */


      cairo_destroy(cr);

      return FALSE;
    }

    gboolean box6_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;


      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-11;

      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (200, rect_height*0.5, 5,200,  rect_height*0.5, 200.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.08, 0.08, 0.08);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.03, 0.03, 0.03);

      cairo_set_source (cr, pat);
      //cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
      cairo_fill(cr);

      cairo_move_to (cr, x0+rect_width, y0);
      cairo_line_to (cr, x0+rect_width ,y0 + rect_height+3);

      cairo_move_to (cr, x0, y0+ rect_height+3);
      cairo_line_to (cr, x0+rect_width ,y0 + rect_height+3);
      cairo_set_source_rgb (cr, 0., 0., 0.);
      cairo_set_line_width (cr, 2.0);

      cairo_stroke (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);

      return FALSE;
    }


    gboolean box7_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {
      cairo_t *cr;

      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);
      cairo_set_font_size (cr, 7.0);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+2;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-4;

      int  db_points[] = { -50, -40, -20, -30, -10, -3, 0, 4 };
      char  buf[32];

      cairo_rectangle (cr, x0,y0,rect_width,rect_height+2);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width-10,  rect_height, 20.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);


      cairo_set_source (cr, pat);
      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-2);
      cairo_fill (cr);



      for (uint32_t i = 0; i < sizeof (db_points)/sizeof (db_points[0]); ++i)
        {
          float fraction = log_meter (db_points[i]);
          cairo_set_source_rgb (cr, 0.12*i, 1, 0.1);

          cairo_move_to (cr, x0+rect_width*0.2,y0+rect_height - (rect_height * fraction));
          cairo_line_to (cr, x0+rect_width*0.8 ,y0+rect_height -  (rect_height * fraction));
          if (i<6)
            {
              snprintf (buf, sizeof (buf), "%d", db_points[i]);
              cairo_move_to (cr, x0+rect_width*0.32,y0+rect_height - (rect_height * fraction));
            }
          else
            {
              snprintf (buf, sizeof (buf), " %d", db_points[i]);
              cairo_move_to (cr, x0+rect_width*0.34,y0+rect_height - (rect_height * fraction));
            }
          cairo_show_text (cr, buf);
        }

      cairo_set_source_rgb (cr, 0.4, 0.8, 0.4);
      cairo_set_line_width (cr, 0.5);
      cairo_stroke (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);


      return FALSE;
    }

    gboolean box10_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {

      cairo_t *cr;


      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-3;


      _image = gdk_pixbuf_scale_simple(gx_gui::tribeimage,rect_width,rect_height,GDK_INTERP_HYPER);


      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width+100,  rect_height, 0.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);

      cairo_set_source (cr, pat);
      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-1);
      cairo_fill (cr);

      gdk_draw_pixbuf(GDK_DRAWABLE(wi->window), gdk_gc_new(GDK_DRAWABLE(wi->window)),
                      _image, 0, 0,
                      x0, y0, rect_width,rect_height,
                      GDK_RGB_DITHER_NORMAL, 0, 0);

      double radius = 38.;

      double x1,y1;

      x1=x0+rect_width;
      y1=y0+rect_height;

      cairo_move_to  (cr, x0, y0 + radius);
      cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
      cairo_line_to (cr, x1 - radius, y0);
      cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
      cairo_line_to (cr, x1 , y1 - radius);
      cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
      cairo_line_to (cr, x0 + radius, y1);
      cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);

      cairo_close_path (cr);

      pat = cairo_pattern_create_linear (0, y0, 0, y1);

      cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 0.8);
      cairo_pattern_add_color_stop_rgba (pat, 0.5, 0.1, 0.1, 0.1, 0.6);
      cairo_pattern_add_color_stop_rgba (pat, 0, 0.4, 0.4, 0.4, 0.4);
      cairo_set_source (cr, pat);
      //cairo_rectangle(cr, x0,y0, rect_width, rect_height);
      cairo_fill (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);
      g_object_unref(_image);

      return FALSE;
    }
    gboolean box11_expose(GtkWidget *wi, GdkEventExpose *ev, gpointer user_data)
    {

      cairo_t *cr;


      /* create a cairo context */
      cr = gdk_cairo_create(wi->window);

      double x0      = wi->allocation.x+1;
      double y0      = wi->allocation.y+1;
      double rect_width  = wi->allocation.width-2;
      double rect_height = wi->allocation.height-3;


      _image = gdk_pixbuf_scale_simple(gx_gui::tribeimage,rect_width,rect_height,GDK_INTERP_HYPER);


      cairo_rectangle (cr, x0,y0,rect_width,rect_height+3);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_fill (cr);

      cairo_pattern_t*pat = cairo_pattern_create_linear (x0, y0, x0, rect_width);
        //cairo_pattern_create_radial (-50, y0, 5,rect_width+100,  rect_height, 0.0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0.02, 0.02, 0.03);
      cairo_pattern_add_color_stop_rgb (pat, 1, 0.05, 0.05, 0.05);

      cairo_set_source (cr, pat);
      cairo_rectangle (cr, x0+1,y0+1,rect_width-2,rect_height-1);
      cairo_fill (cr);

      gdk_draw_pixbuf(GDK_DRAWABLE(wi->window), gdk_gc_new(GDK_DRAWABLE(wi->window)),
                      _image, 0, 0,
                      x0, y0, rect_width,rect_height,
                      GDK_RGB_DITHER_NORMAL, 0, 0);

      double radius = 38.;

      double x1,y1;

      x1=x0+rect_width;
      y1=y0+rect_height;

      cairo_move_to  (cr, x0, y0 + radius);
      cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
      cairo_line_to (cr, x1 - radius, y0);
      cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
      cairo_line_to (cr, x1 , y1 - radius);
      cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
      cairo_line_to (cr, x0 + radius, y1);
      cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);

      cairo_close_path (cr);

      pat = cairo_pattern_create_linear (0, y0, 0, y1);

      cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 0.8);
      cairo_pattern_add_color_stop_rgba (pat, 0.5, 0.1, 0.1, 0.1, 0.6);
      cairo_pattern_add_color_stop_rgba (pat, 1, 0.4, 0.4, 0.4, 0.4);
      cairo_set_source (cr, pat);
      //cairo_rectangle(cr, x0,y0, rect_width, rect_height);
      cairo_fill (cr);

      cairo_pattern_destroy (pat);
      cairo_destroy(cr);
      g_object_unref(_image);

      return FALSE;
    }

    void gx_init_pixmaps()
    {
      /* XPM */
      static const char * tribe_xpm[] =
      {
        "65 20 3 1",
        " 	c None",
        ".	c #656565",
        "+	c #646464",
        "                                                                 ",
        "                                                                 ",
        "                                                                 ",
        "                                                                 ",
        "                          .           +                          ",
        "                    ..  ++.+++     .   ...++                     ",
        "                    . ++.    +     + +  +.+                      ",
        "   +                 ++  .+ .+     +. +.  ..+                ++  ",
        "  +..    .+        . ++  .+. +       +..  +. .         +    ++.+ ",
        " +   +    .         .   +  .+       +.      +  .      .+   ..  + ",
        " +       +.   .   .  .  +  +.++    .++. +.    +   +.   +    .  + ",
        " ++   .. . +++++.   +  ++.... .   + .++.+   .   ....... ++.   +. ",
        "   + ++ +..+++.++. +      ..          +       . + +.++.++  .+.   ",
        "   .  .+                                                     .   ",
        "   +..                                                      ++   ",
        "   +.                                                        .   ",
        "                                                                 ",
        "                                                                 ",
        "                                                                 ",
        "                                                                 "
      };

      tribeimage = gdk_pixbuf_new_from_xpm_data(tribe_xpm);
    }

    /* ----- delete event ---- */
    gboolean gx_delete_event( GtkWidget *widget, gpointer   data )
    {
      gtk_range_set_value(GTK_RANGE(widget), 0);
      return TRUE;
    }
  } /* end of gx_gui namespace */



