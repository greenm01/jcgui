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
*/

// ***** GtkWaveView.cpp *****
/******************************************************************************
part of Jc_Gui, show a wave with Gtk
******************************************************************************/

#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <cmath>
#include <gtk/gtk.h>
#include <sndfile.h>
#include <jack/jack.h>

#include "Jc_Gui.h"


using namespace gx_jconv;

#define GTK_TYPE_WAVEVIEW          (gtk_waveview_get_type())
#define GTK_WAVEVIEW(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WAVEVIEW, GtkWaveView))
#define GTK_IS_WAVEVIEW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WAVEVIEW))
#define GTK_WAVEVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_WAVEVIEW, GtkWaveViewClass))
#define GTK_IS_WAVEVIEW_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GTK_TYPE_WAVEVIEW))

#define ARRAY_SIZE (451)

struct GtkWaveViewClass
  {
    GtkRangeClass parent_class;
    cairo_surface_t *surface_file;
    cairo_surface_t *surface_selection;


    GtkTooltips *comandline;

    int waveview_x;
    int waveview_y;
    int waveleft;
    int wavestay;
    int wavebutton;
    double scale_view;

    int offset_cut;
    int length_cut;
    int filelength;
    double  drawscale;

    int new_pig;

  };

GType gtk_waveview_get_type ();

//----- draw a new wave in a idle thread
static gboolean gtk_waveview_paint(gpointer obj)
{
  GtkWidget *widget = (GtkWidget *)obj;
  g_assert(GTK_IS_WAVEVIEW(widget));
  GtkWaveView *waveview = GTK_WAVEVIEW(widget);

  cairo_t *cr, *cr_show;

  cr_show = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_file);
// retrieve JConv settings
  GxJConvSettings* const jcset = GxJConvSettings::instance();

  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut = 0;
  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut = 0;

  // IR file parameters
  SNDFILE* pvInput = NULL;

  int counter    = 0;
  int vecsize    = 64;
  int length     = 0;
  int length2    = 0;
  int countframe = 1;
  int countfloat;
  int chans;
  int sr;
  float *sfsig;

  double waw = 600; // widget->allocation.width*4 we cant use width in a scrolable window
  double wah = widget->allocation.height*0.5;
  double wah1 = widget->allocation.height*0.25;


  float* sig;
  // some usefull cairo settings
  cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_file);
  cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
  cairo_set_antialias(cr,CAIRO_ANTIALIAS_SUBPIXEL);
  cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
  // draw the background
  cairo_move_to (cr, 0, 0);
  cairo_rectangle (cr, 0, 0, waw, widget->allocation.height);
  cairo_fill_preserve (cr);
  cairo_set_line_width (cr, 1.0);
  cairo_set_source_rgb (cr, 0.3, 0.7, 0.3);
  cairo_stroke (cr);
  // draw the widget frame
  cairo_move_to (cr, 0, wah);
  cairo_line_to (cr, waw, wah);
  cairo_set_line_width (cr, 2.0);
  cairo_set_source_rgb (cr, 0.3, 0.7, 0.3);
  cairo_stroke (cr);

  //----- draw an X if JConv IR file not valid
  if (jcset->isValid() == false)
    {
      gx_system::gx_print_warning("Wave view expose",
                                  GxJConvSettings::instance()->getIRFile() +
                                  " cannot be exposed ");

      cairo_move_to (cr, 0, widget->allocation.height);
      cairo_line_to (cr, waw, 0);
      cairo_move_to (cr, 0, 0);
      cairo_line_to (cr, waw, widget->allocation.height);
      cairo_set_line_width (cr, 8.0);
      cairo_set_source_rgb (cr, 0.8, 0.2, 0.2);
      cairo_stroke (cr);

    }

  //----- okay, here we go, draw the wave view per sample
  else
    {

      gx_system::gx_print_info("Wave view expose", jcset->getIRFile());

      sig = new float[vecsize*2];
      vector<float>yval;

      

      pvInput =
        gx_sndfile::Audio::instance()->openInputSoundFile(jcset->getFullIRPath().c_str(), &chans, &sr, &length2);

      double dws = GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale = ((double)waw)/length2;
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength = length2;

      switch (chans)
        {
          //----- mono file
        case 1:
          yval.push_back(wah);
          cairo_set_line_width (cr, 1 + dws*0.5);
          cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);


          while (counter < length+length2-1)
            {
              gx_sndfile::Audio::instance()->readSoundInput(pvInput, sig, vecsize);
              counter   += vecsize;
              countfloat = 0;
              sfsig = &sig[0];

              while (countfloat < vecsize*chans)
                {
                  cairo_move_to (cr, countframe*dws, wah);
                  cairo_line_to (cr, countframe*dws,
                                 *sfsig++ *wah + yval[0]);
                  countfloat++;
                  countframe++;
                }
              cairo_stroke (cr);
            }



          /// close file desc.
          gx_sndfile::Audio::instance()->closeSoundFile(pvInput);
          delete[] sig;
          break;

          //----- stereo file
        case 2:

          wah = widget->allocation.height*0.75;
          wah1 = widget->allocation.height*0.25;

          cairo_move_to (cr, 0, wah);
          cairo_line_to (cr, waw, wah);
          cairo_move_to (cr, 0, wah1);
          cairo_line_to (cr, waw, wah1);
          cairo_set_line_width (cr, 2.0);
          cairo_set_source_rgb (cr, 0.3, 0.7, 0.3);
          cairo_stroke (cr);

          yval.push_back(50);
          yval.push_back(150);
          cairo_set_line_width (cr, 1 + dws*0.5);
          cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);

          while (counter<length+length2-1)
            {
              gx_sndfile::Audio::instance()->readSoundInput(pvInput, sig, vecsize);
              counter   += vecsize;
              countfloat = 0;
              sfsig = &sig[0];

              //----- here we do the stereo draw, tingel tangel split the samples
              while (countfloat < vecsize*chans)
                {
                  for (int c = 0; c < 2; c++)
                    {
                      cairo_move_to (cr, countframe*dws, yval[c]);
                      cairo_line_to (cr, countframe*dws,
                                     *sfsig++ *wah1 + yval[c]);

                      countfloat++;

                    }
                  countframe++;
                }
              cairo_stroke (cr);
            }

          gx_sndfile::Audio::instance()->closeSoundFile(pvInput);

          delete[] sig;
          break;

        default: // do nothing
          break;
        }
    }
  // copy the surface to a packup surface for the selection
  cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection);
  cairo_set_source_surface (cr, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_file, 0,0);
  cairo_paint (cr);

  //----- draw the selected part (offset  length) with transparent green rectangle
  if ((jcset->getOffset() != (guint)GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut) ||
      (jcset->getLength() != (guint)GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut))
    {
      // -- IR offset
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut =
        jcset->getOffset();

      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut =
        jcset->getLength();

      jcset->setOffset(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut);

      if ((GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut < 0) ||
          (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut >
           (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength)))
        {
          jcset->setOffset(0);
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut
          += GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut;
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut = 0;
        }

      // -- IR length (starting at offset)
      jcset->setLength(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut);
      if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut +
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut >
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength)
        {
          jcset->setLength(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength -
                           GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut);
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut = jcset->getLength();
        }

      // -- tooltips
      ostringstream tip;
      tip << "offset ("    << jcset->getOffset()
      << ")  length (" << jcset->getLength() << ") ";

      gtk_widget_set_sensitive(widget, TRUE);
      gtk_tooltips_set_tip (GTK_TOOLTIPS (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->comandline),
                            widget, tip.str().c_str(), "the offset and length.");

      cairo_set_source_rgba (cr, 0.5, 0.8, 0.5,0.3);
      cairo_rectangle (cr, jcset->getOffset()*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale, 0,
                       jcset->getLength()*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale, widget->allocation.height);
      cairo_fill (cr);


      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay =
        jcset->getOffset()*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale;

      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft =
        jcset->getLength()*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale;

    }
// set scal range to the surface and the screen
  gtk_widget_set_size_request (GTK_WIDGET(waveview), 300.0*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view*2, 200.0);
  cairo_scale (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, 1);
  // copy the surface to the screen
  cairo_set_source_surface (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection, widget->allocation.x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, widget->allocation.y);
  cairo_paint (cr_show);
  // destroy surface handler
  cairo_destroy (cr);
  cairo_destroy (cr_show);

  return false;
}

//----- create and draw the widgets
static gboolean gtk_waveview_expose (GtkWidget *widget, GdkEventExpose *event)
{
  g_assert(GTK_IS_WAVEVIEW(widget));
  GtkWaveView *waveview = GTK_WAVEVIEW(widget);

  // ============= preview widget for JConv settings

  cairo_t *cr, *cr_show;
  cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection);
  cr_show = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  //----- create the background widget when opening a new file
  if (gx_gui::new_wave_view == true)
    {
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_move_to (cr, 0, 0);
      cairo_rectangle (cr, 0, 0, 600, 200);
      cairo_fill_preserve (cr);
      cairo_set_source_rgba (cr, 0.8, 0.8, 0.2,0.6);
      cairo_set_font_size (cr, 84.0);
      cairo_move_to (cr, 120, 150);
      cairo_show_text(cr, "loading");
      cairo_stroke (cr);
      // paint the wave in a low prio idle thread
      g_idle_add(gtk_waveview_paint,gpointer (widget));
      // done with new view:
      gx_gui::new_wave_view = false;
    }
  // set scal range to the surface and the screen
  gtk_widget_set_size_request (GTK_WIDGET(waveview), 300.0*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view*2, 200.0);
  cairo_scale (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, 1);
  // copy the surface to the screen
  cairo_set_source_surface (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection, widget->allocation.x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, widget->allocation.y);
  cairo_paint (cr_show);
  // destroy surface handler
  cairo_destroy (cr);
  cairo_destroy (cr_show);

  return TRUE;
}

//----- a new wavefile is load to the jconv preview widget
gboolean GtkWaveView::gtk_waveview_set_value (GtkWidget *cwidget, gpointer data )
{
  gx_gui::new_wave_view = true;
  return TRUE;
}



//----- mouse funktions to select a part of the file
static gboolean gtk_waveview_pointer_motion (GtkWidget *widget, GdkEventMotion *event)
{
  g_assert(GTK_IS_WAVEVIEW(widget));
  GtkWaveView *waveview = GTK_WAVEVIEW(widget);

  // JConv IR file
  if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton)
    {
      // retrieve JConv settings handler
      GxJConvSettings* const jcset = GxJConvSettings::instance();
      cairo_t *cr, *cr_show;

      cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection);
      cr_show = gdk_cairo_create(GDK_DRAWABLE(widget->window));

      if (GTK_WIDGET_HAS_GRAB(widget))
        {
          // copy the file surface to the selection surface for a new selection
          cairo_set_source_surface (cr, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_file,0,0);
          cairo_paint (cr);
          cairo_set_source_rgba (cr, 0.5, 0.8, 0.5,0.3);
          double event_x;

          switch (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton)
            {
            case 1: // left mouse button is pressed and move to right
              if (event->x>waveview->start_x)
                {
                  cairo_rectangle (cr,round( waveview->start_x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view), 0,
                                   round((event->x - waveview->start_x)/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view), widget->allocation.height);
                  cairo_fill (cr);

                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft = round((event->x - waveview->start_x)/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view);
                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay = round(waveview->start_x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view);
                }
              else // left mouse button is pressed and move to left
                {
                  cairo_rectangle (cr, round(waveview->start_x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view), 0,
                                   round((event->x - waveview->start_x)/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view), widget->allocation.height);
                  cairo_fill (cr);

                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft = round((waveview->start_x - event->x)/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view);
                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay = round(event->x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view);
                }
              break;

            case 2: // middle mouse button is pressed and move
              event_x =  round((event->x)/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view - (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft *0.5 ));
              cairo_rectangle (cr, event_x, 0,
                               GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft,widget->allocation.height);
              cairo_fill (cr);

              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay = event_x;
              break;

            case 3: // right mousbutton is pressed and move
              if (event->x>GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay)
                {

                  cairo_rectangle (cr, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay, 0,
                                   round((event->x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view)-GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay),widget->allocation.height);
                  cairo_fill (cr);

                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft =
                    round((event->x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view)-GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay);
                }
              break;

            default: // do nothing
              break;
            }

          // -- IR offset
          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut =
            round(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavestay/
                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale);

          GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut =
            round(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveleft/
                  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->drawscale);

          jcset->setOffset(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut);

          if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut < 0)
            {
              jcset->setOffset(0);
              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut
              += GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut;
              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut = 0;
            }

          // -- IR length (starting at offset)
          jcset->setLength(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut);
          if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut +
              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut >
              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength)
            {
              jcset->setLength(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->filelength -
                               GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut);
              GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut = jcset->getLength();
            }


        }
      // set scale range to the screen and copy suface to it
      gtk_widget_set_size_request (GTK_WIDGET(waveview), 300.0*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view*2, 200.0);
      cairo_scale (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, 1);
      cairo_set_source_surface (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection, widget->allocation.x/GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, widget->allocation.y);
      cairo_paint (cr_show);

      // set tooltip to the new offset/lengh
      if (GTK_WIDGET_HAS_GRAB(widget))
        {
          // -- tooltips
          ostringstream tip;
          tip << "offset ("    << jcset->getOffset()
          << ")  length (" << jcset->getLength() << ") ";

          gtk_widget_set_sensitive(widget, TRUE);
          gtk_tooltips_set_tip (GTK_TOOLTIPS (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->comandline),
                                widget, tip.str().c_str(), "the offset and length.");
        }

      cairo_destroy (cr);
      cairo_destroy (cr_show);


    }
  return FALSE;
}

//-------- button press event
static gboolean gtk_waveview_button_press(GtkWidget *widget, GdkEventButton *event)
{
  g_assert(GTK_IS_WAVEVIEW(widget));
  GtkWaveView *waveview = GTK_WAVEVIEW(widget);

  gtk_widget_grab_focus(widget);
  gtk_widget_grab_default(widget);
  gtk_grab_add(widget);

  // retrieve instance of jconv settings handler
  GxJConvSettings* jcset = GxJConvSettings::instance();
  ostringstream tip;

  switch (event->button)
    {
    case 1:
      waveview->start_x = event->x;
      waveview->start_y = event->y;
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton = 1;
      break;

    case 2:
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton = 2;
      break;

    case 3:  // scale the surface to screen
      cairo_t *cr, *cr_show;

      cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection);
      cairo_set_source_surface (cr, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_file,0,0);
      cairo_paint (cr);
      cr_show = gdk_cairo_create(GDK_DRAWABLE(widget->window));
      gtk_widget_set_size_request (GTK_WIDGET(waveview), 300.0*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view*2, 200.0);
      cairo_scale (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, 1);
      cairo_set_source_surface (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection, widget->allocation.x, widget->allocation.y);
      cairo_paint (cr_show);

      cairo_destroy (cr);
      cairo_destroy (cr_show);

      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->offset_cut = 0;
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->length_cut = 0;
      GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton = 3;

      jcset->setOffset(0);
      jcset->setLength(0);

      tip << "offset ("    << jcset->getOffset()
      << ")  length (" << jcset->getLength() << ") ";

      gtk_widget_set_sensitive(widget, TRUE);
      gtk_tooltips_set_tip (GTK_TOOLTIPS (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->comandline),
                            widget, tip.str().c_str(), "the offset and length.");
      break;

    default: // do nothing
      break;
    }

  return TRUE;
}

static gboolean gtk_waveview_button_release (GtkWidget *widget, GdkEventButton *event)
{
  g_assert(GTK_IS_WAVEVIEW(widget));

  if (GTK_WIDGET_HAS_GRAB(widget))
    gtk_grab_remove(widget);
  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->wavebutton=0;

  return FALSE;
}

//----------- set value from mouseweel
static gboolean gtk_waveview_scroll (GtkWidget *widget, GdkEventScroll *event)
{
  g_assert(GTK_IS_WAVEVIEW(widget));
  GtkWaveView *waveview = GTK_WAVEVIEW(widget);

  double setscale;

  if (event->direction == 0) setscale = -0.1;
  else setscale = 0.1;
  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view += setscale;
  if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view <0.5)
    GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view = 0.5;
  else if (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view >10)
    GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view = 10;

  cairo_t *cr, *cr_show;

  cr = cairo_create (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection);

  cr_show = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  gtk_widget_set_size_request (GTK_WIDGET(waveview), 300.0*GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view*2, 200.0);
  cairo_scale (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->scale_view, 1);
  cairo_set_source_surface (cr_show, GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->surface_selection, widget->allocation.x, widget->allocation.y);
  cairo_paint (cr_show);

  cairo_destroy (cr);
  cairo_destroy (cr_show);

  return FALSE;
}

//----------- set size for GdkDrawable per type
static void gtk_waveview_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  g_assert(GTK_IS_WAVEVIEW(widget));

  requisition->width = GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveview_x;
  requisition->height = GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveview_y;
}

//----------- init the GtkWaveViewClass
static void gtk_waveview_class_init (GtkWaveViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  klass->waveview_x = 300;
  klass->waveview_y = 200;
  klass->scale_view = 0.5;

  widget_class->expose_event = gtk_waveview_expose;
  widget_class->size_request = gtk_waveview_size_request;
  widget_class->button_press_event = gtk_waveview_button_press;
  widget_class->button_release_event = gtk_waveview_button_release;
  widget_class->motion_notify_event = gtk_waveview_pointer_motion;
  widget_class->scroll_event = gtk_waveview_scroll;

  klass->surface_file = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, klass->waveview_x*2, klass->waveview_y);
  g_assert(klass->surface_file != NULL);
  klass->surface_selection = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, klass->waveview_x*2, klass->waveview_y);
  g_assert(klass->surface_selection != NULL);
}

//----------- init the WaveView type
static void gtk_waveview_init (GtkWaveView *waveview)
{
  GtkWidget *widget = GTK_WIDGET(waveview);
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(waveview), GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(waveview), GTK_CAN_DEFAULT);

  widget->requisition.width = GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveview_x;
  widget->requisition.height = GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->waveview_y;
}


void GtkWaveView::gtk_waveview_destroy (GtkWidget *weidget, gpointer data )
{
  GtkWidget *widget = GTK_WIDGET( g_object_new (GTK_TYPE_WAVEVIEW, NULL ));
  g_assert(GTK_IS_WAVEVIEW(widget));

  if (G_IS_OBJECT(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))-> surface_file))
    cairo_surface_destroy(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))-> surface_file);

  if (G_IS_OBJECT(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))-> surface_selection))
    cairo_surface_destroy(GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))-> surface_selection);
}

//----------- create waveview widget
GtkWidget* GtkWaveView::gtk_wave_view()
{
  GtkWidget* widget = GTK_WIDGET( g_object_new (GTK_TYPE_WAVEVIEW, NULL ));

  gx_gui::new_wave_view   = false;

  // retrieve JConv settings
  GxJConvSettings* jcset = GxJConvSettings::instance();

  ostringstream tip;
  tip << "offset (" << jcset->getOffset() << ") "
  << "length (" << jcset->getLength() << ") ";

  gtk_widget_set_sensitive(widget, TRUE);
  GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->comandline =
    gtk_tooltips_new ();

  gtk_tooltips_set_tip(GTK_TOOLTIPS (GTK_WAVEVIEW_CLASS(GTK_OBJECT_GET_CLASS(widget))->comandline),
                       widget, tip.str().c_str(), "the IR offset and length.");

  return widget;
}

//----------- get the WaveView type
GType gtk_waveview_get_type (void)
{
  static GType kn_type = 0;
  if (!kn_type)
    {
      static const GTypeInfo kn_info =
      {
        sizeof(GtkWaveViewClass), NULL,  NULL,
        (GClassInitFunc)gtk_waveview_class_init, NULL, NULL,
        sizeof (GtkWaveView), 0, (GInstanceInitFunc)gtk_waveview_init
      };
      kn_type = g_type_register_static(GTK_TYPE_RANGE,
                                       "GtkWaveView", &kn_info, (GTypeFlags)0);
    }
  return kn_type;
}

//----------- wrapper for signal connection
void gx_waveview_set_value(GtkWidget* widget, gpointer data)
{

  GtkWaveView wave_view;
  wave_view.gtk_waveview_set_value(widget, data);
}

//----
void gx_waveview_destroy(GtkWidget* widget, gpointer data)
{

  GtkWaveView wave_view;
  wave_view.gtk_waveview_destroy(widget, data);
}

//----
GtkWidget* gx_wave_view()
{
  GtkWaveView wave_view;
  return wave_view.gtk_wave_view();
}
