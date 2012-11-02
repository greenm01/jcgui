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
 *    This is the Jc_Gui system interface
 *
 * ----------------------------------------------------------------------------
 */

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <sys/stat.h>
#include <string.h>
#include <sndfile.h>
#include <jack/jack.h>
#include <gtk/gtk.h>
#include "Jc_Gui.h"

using namespace gx_engine;
using namespace gx_jack;
using namespace gx_preset;

namespace gx_system
  {
    // ---- retrieve and store the shell variable if not NULL
    void gx_assign_shell_var(const char* name, string& value)
    {
      const char* val = getenv(name);
      value = (val != NULL) ? val : "" ;
    }

    // ---- is the shell variable set ?
    bool gx_shellvar_exists(const string& var)
    {
      return !var.empty();
    }

    // ---- OS signal handler -----
    void gx_signal_handler(int sig)
    {
      // print out a warning
      string msg = string("signal ") + gx_i2a(sig) + " received, exiting ...";
      gx_print_warning("signal_handler", msg);

      gx_clean_exit(NULL, NULL);
    }

    // ---- command line options
    void gx_process_cmdline_options(int& argc, char**& argv, string* optvar)
    {
      // store shell variable content
      for (int i = 0; i < NUM_SHELL_VAR; i++)
        gx_assign_shell_var(shell_var_name[i], optvar[i]);



      // ---- parse command line arguments
      try
        {
          gboolean version = FALSE;
          GOptionEntry opt_entries[] =
          {
            { "version", 'v', 0, G_OPTION_ARG_NONE, &version, "Print version string and exit", NULL },
            { NULL }
          };
          GError* error = NULL;
          GOptionContext* opt_context = NULL;

          opt_context = g_option_context_new(NULL);
          g_option_context_set_summary(opt_context,
                                       "All parameters are optional. Examples:\n"
                                       "\tjc_gui\n"
                                       "\tjc_gui -i system:capture_3 -i system:capture_2\n"
                                       "\tjc_gui -o system:playback_1 -o system:playback_2");
          g_option_context_add_main_entries(opt_context, opt_entries, NULL);



          // JACK options: input and output ports
          gchar** jack_input = NULL;
          gchar** jack_outputs = NULL;
          GOptionGroup* optgroup_jack = g_option_group_new("jack",
                                        "\033[1;32mJACK configuration options\033[0m",
                                        "\033[1;32mJACK configuration options\033[0m",
                                        NULL, NULL);
          GOptionEntry opt_entries_jack[] =
          {
            { "jack-input", 'i', 0, G_OPTION_ARG_STRING_ARRAY, &jack_input, "Jc_Gui JACK input", "PORT" },
            {"jack-output", 'o', 0, G_OPTION_ARG_STRING_ARRAY, &jack_outputs, "Jc_Gui JACK outputs", "PORT" },
            { NULL }
          };
          g_option_group_add_entries(optgroup_jack, opt_entries_jack);

          // collecting all option groups

          g_option_context_add_group(opt_context, optgroup_jack);

          // parsing command options
          if (!g_option_context_parse(opt_context, &argc, &argv, &error))
            {
              throw string(error->message);
            }
          g_option_context_free(opt_context);


          // ----------- processing user options -----------

          // *** display version if requested
          if (version)
            {
              cout << "Jc_Gui version \033[1;32m"
              << GX_VERSION << endl
              << "\033[0m   Copyright " << (char)0x40 << " 2009 "
              << "Hermman Meyer - James Warden"
              << endl;
              exit(0);
            }


// *** process jack outputs
          if (jack_input != NULL)
            {
              int idx = JACK_INP1;
              unsigned int i = 0;

              while (jack_input[i] != NULL)
                {
                  if (i >= 2)
                    {
                      gx_print_warning("main",
                                       "Warning --> provided more than 2 input ports, ignoring extra ports");
                      break;
                    }
                  optvar[idx] = string(jack_input[i]);
                  i++;
                  idx++;
                }
              g_strfreev(jack_input);
            }
          else
            {
              if (!gx_shellvar_exists(optvar[JACK_INP1])) optvar[JACK_INP1] = "";
              if (!gx_shellvar_exists(optvar[JACK_INP2])) optvar[JACK_INP2] = "";
            }


          // *** process jack outputs
          if (jack_outputs != NULL)
            {
              int idx = JACK_OUT1;
              unsigned int i = 0;

              while (jack_outputs[i] != NULL)
                {
                  if (i >= 2)
                    {
                      gx_print_warning("main",
                                       "Warning --> provided more than 2 output ports, ignoring extra ports");
                      break;
                    }
                  optvar[idx] = string(jack_outputs[i]);
                  i++;
                  idx++;
                }
              g_strfreev(jack_outputs);
            }
          else
            {
              if (!gx_shellvar_exists(optvar[JACK_OUT1])) optvar[JACK_OUT1] = "";
              if (!gx_shellvar_exists(optvar[JACK_OUT2])) optvar[JACK_OUT2] = "";
            }




        }

      // ---- catch exceptions that occured during user option parsing
      catch (string& e)
        {
          string msg = string("Error in user options! try -?  ") + e;
          gx_print_error("main", msg);
          exit(1);
        }
    }

    // ---- log message handler
    void gx_print_logmsg(const char* func, const string& msg, GxMsgType msgtype)
    {
      string msgbuf("  ");
      msgbuf += func;
      msgbuf += "  ***  ";
      msgbuf += msg;

      // log the stuff to the log message window if possible
      bool terminal  = true;

      // if no window, then terminal
      if (terminal) cerr << msgbuf << endl;

    }


    // warning
    void gx_print_warning(const char* func, const string& msg)
    {
      gx_print_logmsg(func, msg, kWarning);
    }

    // error
    void gx_print_error(const char* func, const string& msg)
    {
      gx_print_logmsg(func, msg, kError);
    }

    // info
    void gx_print_info(const char* func, const string& msg)
    {
      gx_print_logmsg(func, msg, kInfo);
    }


    // ---- check version and if directory exists and create it if it not exist
    bool gx_version_check()
    {
      struct stat my_stat;

      //----- this check dont need to be against real version, we only need to know
      //----- if the presethandling is working with the courent version, we only count this
      //----- string when we must remove the old preset files.
      string rcfilename =
        gx_user_dir + string("version-") + string("0.01") ;

      if  (stat(gx_user_dir.c_str(), &my_stat) == 0) // directory exists
        {
          // check which version we're dealing with
          if  (stat(rcfilename.c_str(), &my_stat) != 0)
            {
              // current version not there, let's create it and refresh the whole shebang
              string oldfiles = gx_user_dir + string("Jc_Gui*rc");
              (void)gx_system_call ("rm -f", oldfiles.c_str(), false);

              oldfiles = gx_user_dir + string("version*");
              (void)gx_system_call ("rm -f", oldfiles.c_str(), false);

              oldfiles = gx_user_dir + string("*.conf");
              (void)gx_system_call ("rm -f", oldfiles.c_str(), false);

              // setting file for current version
              ofstream f(rcfilename.c_str());
              string cim = string("Jc_Gui-") + GX_VERSION;
              f << cim <<endl;
              f.close();

              // --- default jconv setting
              (void)gx_jconv::gx_save_jconv_settings(NULL, NULL);
            }
        }
      else // directory does not exist
        {
          // create .Jc_Gui directory
          (void)gx_system_call("mkdir -p", gx_user_dir.c_str(), false);

          // setting file for current version
          ofstream f(rcfilename.c_str());
          string cim = string("Jc_Gui-") + GX_VERSION;
          f << cim <<endl;
          f.close();

          // --- create setting file
          string tmpstr = "";

          // --- default jconv setting
          (void)gx_jconv::gx_save_jconv_settings(NULL, NULL);

          // --- version file
          //same here, we only change this file, when the presethandling is broken,
          // otherwise we can let it untouched
          tmpstr = gx_user_dir + string("version-") + string("0.01");
          (void)gx_system_call("touch", tmpstr.c_str(), false);

          cim = string("echo 'Jc_Gui-") + string(GX_VERSION) + "' >";
          (void)gx_system_call(cim.c_str(), tmpstr.c_str(), false);

          cim = "echo -e '" + string(default_setting) + "' >";
          (void)gx_system_call(cim.c_str(), tmpstr.c_str(), false);
        }

      // initialize with what we already have, if we have something
      string s;
      gx_jconv::GxJConvSettings::instance()->configureJConvSettings(s);

      return TRUE;
    }

    //----- we must make sure that the images for the status icon be there
    int gx_pixmap_check()
    {
      struct stat my_stat;

      string gx_pix   = gx_pixmap_dir + "Jc_Gui.png";
      string warn_pix = gx_pixmap_dir + "Jc_Gui-warn.png";

      if ((stat(gx_pix.c_str(), &my_stat) != 0)   ||
          (stat(warn_pix.c_str(), &my_stat) != 0))

        {
          gx_print_error("Pixmap Check", " cannot find installed pixmaps! giving up ...");

          // giving up
          return 1;
        }

      GtkWidget *ibf =  gtk_image_new_from_file (gx_pix.c_str());
      gx_gui::ib = gtk_image_get_pixbuf (GTK_IMAGE(ibf));

      GtkWidget *stir = gtk_image_new_from_file (warn_pix.c_str());
      gx_gui::ibr = gtk_image_get_pixbuf (GTK_IMAGE(stir));

      return 0;
    }

    //----convert int to string
    void gx_IntToString(int i, string & s)
    {
      s = "";

      int abs_i = abs(double(i));
      do
        {
          // note: using base 10 since 10 digits (0123456789)
          char c = static_cast<char>(ASCII_START+abs_i%10);
          s.insert(0, &c, 1);
        }
      while ((abs_i /= 10) > 0);
      if (i < 0) s.insert(0, "-");
    }

    const string& gx_i2a(int i)
    {
      static string str;
      gx_IntToString(i, str);

      return str;
    }

    //----clean up preset name given by user
    void gx_nospace_in_name(string& name, const char* subs)
    {
      int p = name.find(' ', 0);
      while (p != -1)
        {
          name.replace(p++, 1, subs);
          p = name.find(' ', p);
        }
    }

    //----abort Jc_Gui
    void gx_abort(void* arg)
    {
      gx_print_warning("gx_abort", "Aborting Jc_Gui, ciao!");
      exit(1);
    }

    //---- Jc_Gui system function
    int gx_system_call(const char* name1,
                       const char* name2,
                       const bool  devnull,
                       const bool  escape)
    {
      string str(name1);
      str.append(" ");
      str.append(name2);

      if (devnull)
        str.append(" 1>/dev/null 2>&1");

      if (escape)
        str.append("&");

      //    cerr << " ********* \n system command = " << str.c_str() << endl;

      return system(str.c_str());
    }

    // polymorph1
    int gx_system_call(const char*   name1,
                       const string& name2,
                       const bool  devnull,
                       const bool  escape)
    {
      return gx_system_call(name1, name2.c_str(), devnull, escape);
    }

    // polymorph2
    // int gx_system_call(const string& name1,
    //    	     const string& name2,
    // 		     const bool  devnull,
    // 		     const bool  escape)
    // {
    //   return gx_system_call(name1.c_str(), name2.c_str(), devnull, escape);
    // }

    // polymorph3
    int gx_system_call(const string& name1,
                       const char*   name2,
                       const bool  devnull,
                       const bool  escape)
    {
      return gx_system_call(name1.c_str(), name2, devnull, escape);
    }


    //----- clean up when shut down
    void gx_destroy_event()
    {
      (void)gx_child_process::ChildProcess::instance()->gx_terminate_child_procs();

      gx_jack::NO_CONNECTION = 1;

      checky = (float)kEngineOff;

      GtkRegler::gtk_regler_destroy();

      if (G_IS_OBJECT(gx_gui::ib))
        g_object_unref(gx_gui::ib);

      if (G_IS_OBJECT(gx_gui::ibr))
        g_object_unref(gx_gui::ibr);

      if (G_IS_OBJECT(gx_gui::tribeimage))
        g_object_unref(gx_gui::tribeimage);

     /* if (G_IS_OBJECT(gx_gui::_image))
        g_object_unref(gx_gui::_image);*/

      gtk_main_quit();
    }

    //-----Function that must be called before complete shutdown
    void gx_clean_exit(GtkWidget* widget, gpointer data)
    {
      // save DSP state
      GxEngine* engine = GxEngine::instance();
      if (engine->isInitialized())
        {
          string previous_state = gx_user_dir + client_name + "rc";
          engine->get_latency_warning_change();


          // only save if we are not in a preset context
          if (!setting_is_preset)
            gx_gui::GxMainInterface::instance()->
            saveStateToFile(previous_state.c_str());
        }

      if (gx_gui::fWindow)
        gx_destroy_event();

      // clean jack client stuff
      GxJack::instance()->gx_jack_cleanup();
      // delete the locked mem buffers
      if (get_frame)
        delete[] get_frame;
      if (get_frame1)
        delete[] get_frame1;

      exit(GPOINTER_TO_INT(data));
    }


  } /* end of gx_system namespace */
