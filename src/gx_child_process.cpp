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
 *  This is the Jc_Gui module handling child processes spawned from it
 *
 * --------------------------------------------------------------------------
 */

#include <sys/wait.h>
#include <errno.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cmath>


using namespace std;

#include <gtk/gtk.h>
#include <jack/jack.h>
#include <sndfile.h>

#include "Jc_Gui.h"

using namespace gx_system;
using namespace gx_engine;

namespace gx_child_process
  {

    //----- terminate child processes
    int gx_terminate_child_procs()
    {
      // jconv
      if (child_pid[JCONV_IDX] != NO_PID)
        {
          (void)kill(child_pid[JCONV_IDX], SIGINT);
          (void)gx_pclose(jconv_stream, JCONV_IDX);
        }
      return 0;
    }

    //---- popen revisited for Jc_Gui
    FILE* gx_popen(const char *cmdstring,
                   const char *type,
                   const int proc_idx)
    {
      int pfd[2] = { 0, 0 };
      pid_t	pid;
      FILE	*fp;

      /* only allow "r" */
      if ((type[0] != 'r') || (type[1] != 0))
        {
          errno = EINVAL;		/* required by POSIX.2 */
          return(NULL);
        }

      if (pipe(pfd) < 0)
        return(NULL);	/* errno set by pipe() */

      if ((pid = fork()) < 0)
        return(NULL);	/* errno set by fork() */

      else if (pid == 0)
        {
          close(pfd[0]);
          if (pfd[1] != STDOUT_FILENO)
            {
              dup2(pfd[1], STDOUT_FILENO);
              close(pfd[1]);
            }

          /* close descriptor in child_pid[] */
          if (child_pid[0] > 0)
            close(0);

          execl("/bin/sh", "sh", "-c", cmdstring, (char *) 0);
          _exit(127);
        }

      /* parent */
      close(pfd[1]);

      if ((fp = fdopen(pfd[0], type)) == NULL)
        return(NULL);

      child_pid[proc_idx] = pid; /* remember child pid for this fd */
      return(fp);
    }

    //---- pclose revisited for Jc_Gui
    int gx_pclose(FILE *fp, const int proc_idx)
    {
      int stat;
      pid_t	pid;

      if ((pid = child_pid[proc_idx]) == 0)
        return(-1);	/* fp wasn't opened by gx_popen() */

      // reset internal process pid
      child_pid[proc_idx] = NO_PID;

      // check control stream
      if (!fp)
        return(-1);

      // close it
      if (fclose(fp) == EOF)
        return(-1);

      while (waitpid(pid, &stat, 0) < 0)
        if (errno != EINTR)
          return(-1); /* error other than EINTR from waitpid() */

      return(stat);	/* return child's termination status */
    }

    // -------------------------------------------
    bool gx_lookup_pid(const pid_t child_pid)
    {
      // --- this function looks up the given PID from the list of processes
      // it returns true if a match is found.
      string pstr = gx_i2a(child_pid) + " -o pid";

      return (gx_system_call("ps -p", pstr.c_str(), true) == SYSTEM_OK) ?
             true : false;
    }

    //---- find latest process ID by reading stdout from pgrep -n
    pid_t gx_find_child_pid(const char* procname)
    {
      // --- this function retrieves the latest PID of a named process.
      // it is to be called just after Jc_Gui spawns a child process

      // file desc
      int fd[2];
      pid_t pid = NO_PID;

      // piping
      if (pipe(fd) < 0)
        return NO_PID;

      // forking
      if ((pid = fork()) < 0)
        return NO_PID;

      else if (pid == 0)
        {
          close(fd[0]);

          if (fd[1] != STDOUT_FILENO)
            {
              dup2(fd[1], STDOUT_FILENO);
              close(fd[1]);
            }

          // executing 'pgrep -n <proc name>'
          if (execl("/usr/bin/pgrep", "pgrep", "-n", procname, NULL) < 0)
            return NO_PID;
        }
      else
        {
          close(fd[1]);

          int rv;
          char line[16];

          // read stdout
          if ( (rv = read(fd[0], line, 16)) > 0)
            {
              string str = line;
              str.resize(rv-1); // remove extra character crap
              pid = atoi(str.c_str());
            }
        }

      return pid;
    }


    // ---------------  start stop JConv
    void gx_start_stop_jconv(GtkWidget *widget, gpointer data)
    {

      if (gx_jconv::GxJConvSettings::checkbutton7 == 0)
        {
          gx_jconv::checkbox7 = 1.0;

          pid_t pid = gx_child_process::child_pid[JCONV_IDX];

          // if jconv is already running, we have to kill it
          // applying a new jconv setting is not a runtime thing ... :(
          if (pid != NO_PID)
            {
              gx_print_info("JConv Start / Stop", string("killing JConv, PID = ") +
                            gx_system::gx_i2a(pid));

              gx_jconv::jconv_is_running = false;
              (void)kill(pid, SIGINT);
              usleep(100000);

              // gx_jconv::jconv_is_running = false;
              for (int c = 0; c < 2; c++)
                gtk_widget_hide(gx_gui::GxMainInterface::instance()->
                                getJCLevelMeters()[c]);

              // unregister our own jconv dedicated ports
              if (gx_jack::client)
                {
                  if (jack_port_is_mine (gx_jack::client, gx_jack::output_ports[3]))
                    {
                      jack_port_unregister(gx_jack::client, gx_jack::output_ports[3]);
                      gx_engine::gNumOutChans--;
                    }

                  if (jack_port_is_mine (gx_jack::client, gx_jack::output_ports[2]))
                    {
                      jack_port_unregister(gx_jack::client, gx_jack::output_ports[2]);
                      gx_engine::gNumOutChans--;
                    }

                  if (jack_port_is_mine (gx_jack::client, gx_jack::input_ports[3]))
                    {
                      jack_port_unregister(gx_jack::client, gx_jack::input_ports[3]);
                      gx_engine::gNumInChans--;
                    }

                  if (jack_port_is_mine (gx_jack::client, gx_jack::input_ports[2]))
                    {
                      jack_port_unregister(gx_jack::client, gx_jack::input_ports[2]);
                      gx_engine::gNumInChans--;
                    }

                  usleep(100000);
                  (void)gx_pclose(jconv_stream, JCONV_IDX);
                  gx_engine::gNumInChans = 2;
                  gx_engine::gNumOutChans = 2;
                }
            }
        }

      else if (gx_engine::is_setup == 1)
        {
          // check whether jconv is installed in PATH
          int  jconv_ok = gx_system_call("which", "jconvolver");

          // popup message if something goes funny
          string warning;
          string witch_convolver = "jconvolver";

          // is jconvolver installed ?
          if (jconv_ok != SYSTEM_OK)   // no jconvolver in PATH! :(
            {
              jconv_ok = gx_system_call("which", "jconv");
              witch_convolver = "jconv";
            }
          // is jconv installed ?
          if (jconv_ok != SYSTEM_OK)   // no jconv in PATH! :(
            {
              warning +=
                "  WARNING [JConv]\n\n  "
                "  You need jconv by  Fons Adriaensen "
                "  Please look here\n  "
                "  http://www.kokkinizita.net/linuxaudio/index.html\n";
            }

          // yeps
          else
            {
              if (gx_jack::client == NULL)
                {
                  warning +=
                    "  WARNING [JConv]\n\n  "
                    "  Reconnect to Jack server first (Shift+C)";
                  gx_jconv::GxJConvSettings::checkbutton7 = 0;
                  gx_jconv::jconv_is_running = false;

                  gx_child_process::child_pid[JCONV_IDX] = NO_PID;
                }
              else
                {

                  string cmd(witch_convolver);
                  cmd += " -N jconvjc ";
                  cmd += " " + gx_user_dir + "jconv_";
                  cmd += gx_preset::gx_current_preset.empty() ? "set" :
                         gx_preset::gx_current_preset;
                  cmd += ".conf 2> /dev/null";

                  jconv_stream = gx_popen (cmd.c_str(), "r", JCONV_IDX);
                  sleep (2);

                  // let's fetch the pid of the new jconv process
                  pid_t pid = gx_find_child_pid(witch_convolver.c_str());

                  string check_double(" pidof ");
                  check_double += witch_convolver;
                  check_double += " > /dev/null";

                  // failed ?
                  if (pid == NO_PID)
                    warning +=
                      "  WARNING [JConv]\n\n  "
                      "  Sorry, jconv startup failed ... giving up!";
                  else if (!system(check_double.c_str()) == 0)
                    warning +=
                      "  WARNING [JConv]\n\n  "
                      "  Sorry, jconv startup failed ... giving up!";
                  else
                    {
                      // store pid for future process monitoring
                      child_pid[JCONV_IDX] = pid;

                      // let's (re)open our dedicated ports to jconv
                      if (!gx_jconv::jconv_is_running)
                        {

                          ostringstream buf;

                          // extra Jc_Gui jack ports for jconv
                          for (int i = 2; i < 4; i++)
                            {
                              buf.str("");
                              buf << "out_" << i;

                              gx_jack::output_ports[i] =
                                jack_port_register(gx_jack::client,
                                                   buf.str().c_str(),
                                                   JACK_DEFAULT_AUDIO_TYPE,
                                                   JackPortIsOutput, 0);
                              gx_engine::gNumOutChans++;
                            }
                          for (int i = 2; i < 4; i++)
                            {
                              buf.str("");
                              buf << "in_" << i;

                              gx_jack::input_ports[i] =
                                jack_port_register(gx_jack::client,
                                                   buf.str().c_str(),
                                                   JACK_DEFAULT_AUDIO_TYPE,
                                                   JackPortIsInput, 0);
                              gx_engine::gNumInChans++;

                            }

                          // ---- port connection

                          witch_convolver = "jconvjc";

                          string jc_port = witch_convolver +":In-1";
                          if (jack_port_by_name(gx_jack::client,jc_port.c_str()))
                            {
                              // Jc_Gui outs to jconv ins
                              jack_connect(gx_jack::client, jack_port_name(gx_jack::output_ports[2]), jc_port.c_str());
                              jc_port = witch_convolver +":In-2";
                              jack_connect(gx_jack::client, jack_port_name(gx_jack::output_ports[3]), jc_port.c_str());

                              //  jconv to Jc_Gui monitor
                              jc_port = witch_convolver +":Out-1";
                              jack_connect(gx_jack::client, jc_port.c_str(), jack_port_name(gx_jack::input_ports[2]));
                              jc_port = witch_convolver +":Out-2";
                              jack_connect(gx_jack::client, jc_port.c_str(), jack_port_name(gx_jack::input_ports[3]));
                            }
                          else
                            {
                              (void)gx_gui::gx_message_popup("sorry, connection faild");
                            }

                        }

                      // tell the compute method that JConv is running
                      gx_jconv::jconv_is_running = true;
                      usleep(100000);

                      for (int c = 0; c < 2; c++)
                        gtk_widget_show(gx_gui::GxMainInterface::instance()->
                                        getJCLevelMeters()[c]);

                      // gx_print_info("JConv Start / Stop", string("Started JConv, PID = ") +
                      //               gx_system::gx_i2a(child_pid[JCONV_IDX]));
                    }
                }
            }

          // pop up warning if any
          if (!warning.empty())
            {
              (void)gx_gui::gx_message_popup(warning.c_str());
              gx_jconv::GxJConvSettings::checkbutton7 = 0;
              gx_jconv::jconv_is_running = false;
              gx_child_process::child_pid[JCONV_IDX] = NO_PID;
            }
          gx_jconv::checkbox7 = 0.0;
        }
    }



  } /* end of gx_child_process namespace */
