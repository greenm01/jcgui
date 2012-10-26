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
 *    This is the Jc_Gui GUI main class
 *
 * ----------------------------------------------------------------------------
 */
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

namespace gx_gui
  {
    // -------------------------------------------------------------
    // GxMainInterface method definitions
    //
    // static member
    bool GxMainInterface::fInitialized = false;

    GxMainInterface::GxMainInterface(const char * name, int* pargc, char*** pargv)
    {
      gtk_init(pargc, pargv);
      /*-- Declare the GTK Widgets --*/
      fWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      /* parce a rc string */
      gtk_rc_parse_string ("style \"background\"\n"
                           "{\n"
                           "bg[NORMAL] = \"#000000\"\n"
                           "bg[ACTIVE] = \"#222222\"\n"
                           "bg[PRELIGHT] = \"#111111\"\n"
                           "fg[NORMAL] = \"#a6a9aa\"\n"
                           "fg[ACTIVE] = \"#a6a9aa\"\n"
                           "fg[PRELIGHT] = \"#ffffff\"\n"
                           "text[NORMAL] = \"#a6a9aa\"\n"
                           "text[PRELIGHT] = \"#a6a9aa\"\n"
                           "base[NORMAL] = \"#222222\"\n"
                           "}\n"
                           "widget_class \"*GtkCombo*\" style \"background\""
                           "widget_class \"*GtkMenu*\" style \"background\""
                           "widget_class \"*GtkWidget*\" style \"background\""
                           "widget_class \"*GtkButton*\" style \"background\""
                           "widget_class \"*GtkToggleButton*\" style \"background\""
                           "widget_class \"*GtkWindow*\" style \"background\""
                           "widget_class \"*GtkFileChooserDialog*\" style \"background\""
                           "widget_class \"*GtkDialog*\" style \"background\""
                          );

      /*---------------- set window defaults ----------------*/
      gtk_window_set_resizable(GTK_WINDOW (fWindow) , FALSE);
      gtk_window_set_title (GTK_WINDOW (fWindow), name);
      gtk_window_set_gravity(GTK_WINDOW(fWindow), GDK_GRAVITY_NORTH_EAST);

      /*---------------- singnals ----------------*/
      g_signal_connect (GTK_OBJECT (fWindow), "destroy",
                        G_CALLBACK (gx_clean_exit), NULL);

      /*---------------- status icon ----------------*/
      if (gx_pixmap_check() == 0)
        {
          status_icon =    gtk_status_icon_new_from_pixbuf (GDK_PIXBUF(ib));
          gtk_window_set_icon(GTK_WINDOW (fWindow), GDK_PIXBUF(ib));
          g_signal_connect (G_OBJECT (status_icon), "activate", G_CALLBACK (gx_hide_extended_settings), NULL);
          g_signal_connect (G_OBJECT (status_icon), "popup-menu", G_CALLBACK (gx_systray_menu), NULL);
        }
      else
        {
          gx_print_error("Main Interface Constructor",
                         "pixmap check failed, giving up");
          gx_clean_exit(NULL, (gpointer)1);
        }

      /*-- create accelerator group for keyboard shortcuts --*/
      fAccelGroup = gtk_accel_group_new();
      gtk_window_add_accel_group(GTK_WINDOW(fWindow), fAccelGroup);

      /*---------------- create boxes ----------------*/
      fTop = 0;
      fBox[fTop] = gtk_vbox_new (homogene, 4);
      fMode[fTop] = kBoxMode;

      /*---------------- add mainbox to main window ---------------*/
      gtk_container_add (GTK_CONTAINER (fWindow), fBox[fTop]);


      fStopped = false;
    }

    //------- create or retrieve unique instance
    GxMainInterface* GxMainInterface::instance(const char* name, int* pargc, char*** pargv)
    {
      static GxMainInterface maingui(name, pargc, pargv);
      return &maingui;
    }


    //------- retrieve jack latency menu item
    GtkWidget* const
    GxMainInterface::getJackLatencyItem(const jack_nframes_t bufsize) const
      {
        int index = (int)(log((float)bufsize)/log(2)) - 5;

        if (index >= 0 && index < NJACKLAT)
          return fJackLatencyItem[index];

        return NULL;
      }

    //------- box stacking up
    void GxMainInterface::pushBox(int mode, GtkWidget* w)
    {
      assert(++fTop < stackSize);
      fMode[fTop] 	= mode;
      fBox[fTop] 		= w;
    }

    void GxMainInterface::closeBox()
    {
      assert(--fTop >= 0);
    }

    //-------- different box styles
    void GxMainInterface::openFrameBox(const char* label)
    {
      GtkWidget * box = gtk_hbox_new (homogene, 2);
      gtk_container_set_border_width (GTK_CONTAINER (box), 2);
      g_signal_connect(box, "expose-event", G_CALLBACK(box10_expose), NULL);
      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget * frame = addWidget(label, gtk_frame_new (label));
          gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
          gtk_container_add (GTK_CONTAINER(frame), box);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openFrame1Box(const char* label)
    {
      GtkWidget * box = gtk_hbox_new (homogene, 2);
      gtk_container_set_border_width (GTK_CONTAINER (box), 2);
      g_signal_connect(box, "expose-event", G_CALLBACK(box11_expose), NULL);
      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget * frame = addWidget(label, gtk_frame_new (label));
          gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
          gtk_container_add (GTK_CONTAINER(frame), box);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openTabBox(const char* label)
    {
      pushBox(kTabMode, addWidget(label, gtk_notebook_new ()));
    }

    void GxMainInterface::openLevelMeterBox(const char* label)
    {
      GtkWidget* box = addWidget(label, gtk_hbox_new (FALSE, 0));

      gint boxheight = 135;
      gint boxwidth  = 40;

      gtk_container_set_border_width (GTK_CONTAINER (box), 3);
      gtk_box_set_spacing(GTK_BOX(box), 1);

      gtk_widget_set_size_request (GTK_WIDGET(box), boxwidth, boxheight);
      g_signal_connect(box, "expose-event", G_CALLBACK(box7_expose), NULL);
      g_signal_connect(GTK_CONTAINER(box), "check-resize",
                       G_CALLBACK(box7_expose), NULL);

      // meter level colors
      int base = 0x00380800;
      int mid  = 0x00ff0000;
      int top  = 0xff000000;
      int clip = 0xff000000;

      // width of meter
      int width    = 4;
      // how long we hold the peak bar = hold * thread call timeout

      // Note: 30 * 80 = 2.4 sec
      int hold     = 20;

      // Jc_Gui output levels
      GtkWidget* gxbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (gxbox), 0);
      gtk_box_set_spacing(GTK_BOX(gxbox), 1);

      for (int i = 0; i < 2; i++)
        {
          fLevelMeters[i] = 0;

          GtkWidget* meter =
            gtk_fast_meter_new(hold, width, boxheight,
                               base, mid, top, clip);

          gtk_widget_add_events(meter, GDK_BUTTON_RELEASE_MASK);
          g_signal_connect(G_OBJECT(meter), "button-release-event",
                           G_CALLBACK(gx_meter_button_release), 0);

          gtk_box_pack_start(GTK_BOX(gxbox), meter, FALSE, TRUE, 0);
          gtk_widget_show(meter);

          GtkTooltips* tooltips = gtk_tooltips_new ();
          gtk_tooltips_set_tip(tooltips, meter, "Jc_Gui output", " ");
          fLevelMeters[i] = meter;
        }

      gtk_box_pack_start(GTK_BOX(box), gxbox, FALSE, TRUE, 0);
      gtk_widget_show(gxbox);

      // jconv output levels
      GtkWidget* jcbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (jcbox), 1);
      gtk_box_set_spacing(GTK_BOX(jcbox), 1);

      for (int i = 0; i < 2; i++)
        {
          fJCLevelMeters[i] = 0;

          GtkWidget* meter =
            gtk_fast_meter_new(hold, width, boxheight,
                               base, mid, top, clip);

          gtk_widget_add_events(meter, GDK_BUTTON_RELEASE_MASK);
          g_signal_connect(G_OBJECT(meter), "button-release-event",
                           G_CALLBACK(gx_meter_button_release), 0);

          gtk_box_pack_end(GTK_BOX(box), meter, FALSE, FALSE, 0);

          GtkTooltips* tooltips = gtk_tooltips_new ();
          gtk_tooltips_set_tip(tooltips, meter, "jconv output", " ");

          gtk_widget_hide(meter);
          fJCLevelMeters[i] = meter;
        }

      gtk_box_pack_end(GTK_BOX(box), jcbox, FALSE, TRUE, 0);
      gtk_widget_show(jcbox);

      // show main box
      gtk_widget_show(box);
    }

    /* --- create the portmap window with tabbed client port tables --- */
    void GxMainInterface::createPortMapWindow(const char* label)
    {
      // static box containing all
      GtkWidget* vbox = gtk_vbox_new(FALSE, 2);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      g_signal_connect(vbox, "expose-event", G_CALLBACK(box4_expose), NULL);
      gtk_widget_show(vbox);

      // static hbox containing Jc_Gui port names
      GtkWidget* hbox = gtk_hbox_new(FALSE, 2);
      for (int i = gx_jack::kAudioInput1; i <= gx_jack::kAudioOutput2; i++)
        {
          string pname =
            gx_jack::client_name + string(" : ") +
            gx_jack::gx_port_names[i];

          GtkWidget* label = gtk_label_new(pname.c_str());
          GdkColor colorGreen;
          gdk_color_parse("#a6a9aa", &colorGreen);
          gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &colorGreen);
          gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 2);
          gtk_widget_show(label);
        }


      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(vbox), hbox,   FALSE, FALSE, 2);

      // add seperator
      GtkWidget* sep = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

      gtk_widget_show(sep);

      // notebook
      GtkWidget* nb = gtk_notebook_new();
      gtk_notebook_set_scrollable(GTK_NOTEBOOK(nb), TRUE);
      g_signal_connect(nb, "expose-event", G_CALLBACK(box4_expose), NULL);
      fPortMapTabs = GTK_NOTEBOOK(nb);

      // scrolled window
      GtkWidget* scrlwd = gtk_scrolled_window_new(NULL, NULL);
      g_signal_connect(scrlwd, "expose-event", G_CALLBACK(box4_expose), NULL);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrlwd),
                                     GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
      gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrlwd),
                                          GTK_SHADOW_IN);

      gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrlwd), nb);
      gtk_widget_show(nb);
      gtk_widget_show(scrlwd);

      // add scrolled window in vbox
      gtk_box_pack_start(GTK_BOX(vbox), scrlwd, TRUE, TRUE, 2);

      // main window
      GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(GTK_WINDOW (window), label);
      gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(fWindow));
      gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
      gtk_window_add_accel_group(GTK_WINDOW(window), fAccelGroup);
      gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_NORTH_WEST);


      gtk_container_add(GTK_CONTAINER(window), vbox);
      gtk_widget_hide(window);

      fPortMapWindow = GTK_WINDOW(window);
    }

    void GxMainInterface::openHorizontalBox(const char* label)
    {
      GtkWidget * box = gtk_hbox_new (homogene, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box), 4);

      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget * frame = addWidget(label, gtk_frame_new (label));
          gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
          gtk_container_add (GTK_CONTAINER(frame), box);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openEventBox(const char* label)
    {
      GtkWidget * box = gtk_hbox_new (homogene, 4);
      gtk_container_set_border_width (GTK_CONTAINER (box), 2);
      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget * frame = addWidget(label, gtk_event_box_new ());
          gtk_container_add (GTK_CONTAINER(frame), box);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openVerticalBox(const char* label)
    {
      GtkWidget * box = gtk_vbox_new (homogene, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box), 4);
      g_signal_connect(box, "expose-event", G_CALLBACK(box10_expose), NULL);

      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget* lw = gtk_label_new(label);
          GdkColor colorGreen;
          gdk_color_parse("#a6a9aa", &colorGreen);
          gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
          GtkStyle *style = gtk_widget_get_style(lw);
          pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
          pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
          gtk_widget_modify_font(lw, style->font_desc);

          gtk_container_add (GTK_CONTAINER(box), lw);
          gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
          gtk_widget_show(lw);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openVertical1Box(const char* label)
    {
      GtkWidget * box = gtk_vbox_new (homogene, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box), 4);
      g_signal_connect(box, "expose-event", G_CALLBACK(box4_expose), NULL);

      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget* lw = gtk_label_new(label);
          GdkColor colorGreen;
          gdk_color_parse("#a6a9aa", &colorGreen);
          gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
          GtkStyle *style = gtk_widget_get_style(lw);
          pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
          pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
          gtk_widget_modify_font(lw, style->font_desc);

          gtk_container_add (GTK_CONTAINER(box), lw);
          gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
          gtk_widget_show(lw);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    void GxMainInterface::openVertical2Box(const char* label)
    {
      GtkWidget * box = gtk_vbox_new (homogene, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box), 4);


      if (fMode[fTop] != kTabMode && label[0] != 0)
        {
          GtkWidget* lw = gtk_label_new(label);
          GdkColor colorGreen;
          gdk_color_parse("#a6a9aa", &colorGreen);
          gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
          GtkStyle *style = gtk_widget_get_style(lw);
          pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
          pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
          gtk_widget_modify_font(lw, style->font_desc);

          gtk_container_add (GTK_CONTAINER(box), lw);
          gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
          gtk_widget_show(lw);
          gtk_widget_show(box);
          pushBox(kBoxMode, box);
        }
      else
        {
          pushBox(kBoxMode, addWidget(label, box));
        }
    }

    GtkWidget* GxMainInterface::addWidget(const char* label, GtkWidget* w)
    {
      switch (fMode[fTop])
        {
        case kSingleMode	:
          gtk_container_add (GTK_CONTAINER(fBox[fTop]), w);
          break;
        case kBoxMode 		:
          gtk_box_pack_start (GTK_BOX(fBox[fTop]), w, expand, fill, 0);
          break;
        case kTabMode 		:
          gtk_notebook_append_page (GTK_NOTEBOOK(fBox[fTop]), w, gtk_label_new(label));
          break;
        }
      gtk_widget_show (w);
      return w;
    }

    // ---------------------------	Check Button ---------------------------

    struct uiCheckButton : public gx_ui::GxUiItem
      {
        GtkToggleButton* fButton;
        uiCheckButton(gx_ui::GxUI* ui, float* zone, GtkToggleButton* b) : gx_ui::GxUiItem(ui, zone), fButton(b) {}
        static void toggled (GtkWidget *widget, gpointer data)
        {
          float	v = (GTK_TOGGLE_BUTTON (widget)->active) ? 1.0 : 0.0;
          ((gx_ui::GxUiItem*)data)->modifyZone(v);
        }

        virtual void reflectZone()
        {
          float 	v = *fZone;
          fCache = v;
          gtk_toggle_button_set_active(fButton, v > 0.0);
        }
      };

    // ---------------------------	Adjustmenty based widgets ---------------------------

    struct uiAdjustment : public gx_ui::GxUiItem
      {
        GtkAdjustment* fAdj;
        uiAdjustment(gx_ui::GxUI* ui, float* zone, GtkAdjustment* adj) : gx_ui::GxUiItem(ui, zone), fAdj(adj) {}
        static void changed (GtkWidget *widget, gpointer data)
        {
          float	v = GTK_ADJUSTMENT (widget)->value;
          ((gx_ui::GxUiItem*)data)->modifyZone(v);
        }

        virtual void reflectZone()
        {
          float 	v = *fZone;
          fCache = v;
          gtk_adjustment_set_value(fAdj, v);
        }
      };

    int precision(double n)
    {
      if (n < 0.009999) return 3;
      else if (n < 0.099999) return 2;
      else if (n < 0.999999) return 1;
      else return 0;
    }

    struct uiValueDisplay : public gx_ui::GxUiItem
      {
        GtkLabel* fLabel;
        int	fPrecision ;

        uiValueDisplay(gx_ui::GxUI* ui, float* zone, GtkLabel* label, int precision)
            : gx_ui::GxUiItem(ui, zone), fLabel(label), fPrecision(precision) {}

        virtual void reflectZone()
        {
          float v = *fZone;
          fCache = v;
          char s[64];
          if (fPrecision <= 0)
            snprintf(s, 63, "%d", int(v));

          else if (fPrecision > 3)
            snprintf(s, 63, "%f", v);

          else if (fPrecision == 1)
            {
              const char* format[] = {"%.1f", "%.2f", "%.3f"};
              snprintf(s, 63, format[1-1], v);
            }
          else if (fPrecision == 2)
            {
              const char* format[] = {"%.1f", "%.2f", "%.3f"};
              snprintf(s, 63, format[2-1], v);
            }
          else
            {
              const char* format[] = {"%.1f", "%.2f", "%.3f"};
              snprintf(s, 63, format[3-1], v);
            }
          gtk_label_set_text(fLabel, s);
        }
      };

    void GxMainInterface::addValueDisplay(const char* label, float* zone, float init, float min, float max, float step)
    {
      *zone = init;
      GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkWidget* lw = gtk_label_new("");

      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
      GtkStyle *style = gtk_widget_get_style(lw);
      pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
      gtk_widget_modify_font(lw, style->font_desc);

      new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
      openVerticalBox("");
      addWidget(label, lw);
      closeBox();
    }

    void GxMainInterface::addbtoggle(const char* label, float* zone)
    {
      GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkRegler myGtkRegler;
      GtkWidget* button = myGtkRegler.gtk_button_toggle_new_with_adjustment(label, GTK_ADJUSTMENT(adj));
      addWidget(label, button);

      g_signal_connect (GTK_OBJECT (button), "value-changed",
                        G_CALLBACK (gx_child_process::gx_start_stop_jconv), (gpointer)c);
    }

    void GxMainInterface::addstoggle(const char* label, float* zone)
    {
      GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkRegler myGtkRegler;
      GtkWidget* button = myGtkRegler.gtk_button_new_with_adjustment(label, GTK_ADJUSTMENT(adj));
      addWidget(label, button);

      g_signal_connect (GTK_OBJECT (button), "value-changed",
                        G_CALLBACK (gx_jconv::gx_show_jconv_dialog_gui), (gpointer)c);
    }


    void GxMainInterface::addregler(const char* label, float* zone, float init, float min, float max, float step)
    {
      *zone = init;
      GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkWidget* lw = gtk_label_new("");
      GtkWidget* lwl = gtk_label_new(label);
      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
      gtk_widget_modify_fg (lwl, GTK_STATE_NORMAL, &colorGreen);
      GtkStyle *style = gtk_widget_get_style(lw);
      pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
      gtk_widget_modify_font(lw, style->font_desc);
      gtk_widget_modify_font(lwl, style->font_desc);
      new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
      GtkRegler myGtkRegler;
      GtkWidget* slider = myGtkRegler.gtk_regler_new_with_adjustment(GTK_ADJUSTMENT(adj));
      gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
      openVerticalBox("");
      addWidget(label, lwl);
      addWidget(label, slider);
      addWidget(label, lw);
      closeBox();
    }

    void GxMainInterface::addbigregler(const char* label, float* zone, float init, float min, float max, float step)
    {
      *zone = init;
      GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkWidget* lw = gtk_label_new("");
      GtkWidget* lwl = gtk_label_new(label);
      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
      gtk_widget_modify_fg (lwl, GTK_STATE_NORMAL, &colorGreen);
      GtkStyle *style = gtk_widget_get_style(lw);
      pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
      gtk_widget_modify_font(lw, style->font_desc);
      gtk_widget_modify_font(lwl, style->font_desc);

      new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
      GtkRegler myGtkRegler;
      GtkWidget* slider = myGtkRegler.gtk_big_regler_new_with_adjustment(GTK_ADJUSTMENT(adj));
      gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
      openVerticalBox("");
      addWidget(label, lwl);
      addWidget(label, slider);
      addWidget(label, lw);
      closeBox();
    }

    void GxMainInterface::addslider(const char* label, float* zone, float init, float min, float max, float step)
    {
      *zone = init;
      GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
      uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
      g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
      GtkWidget* lw = gtk_label_new("");
      GdkColor colorGreen;
      gdk_color_parse("#a6a9aa", &colorGreen);
      gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
      GtkStyle *style = gtk_widget_get_style(lw);
      pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
      pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
      gtk_widget_modify_font(lw, style->font_desc);
      new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
      GtkRegler myGtkRegler;
      GtkWidget* slider = myGtkRegler.gtk_hslider_new_with_adjustment(GTK_ADJUSTMENT(adj));
      gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
      openVertical2Box(label);
      addWidget(label, slider);
      addWidget(label, lw);

      closeBox();
    }

    void GxMainInterface::openWarningBox(const char* label, float* zone)
    {
      GtkWidget* 	button = gtk_check_button_new ();
      uiCheckButton* c = new uiCheckButton(this, zone, GTK_TOGGLE_BUTTON(button));
      g_signal_connect (GTK_OBJECT (button), "toggled", G_CALLBACK(uiCheckButton::toggled), (gpointer) c);
    }

    struct uiStatusDisplay : public gx_ui::GxUiItem
      {
        GtkLabel* fLabel;
        int	fPrecision;

        uiStatusDisplay(gx_ui::GxUI* ui, float* zone, GtkLabel* label)
            : gx_ui::GxUiItem(ui, zone), fLabel(label) {}

        virtual void reflectZone()
        {
          float 	v = *fZone;
          fCache = v;

        }
      };

    void GxMainInterface::addStatusDisplay(const char* label, float* zone )
    {
      GtkWidget* lw = gtk_label_new("");
      new uiStatusDisplay(this, zone, GTK_LABEL(lw));
      openFrameBox(label);
      addWidget(label, lw);
      closeBox();
      gtk_widget_hide(lw);
    };

    //----------------------------- main menu ----------------------------
    void GxMainInterface::addMainMenu()
    {
      /*-- Declare the GTK Widgets used in the menu --*/
      GtkWidget* menucont;  // menu container
      GtkWidget* menupix;  // menu container
      GtkWidget* hbox;      // top menu bar box container

      /*------------------ TOP Menu BAR ------------------*/
      hbox = gtk_hbox_new(FALSE, 0);

      /*-- Create the menu bar --*/
      menucont = gtk_menu_bar_new();
      gtk_box_pack_start(GTK_BOX(hbox), menucont, TRUE, TRUE, 0);

      /*-- Create the pixmap menu bar --*/
      menupix = gtk_menu_bar_new();
      gtk_box_pack_end(GTK_BOX(hbox), menupix, TRUE, TRUE, 0);

      /*-- set packdirection for pixmaps from right to left --*/
      gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menupix),GTK_PACK_DIRECTION_RTL);

      /*-- Engine on/off and status --*/
      // set up ON image: shown by default
      string img_path = gx_pixmap_dir + "gxjc_on.png";

      gx_engine_on_image =  gtk_image_menu_item_new_with_label("");
      GtkWidget* engineon = gtk_image_new_from_file(img_path.c_str());
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(gx_engine_on_image),engineon);
      gtk_menu_bar_append (GTK_MENU_BAR(menupix), gx_engine_on_image);
      GtkTooltips* comandlin = gtk_tooltips_new ();

      gtk_tooltips_set_tip(GTK_TOOLTIPS (comandlin),
                           gx_engine_on_image, "engine is on", "engine state.");
      gtk_widget_show(gx_engine_on_image);

      // set up OFF image: hidden by default
      img_path = gx_pixmap_dir + "gxjc_off.png";

      gx_engine_off_image =  gtk_image_menu_item_new_with_label("");
      GtkWidget* engineoff = gtk_image_new_from_file(img_path.c_str());
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(gx_engine_off_image),engineoff);
      gtk_menu_bar_append (GTK_MENU_BAR(menupix), gx_engine_off_image);
      gtk_tooltips_set_tip(GTK_TOOLTIPS (comandlin),
                           gx_engine_off_image, "engine is off", "engine state.");
      gtk_widget_hide(gx_engine_off_image);

      /*-- Jack server status image --*/
      // jackd ON image
      img_path = gx_pixmap_dir + "jc_on.png";

      gx_jackd_on_image =  gtk_image_menu_item_new_with_label("");
      GtkWidget*   jackstateon = gtk_image_new_from_file(img_path.c_str());
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(gx_jackd_on_image),jackstateon);
      gtk_menu_bar_append (GTK_MENU_BAR(menupix), gx_jackd_on_image);

      GtkTooltips* comandline = gtk_tooltips_new ();

      gtk_tooltips_set_tip(GTK_TOOLTIPS (comandline),
                           gx_jackd_on_image, "jack server is connectet", "jack server state.");

      gtk_widget_show(gx_jackd_on_image);

      // jackd OFF image: hidden by default
      img_path = gx_pixmap_dir + "jc_off.png";

      gx_jackd_off_image =  gtk_image_menu_item_new_with_label("");
      GtkWidget*   jackstateoff = gtk_image_new_from_file(img_path.c_str());
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(gx_jackd_off_image),jackstateoff);
      gtk_menu_bar_append (GTK_MENU_BAR(menupix), gx_jackd_off_image);
      gtk_tooltips_set_tip(GTK_TOOLTIPS (comandline),
                           gx_jackd_off_image, "jack server is unconnectet", "jack server state.");
      gtk_widget_hide(gx_jackd_off_image);

      /* ----------------------------------------------------------- */
      fMenuList["Top"] = menucont;

      addEngineMenu();
      addPresetMenu();
      addAboutMenu();

      /*---------------- add menu to main window box----------------*/
      gtk_box_pack_start (GTK_BOX (fBox[fTop]), hbox , FALSE, FALSE, 0);
      gtk_widget_show(menucont);
      gtk_widget_show(menupix);
      gtk_widget_show(hbox);
    }

    //----------------------------- engine menu ----------------------------
    void GxMainInterface::addEngineMenu()
    {
      GtkWidget* menulabel;   // menu label
      GtkWidget* menuitem;    // menu item
      //GSList   * group = NULL;

      /*---------------- Create Engine menu items ------------------*/
      menuh = fMenuList["Top"];

      menulabel = gtk_menu_item_new_with_mnemonic ("_Engine");
      gtk_menu_bar_append (GTK_MENU_BAR(menuh), menulabel);
      gtk_widget_show(menulabel);

      /*-- Create Engine submenu  --*/
      menuh = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menuh);
      gtk_widget_show(menuh);
      fMenuList["Engine"] = menuh;

      /*-- Create Engine start / stop item  --*/
      //group = NULL;

      menuitem = gtk_check_menu_item_new_with_mnemonic ("Engine _Start / _Stop");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_space, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);

      gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), TRUE);
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_engine_switch), (gpointer)0);
      gx_engine_item = menuitem; // save into global var
      gtk_widget_show (menuitem);

      /*-- add a separator line --*/
      GtkWidget* sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
      gtk_widget_show (sep);

      /*---------------- Create Jack Server menu --------------------*/
      addJackServerMenu();

      /*---------------- End Jack server menu declarations ----------------*/

      /*-- add a separator line --*/
      sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
      gtk_widget_show (sep);

      /*-- add a separator line --*/
      sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
      gtk_widget_show (sep);

      /*-- Create Exit menu item under Engine submenu --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("_Quit");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
      g_signal_connect(G_OBJECT (menuitem), "activate",
                       G_CALLBACK (gx_clean_exit), NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
      gtk_widget_show (menuitem);

      /*---------------- End Engine menu declarations ----------------*/
    }

    //----------------------------- preset menu ----------------------------
    void GxMainInterface::addPresetMenu()
    {
      GtkWidget* menulabel; // menu label
      GtkWidget* menucont;  // menu container
      GtkWidget* menuitem;  // menu item

      menucont = fMenuList["Top"];

      /*---------------- Create Presets menu items --------------------*/
      menulabel = gtk_menu_item_new_with_mnemonic ("_Presets");
      gtk_menu_bar_append (GTK_MENU_BAR(menucont), menulabel);
      gtk_widget_show(menulabel);

      /*-- Create Presets submenus --*/
      menucont = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
      gtk_widget_show(menucont);
      fMenuList["Presets"] = menucont;

      /* special treatment of preset lists, from gx_preset namespace */
      for (int i = 0; i < GX_NUM_OF_PRESET_LISTS; i++)
        {
          GtkWidget* menuItem =
            gtk_menu_item_new_with_mnemonic (preset_menu_name[i]);
          gtk_menu_shell_append (GTK_MENU_SHELL(menucont), menuItem);

          GtkWidget* menu = gtk_menu_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), menu);

          gtk_menu_set_accel_path(GTK_MENU(menu), preset_accel_path[i]);

          presmenu[i] = menu;
          presMenu[i] = menuItem;
        }

      /*-- add New Preset saving under Save Presets menu */
      menuitem = gtk_menu_item_new_with_mnemonic ("New _Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_save_newpreset_dialog), NULL);
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_p, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_insert(GTK_MENU_SHELL(presmenu[SAVE_PRESET_LIST]), menuitem, 0);
      gtk_widget_show (menuitem);

      /*-- add a separator line --*/
      GtkWidget* sep = gtk_separator_menu_item_new();
      gtk_menu_shell_insert(GTK_MENU_SHELL(presmenu[SAVE_PRESET_LIST]), sep, 1);
      gtk_widget_show (sep);

      /*-- initial preset list --*/
      gx_preset::gx_build_preset_list();

      vector<string>::iterator it;
      for (it = gx_preset::plist.begin() ; it < gx_preset::plist.end(); it++ )
        {
          const string presname = *it;
          gx_add_preset_to_menus(presname);
        }

      for (int i = 0; i < GX_NUM_OF_PRESET_LISTS; i++)
        gtk_widget_show(presMenu[i]);

      /* ------------------- */

      /*-- add a separator line --*/
      sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
      gtk_widget_show (sep);

      /*-- Create  Main setting submenu --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("Recall Main _Setting");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_recall_main_setting), NULL);
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_s, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic ("_Save As Main _Setting");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_save_main_setting), NULL);
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      /*-- add a separator line --*/
      sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
      gtk_widget_show (sep);

      /*-- Create sub menu More Preset Action --*/
      menulabel = gtk_menu_item_new_with_mnemonic("More Preset Options...");
      gtk_menu_shell_append (GTK_MENU_SHELL(menucont), menulabel);
      gtk_widget_show(menulabel);

      menucont = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
      gtk_widget_show(menucont);
      fMenuList["ExtraPresets"] = menucont;

      /*--------------- Extra preset menu */
      addExtraPresetMenu();
    }

    //------------------------ extra preset menu ----------------------------
    void GxMainInterface::addExtraPresetMenu()
    {
      GtkWidget* menucont;  // menu container
      GtkWidget* menuitem;  // menu item

      menucont = fMenuList["ExtraPresets"];

      /*---------------- Create Presets menu items --------------------*/

      /* forward preset */
      menuitem = gtk_menu_item_new_with_mnemonic("Next _Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_next_preset), NULL);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_Page_Down,
                                 GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show(menuitem);

      /* rewind preset */
      menuitem = gtk_menu_item_new_with_mnemonic("Previous _Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_previous_preset), NULL);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_Page_Up,
                                 GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show(menuitem);

      /*-- add a separator line --*/
      GtkWidget* sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
      gtk_widget_show (sep);

      /*-- Create  menu item Delete Active preset --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("_Save _Active Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_save_oldpreset), (gpointer)1);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_s,
                                 GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic ("_Rename _Active Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_rename_active_preset_dialog), NULL);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_r,
                                 GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_mnemonic ("_Delete Active Preset");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_delete_active_preset_dialog), NULL);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_Delete,
                                 GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      /*-- add a separator line --*/
      sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
      gtk_widget_show (sep);

      /*-- Create  menu item Delete All presets --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("_Delete All Presets");
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_delete_all_presets_dialog), NULL);
      gtk_widget_add_accelerator(menuitem, "activate",
                                 fAccelGroup, GDK_d,
                                 GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK), GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

    }

    //----------------------------- about menu ----------------------------
    void GxMainInterface::addAboutMenu()
    {
      GtkWidget* menulabel; // menu label
      GtkWidget* menucont;  // menu container
      GtkWidget* menuitem;  // menu item

      menucont = fMenuList["Top"];

      /*---------------- Start About menu declarations ----------------*/
      menulabel = gtk_menu_item_new_with_mnemonic ("_About");
      gtk_menu_bar_append (GTK_MENU_BAR(menucont), menulabel);
      gtk_widget_show(menulabel);

      /*-- Create About submenu --*/
      menucont = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);

      /*-- Create About menu item under About submenu --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("_About");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_a, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      g_signal_connect(GTK_OBJECT (menuitem), "activate",
                       G_CALLBACK (gx_show_about), NULL);
      gtk_widget_show (menuitem);

      /*-- Create Help menu item under About submenu --*/
      menuitem = gtk_menu_item_new_with_mnemonic ("_Help");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_h, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      //    g_signal_connect(GTK_OBJECT (menuitem), "activate", G_CALLBACK (gx_show_about), NULL);
      gtk_widget_show (menuitem);

      /*---------------- End About menu declarations ----------------*/
    }

    /*---------------- Jack Server Menu ----------------*/
    void GxMainInterface::addJackServerMenu()
    {
      GtkWidget* menulabel; // menu label
      GtkWidget* menucont;  // menu container
      GtkWidget* menuitem;  // menu item
      GSList   * group = NULL;

      menucont = fMenuList["Engine"];

      /*-- Create Jack Connection toggle button --*/
      menuitem = gtk_check_menu_item_new_with_mnemonic ("Jack Server _Connection ");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_c, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_jack::gx_jack_connection), NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);

      gtk_widget_show (menuitem);
      fJackConnectItem = menuitem;

      /*-- create Jack Ports menu item --*/
      menuitem = gtk_check_menu_item_new_with_mnemonic ("Jack _Ports ");
      gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
                                 GDK_p, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
                        G_CALLBACK (gx_show_portmap_window), NULL);

      g_signal_connect_swapped(G_OBJECT(fPortMapWindow), "delete_event",
                               G_CALLBACK(gx_hide_portmap_window), menuitem);
      g_signal_connect(G_OBJECT(fPortMapWindow),"delete-event",
                               G_CALLBACK(gtk_widget_hide_on_delete),NULL);


      gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
      gtk_widget_show (menuitem);

      menucont = fMenuList["Engine"];

      /*-- Create  Latency submenu under Jack Server submenu --*/
      menulabel = gtk_menu_item_new_with_mnemonic ("_Latency");
      gtk_menu_append (GTK_MENU(menucont), menulabel);
      gtk_widget_show(menulabel);

      menucont = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);

      /*-- Create  menu item under Latency submenu --*/
      gchar buf_size[8];
      const int min_pow = 5;  // 2**5  = 32
      const int max_pow = 13; // 2**13 = 8192
      group = NULL;

      for (int i = min_pow; i <= max_pow; i++)
        {
          int jack_buffer_size = (int)pow(2.,i);
          (void)snprintf(buf_size, 5, "%d", jack_buffer_size);
          menuitem = gtk_radio_menu_item_new_with_label (group, buf_size);
          group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), FALSE);

          g_signal_connect (GTK_OBJECT (menuitem), "activate",
                            G_CALLBACK (gx_jack::gx_set_jack_buffer_size),
                            GINT_TO_POINTER(jack_buffer_size));

          // display actual buffer size as default
          gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
          gtk_widget_show (menuitem);

          fJackLatencyItem[i-min_pow] = menuitem;
        }
    }

    /* -------- init jack client menus ---------- */
    void GxMainInterface::initClientPortMaps()
    {
      // make sure everything is reset
      deleteAllClientPortMaps();

      gx_client_port_dequeue.clear();
      gx_client_port_queue.clear();

      // if jack down, no bother
      // (should not be called when jack is down anyway)
      if (!gx_jack::client)
        return;

      // get all existing port names (no MIDI stuff for now)
      const char** iportnames =
        jack_get_ports(gx_jack::client, NULL,
                       JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);

      const char** oportnames =
        jack_get_ports(gx_jack::client, NULL,
                       JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);

      // populating output port menus
      int p = 0;
      while (oportnames[p] != 0)
        {
          string pname = oportnames[p];
          if (pname.substr(0, pname.find(":")) != gx_jack::client_name)
            gx_client_port_queue.insert(pair<string, int>(pname, JackPortIsOutput));

          p++;
        }

      // populating input port menus
      p = 0;
      while (iportnames[p] != 0)
        {
          string pname = iportnames[p];
          if (pname.substr(0, pname.find(":")) != gx_jack::client_name)
            gx_client_port_queue.insert(pair<string, int>(pname, JackPortIsInput));
          p++;
        }

      // free port name lists (cf. JACK API doc)
      free(iportnames);
      free(oportnames);
    }

    /* -------- add  jack client item ---------- */
    void GxMainInterface::addClientPortMap(const string clname)
    {
      // no need to bother if are not a jack client
      if (gx_jack::client == NULL)
        {
          gx_print_warning("Jack Client", "Connect back to jack first");
          return;
        }

      // we don't want these guys here :)
      if (clname == gx_jack::client_name ||
          clname == "probe"              ||
          clname == "ardourprobe"        ||
          clname == "freewheel"          ||
          clname == "qjackctl"           ||
          clname == "Patchage")
        return;

      // add tab in client notebook if needed
      // Note: one-to-one mapping: only ONE tab per client
      if (getClientPortMap(clname))
        return;

      GtkWidget* label   = gtk_label_new(clname.c_str());

      GtkWidget* mapbox  = gtk_hbox_new(TRUE, 10);
      gtk_widget_set_name(mapbox, clname.c_str());

      for (int t = gx_jack::kAudioInput1; t <= gx_jack::kAudioOutput2; t++)
        {
          GtkWidget* table  = gtk_vbox_new(FALSE, 0);
          gtk_box_pack_start(GTK_BOX(mapbox), table, TRUE, FALSE, 0);

          g_signal_connect(table, "expose-event", G_CALLBACK(box6_expose), NULL);
          gtk_widget_show(table);
        }

      gtk_notebook_append_page(fPortMapTabs, mapbox, label);
      g_signal_connect(mapbox, "expose-event", G_CALLBACK(box4_expose), NULL);
      gtk_widget_show(label);
      gtk_widget_show(mapbox);
      fClientPortMap.insert(mapbox);

      // move focus back to Jc_Gui main window
      gtk_widget_grab_focus(fWindow);

    }

    /* -------- add port to a given jack client portmap  ---------- */
    void GxMainInterface::addClientPorts()
    {

      // no need to bother if are not a jack client
      if (gx_jack::client == NULL)
        {
          gx_print_warning("Jack Client Port Add",
                           "we are not yet a jack client");
          gx_client_port_queue.clear();
          return;
        }

      // go through list
      multimap<string, int>::iterator pn;
      for (pn  = gx_client_port_queue.begin();
           pn != gx_client_port_queue.end();
           pn++)
        {
          string port_name = pn->first;

          // retrieve the client name from the port name
          string client_name = port_name.substr(0, port_name.find(':'));
          string short_name  = port_name.substr(port_name.find(':')+1);

          // if client portmap does not exist, create it
          if (!getClientPortMap(client_name))
            addClientPortMap(client_name);

          if (!getClientPortMap(client_name))
            continue;

          // port flags
          int flags          = pn->second;

          // set up how many port tables we should deal with:
          // 2 for Jc_Gui input (stereo)
          // 2 for Jc_Gui outputs (stereo)

          int table_index = gx_jack::kAudioInput1, ntables = 2;
          if ((flags & JackPortIsOutput) == 0)
            {
              table_index = gx_jack::kAudioOutput1;
              ntables = 2;
            }

          // add port item
          for (int i = table_index; i < table_index + ntables; i++)
            {
              // retrieve port table
              GtkVBox* portbox =
                GTK_VBOX(getClientPortTable(client_name, i));
              gtk_container_set_border_width (GTK_CONTAINER (portbox), 8);
              // create checkbutton
              GtkWidget* button =
                gtk_check_button_new_with_label(short_name.c_str());
              GtkWidget *button_text = gtk_bin_get_child(GTK_BIN(button));

              GdkColor colorGreen;
              GdkColor color1;
              GdkColor color2;
              gdk_color_parse("#000000", &colorGreen);
              gtk_widget_modify_fg (button_text, GTK_STATE_NORMAL, &colorGreen);
              gdk_color_parse("#292995", &color1);
              gtk_widget_modify_fg (button_text, GTK_STATE_ACTIVE, &color1);
              gdk_color_parse("#444444", &color2);
              gtk_widget_modify_fg (button_text, GTK_STATE_PRELIGHT, &color2);
              GtkStyle *style = gtk_widget_get_style(button_text);
              pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
              pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
              gtk_widget_modify_font(button_text, style->font_desc);

              gtk_widget_set_name(button,  (gchar*)port_name.c_str());
              gtk_box_pack_start(GTK_BOX(portbox), button, FALSE, FALSE, 0);
              g_signal_connect(GTK_OBJECT (button), "toggled",
                               G_CALLBACK (gx_jack::gx_jack_port_connect),
                               GINT_TO_POINTER(i));
              gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
              gtk_widget_show_all(button);
            }
        }

      // empty queue
      gx_client_port_queue.clear();

      // move focus back to Jc_Gui main window
      gtk_widget_grab_focus(fWindow);
    }

    /* -------- delete port lists for a given jack client ---------- */
    void GxMainInterface::deleteClientPorts()

    {
      // no need to bother if are not a jack client
      if (gx_jack::client == NULL)
        {
          gx_print_warning("Jack Client Port Add",
                           "we are not yet a jack client");
          gx_client_port_dequeue.clear();
          return;
        }

      if (gx_client_port_dequeue.empty())
        return;

      // go through list
      string clname;

      multimap<string, int>::iterator pn;
      for (pn = gx_client_port_dequeue.begin(); pn != gx_client_port_dequeue.end(); pn++)
        {
          string port_name = pn->first;

          // delete port item to be displayed as a submenu if some ports exist
          clname = port_name.substr(0, port_name.find(':'));

          // check that portmap does exists, otherwise, no point
          if (!getClientPortMap(clname))
            break;

          // lookup port tables
          for (int l = 0; l < NUM_PORT_LISTS; l++)
            {
              GtkWidget* wd = getClientPort(port_name, l);
              if (wd)
                gtk_widget_destroy(wd);
            }
        }

      // we could delete the tab if needed
      bool mapempty = true;
      if (getClientPortMap(clname))
        {
          for (int l = 0; l < NUM_PORT_LISTS; l++)
            {
              GtkWidget* wd = getClientPortMap(clname);
              if (wd)
                {
                  GList* list =
                    gtk_container_get_children(GTK_CONTAINER(wd));

                  if (g_list_length(list) > 0)
                    {
                      mapempty = false;
                      break;
                    }
                }
            }
        }

      if (mapempty)
        deleteClientPortMap(clname);

      // empty queue
      gx_client_port_dequeue.clear();

      // move focus back to Jc_Gui main window
      gtk_widget_grab_focus(fWindow);
    }

    /* -------- delete jack client item ---------- */
    void GxMainInterface::deleteClientPortMap(string clname)
    {
      // no need to delete it if nothing to delete
      GtkWidget* tab = getClientPortMap(clname);
      if (!tab)
        return;

      // remove it from our list
      fClientPortMap.erase(fClientPortMap.find(tab));

      // remove the notebook tab
      int page = gtk_notebook_page_num(fPortMapTabs, tab);
      gtk_notebook_remove_page(fPortMapTabs, page);

      // destroy the widget
      if (GTK_IS_WIDGET(tab))
        gtk_widget_destroy(tab);

      // move focus back to Jc_Gui main window
      gtk_widget_grab_focus(fWindow);

    }

    /* -------- delete all jack client menus ---------- */
    void GxMainInterface::deleteAllClientPortMaps()
    {
      // don't do it if nothing to do
      if (fClientPortMap.empty())
        return;

      set<GtkWidget*>::iterator it;

      // all port maps deletion
      for (it = fClientPortMap.begin(); it != fClientPortMap.end(); it++)
        {
          GtkWidget* mapbox = *it;

          int page = gtk_notebook_page_num(fPortMapTabs, mapbox);
          gtk_notebook_remove_page(fPortMapTabs, page);

          if (GTK_IS_WIDGET(mapbox))
            gtk_widget_destroy(mapbox);
        }

      fClientPortMap.clear();

      // move focus back to Jc_Gui main window
      gtk_widget_grab_focus(fWindow);

      // print warning
      gx_print_warning("Jack Client Delete All",
                       "All client portmaps have been deleted");
    }

    /* ---------------- retrieve a client port widget --------------- */
    GtkWidget* GxMainInterface::getClientPort(const string port_name,
        const int    tab_index)
    {

      // client name
      string clname = port_name.substr(0, port_name.find(':'));

      // get client port table
      GtkWidget* table = getClientPortTable(clname, tab_index);
      if (!table)
        return NULL;

      // get list of elements
      GList* list = gtk_container_get_children(GTK_CONTAINER(table));

      // retrieve element
      for (guint p = 0; p < g_list_length(list); p++)
        {
          GtkWidget* wd = (GtkWidget*)g_list_nth_data(list, p);
          if (port_name == gtk_widget_get_name(wd))
            return wd;
        }

      return NULL;
    }

    /* --------------- retrieve a client port table widget -------------- */
    GtkWidget* GxMainInterface::getClientPortTable(const string clname,
        const int    index)
    {

      // get port map
      GtkWidget* portmap = getClientPortMap(clname);
      if (!portmap)
        return NULL;

      // look up list of vboxes in portmap
      GList* list = gtk_container_get_children(GTK_CONTAINER(portmap));
      return (GtkWidget*)g_list_nth_data(list, index);
    }

    /* ----------------- retrieve a client portmap widget --------------- */
    GtkWidget* GxMainInterface::getClientPortMap(const string clname)
    {
      // try to find a match
      set<GtkWidget*>::iterator it;

      for (it = fClientPortMap.begin(); it != fClientPortMap.end(); it++)
        if (clname == gtk_widget_get_name(*it))
          return *it; // got it

      return NULL;
    }

    /* -------- user interface builder ---------- */
    void GxMainInterface::setup()
    {
      //----- notebook window with tabs representing jack clients and portmaps
      // Note: out of box stack scheme.
      createPortMapWindow("Jack Port Maps");

      gx_engine::GxEngine* engine = gx_engine::GxEngine::instance();



      //----- the main box, all visible widgets are a child of this box
      openVerticalBox("");
      //----- add the menubar on top
      {
        addMainMenu();
        //----- this is a dummy widget, only for save settings for the latency warning dialog
        openWarningBox("WARNING", &engine->fwarn);
      }
      closeBox();
      //----- the upper box,
      openVertical1Box("");
      {
        openHorizontalBox("");
        {
          //----- the balance widget
          openFrame1Box("");
          {
            addslider("balance", &engine->fslider25, 0.f, -1.f, 1.f, 1.e-01f);
          }
          closeBox();
        }
        closeBox();

        openHorizontalBox("");
        {
          openVerticalBox("");
          {
            openVertical1Box("");
            {
              openHorizontalBox("");
              {
                openVerticalBox("");
                {
                  openFrame1Box("");
                  closeBox();
                  // add a meter level box: out of box stack, no need to closeBox
                  openLevelMeterBox("Signal Level");
                  openFrameBox("");
                  closeBox();
                }
                closeBox();
                openVertical1Box("volume");
                {
                  addbigregler(" out ", &engine->fslider17, 0.f, -40.f, 40.f, 0.1f);
                }
                closeBox();
                //----- volume controll ready
                //----- open a box for the tone and the fuzz controllers
                openVertical1Box("tone");
                {
                  addregler("bass",   &engine->fslider_tone2, 0.f, -20.f, 20.f, 0.1f);
                  addregler("middle", &engine->fslider_tone1, 0.f, -20.f, 20.f, 0.1f);
                  addregler("treble", &engine->fslider_tone0, 0.f, -20.f, 20.f, 0.1f);
                }
                closeBox();
              }
              closeBox();
            }
            closeBox();
          }
          closeBox();
          openVerticalBox("");
          {
            openVertical1Box("");
            {
              addslider("wet/dry", &engine->fslider24,  0.f, -1.f, 1.f, 1.e-01f);
              openHorizontalBox("");
              {
                openHorizontalBox("");
                {
                  addregler("  left delay  ", &engine->fsliderdel0,  0.f, 0.f, 5000.0f, 10.f);
                  addregler(" right delay ", &engine->fsliderdel1,  0.f, 0.f, 5000.0f, 10.f);
                  addregler("  left gain  ", &engine->fjc_ingain,  0.f, -20.f, 20.f, 0.1f);
                  addregler(" right gain ", &engine->fjc_ingain1,  0.f, -20.f, 20.f, 0.1f);
                }
                closeBox();
              }
              closeBox();
              openVertical1Box("");
              {
                addstoggle("jconvolver settings", &engine->filebutton);
                openHorizontalBox("");
                closeBox();
                addbtoggle("run jconvolver", &gx_jconv::GxJConvSettings::checkbutton7);
              }
              closeBox();
            }
            closeBox();
          }
          closeBox();
        }
        closeBox();
      }
      closeBox();
    }

    //---- show main GUI
    void GxMainInterface::show()
    {
      assert(fTop == 0);
      gx_init_pixmaps();
      fInitialized = true;

      if (gx_jack::client)
        {
          // refresh some GUI stuff
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(fJackConnectItem), TRUE);

          GtkWidget* wd = getJackLatencyItem(gx_jack::jack_bs);
          if (wd) gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(wd), TRUE);

          gtk_window_set_title(GTK_WINDOW(fWindow), gx_jack::client_name.c_str());

          // build port menus for existing jack clients
          initClientPortMaps();
        }
      else
        {
          gtk_widget_hide(gx_gui::gx_jackd_on_image);
          gtk_widget_show(gx_gui::gx_jackd_off_image);
        }

      gtk_widget_show  (fBox[0]);
      gtk_widget_show  (fWindow);
      gx_jconv::gx_setting_jconv_dialog_gui(NULL,NULL);
    }

    //---- show main GUI thread and more
    void GxMainInterface::run()
    {
      string previous_state = gx_user_dir + gx_jack::client_name + "rc";
      recallState(previous_state.c_str());

      //----- set the state for the latency change warning widget
      gx_engine::GxEngine::instance()->set_latency_warning_change();

      /* timeout in milliseconds */
      g_timeout_add(update_gui,  gx_update_all_gui,        0);
      g_timeout_add_full(G_PRIORITY_LOW,refresh_jack, gx_survive_jack_shutdown, 0, NULL);
      g_timeout_add_full(G_PRIORITY_LOW,refresh_ports, gx_monitor_jack_ports,0, NULL);
      g_timeout_add(750, gx_check_startup, 0);

      // Note: meter display timeout is a global var in gx_gui namespace
      g_timeout_add(meter_display_timeout, gx_refresh_meter_level,   0);
      gtk_main();
      stop();
    }

  } /* end of gx_gui namespace */
