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

/* ------- This is the GUI namespace ------- */

#pragma once

#ifndef NJACKLAT
#define NJACKLAT (9)
#endif

#ifndef NUM_PORT_LISTS
#define NUM_PORT_LISTS (4)
#endif

namespace gx_gui
{
  /* ---------------- the main GUI class ---------------- */
  // Note: this header file depends on gx_engine.h

  class GxMainInterface : public gx_ui::GxUI
  {
  private:
    // private constructor
    GxMainInterface(const char* name, int* pargc, char*** pargv);

    void addMainMenu();

    void addEngineMenu();
    void addJackServerMenu();

    void addPresetMenu();
    void addExtraPresetMenu();

    void addOptionMenu();
    void addGuiSkinMenu();

    void addAboutMenu();

  protected :
    int			fTop;
    GtkWidget*          fBox[stackSize];
    int 		fMode[stackSize];
    bool		fStopped;

    GtkNotebook*        fPortMapTabs;
    GtkWindow*          fPortMapWindow;
    GtkWidget*          fLevelMeters[2];
    GtkWidget*          fJCLevelMeters[2];

    GtkWidget*          fSignalLevelBar;
    GtkWidget*          fJCSignalLevelBar;

    // menu items
    map<string, GtkWidget*> fMenuList;

    // jack menu widgets
    GtkWidget*          fJackConnectItem;
    GtkWidget*          fJackLatencyItem[NJACKLAT];

    GtkWidget* addWidget(const char* label, GtkWidget* w);
    virtual void pushBox(int mode, GtkWidget* w);

  public :
    static bool	 fInitialized;

    static const gboolean expand   = TRUE;
    static const gboolean fill     = TRUE;
    static const gboolean homogene = FALSE;

    static GxMainInterface* instance(const char* name = "",
				     int* pargc = NULL, char*** pargv = NULL);

    // for key acclerators
    GtkAccelGroup* fAccelGroup;

    // list of client portmaps
    set<GtkWidget*> fClientPortMap;

    GtkNotebook* const getPortMapTabs()      const { return fPortMapTabs;     }
    GtkWindow*   const getPortMapWindow()    const { return fPortMapWindow;   }

    GtkWidget*   const getJackConnectItem()  const { return fJackConnectItem; }

    GtkWidget*   const* getLevelMeters()     const { return fLevelMeters;     }
    GtkWidget*   const* getJCLevelMeters()   const { return fJCLevelMeters;   }

    GtkWidget*   const getJackLatencyItem(const jack_nframes_t bufsize) const;

    GtkWidget*   const getMenu(const string name) const { return fMenuList.at(name); }

    // -- update jack client port lists
    void initClientPortMaps      ();

    void addClientPorts          ();
    void addClientPortMap        (const string);

    void deleteClientPorts       ();
    void deleteClientPortMap     (const string);

    void deleteAllClientPortMaps ();

    GtkWidget* getClientPort       (const string, const int);
    GtkWidget* getClientPortTable  (const string, const int);
    GtkWidget* getClientPortMap    (const string);

    // -- create jack portmap window
    void createPortMapWindow(const char* label = "");

    // -- layout groups

    virtual void openFrameBox(const char* label);
    virtual void openFrame1Box(const char* label);
    virtual void openHorizontalBox(const char* label = "");
    virtual void openVerticalBox(const char* label = "");
    virtual void openVertical1Box(const char* label = "");
    virtual void openVertical2Box(const char* label = "");
    virtual void openWarningBox(const char* label, float* zone);
    virtual void openEventBox(const char* label = "");
    virtual void openTabBox(const char* label = "");
    virtual void openLevelMeterBox(const char* label);

    virtual void closeBox();

    // -- active widgets

    virtual void addbtoggle(const char* label, float* zone);
    virtual void addstoggle(const char* label, float* zone);
    virtual void addregler(const char* label, float* zone, float init, float min, float max, float step);
    virtual void addbigregler(const char* label, float* zone, float init, float min, float max, float step);
    virtual void addslider(const char* label, float* zone, float init, float min, float max, float step);
    virtual void addValueDisplay(const char* label, float* zone, float init, float min, float max, float step);
    virtual void addStatusDisplay(const char* label, float* zone );

    virtual void setup();
    virtual void show();
    virtual void run();
  };

  /* -------------------------------------------------------------------------- */
} /* end of gx_gui namespace */

