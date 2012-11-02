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

/* ----- This is the Jc_Gui UI, it belongs to the Jc_Gui namespace ------ */

#pragma once

// --- interface defines
#define stackSize 256
#define kSingleMode 0
#define kBoxMode 1
#define kTabMode 2

namespace gx_ui
{
  /* ------------- UI Classes ------------- */
  /* base interface classes interfacing with the GUI  */
  class GxUI;

  /* --- GxUiItem (virtual class) --- */
  class GxUiItem
  {
  protected :
    GxUI*	fGUI;
    float*	fZone;
    float	fCache;

    GxUiItem (GxUI* ui, float* zone);

  public :
    virtual ~GxUiItem() {}

    void  modifyZone(float v);
    float cache();
    virtual void reflectZone() = 0;
  };


  /* --- Callback Item --- */
  typedef void (*GxUiCallback)(float val, void* data);

  struct GxUiCallbackItem : public GxUiItem
  {
    GxUiCallback fCallback;
    void*	 fData;

    GxUiCallbackItem(GxUI* ui, float* zone, GxUiCallback foo, void* data);
    virtual void reflectZone();
  };

  /* --- Main UI base class --- */
  class GxUI
  {
    typedef list< GxUiItem* > clist;
    typedef map < float*, clist* > zmap;

  private:
    static list<GxUI*>	fGuiList;
    zmap		fZoneMap;
    bool		fStopped;

    virtual void addMenu() {};

    virtual void addbtoggle(const char* label, float* zone){};
    virtual void addstoggle(const char* label, float* zone){};
    virtual void addregler(const char* label, float* zone, float init, float min, float max, float step){};
    virtual void addbigregler(const char* label, float* zone, float init, float min, float max, float step) {};
    virtual void addslider(const char* label, float* zone, float init, float min, float max, float step){};
    virtual void addValueDisplay(const char* label, float* zone, float init, float min, float max, float step){};
    virtual void addStatusDisplay(const char* label, float* zone ) {};

    void addCallback(float* zone, GxUiCallback foo, void* data);

    // -- widget's layouts

    virtual void openFrameBox(const char* label) {};
    virtual void openFrame1Box(const char* label) {};
    virtual void openHorizontalBox(const char* label) {};
    virtual void openVerticalBox(const char* label) {};
    virtual void openVertical1Box(const char* label) {};
    virtual void openVertical2Box(const char* label) {};
    virtual void openWarningBox(const char* label, float* zone){};
    virtual void openEventBox(const char* label) {};
    virtual void openTabBox(const char* label) {};
    virtual void openLevelMeterBox(const char* label)  {};
    virtual void openJackClientBox(const char* label) {};

    virtual void closeBox() {};

  public:
    GxUI();
    virtual ~GxUI() {}

    // public methods
    void registerZone(float*, GxUiItem*);
    void saveStateToFile(const char*);
    void dumpStateToString(string&);
    bool applyStateFromString(const string&);
    void fetchPresetStateFromFile(const char*, const char*, string&);
    bool recallPresetByname(const char*, const char*);
    bool renamePreset(const char*, const char*, const char*);
    void recallState(const char* filename);
    void recalladState(const char* filename, int a, int b, int lin);
    void updateAllZones();
    void updateZone(float* z);
    static void updateAllGuis();

    virtual void setup() {};
    virtual void show() {};
    virtual void run() {};

    void stop()    { fStopped = true; }
    bool stopped() { return fStopped; }

    virtual void declare(float* zone, const char* key, const char* value) {}
  };

} /* end of gx_ui namespace */
