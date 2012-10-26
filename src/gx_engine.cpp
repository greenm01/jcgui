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
 *
 *	This is the Jc_Gui engine definitions
 *
 *
 * --------------------------------------------------------------------------
 */

#include <cstring>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <cmath>
#include <gtk/gtk.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <sndfile.h>

#include "Jc_Gui.h"

using namespace gx_system;

namespace gx_engine
  {

    void GxEngine::metadata(Meta* m)
    {
      m->declare("name", "Jc_Gui");
      m->declare("version", "0.01");
      m->declare("author", "brummer");
      m->declare("contributor", "Julius O. Smith (jos at ccrma.stanford.edu)");
      m->declare("license", "BSD");
      m->declare("copyright", "(c)brummer 2008");
      m->declare("reference", "http://ccrma.stanford.edu/realsimple/faust_strings/");
    }

    void GxEngine::initEngine(int samplingFreq)
    {
      // initialize all variables for audio, midi and interface
      fSamplingFreq = samplingFreq;

      //----- tone
      fslider_tone0 = 0.0f;
      fConst_tone0 = (15079.645508f / fSamplingFreq);
      fConst_tone1 = cosf(fConst_tone0);
      fConst_tone2 = (1.414214f * sinf(fConst_tone0));
      fslider_tone1 = 0.0f;
      fConst_tone3 = (3769.911377f / fSamplingFreq);
      fConst_tone4 = cosf(fConst_tone3);
      fConst_tone5 = (1.414214f * sinf(fConst_tone3));
      fslider_tone2 = 0.0f;
      zeroize(fVec_tone0, 3);
      zeroize(fRec_tone3, 3);
      zeroize(fRec_tone2, 3);
      zeroize(fRec_tone1, 3);
      zeroize(fRec_tone0, 3);
      // tone end
      zeroize(fVec_ltone0, 3);
      zeroize(fRec_ltone3, 3);
      zeroize(fRec_ltone2, 3);
      zeroize(fRec_ltone1, 3);
      zeroize(fRec_ltone0, 3);
      // engine state
      checky = (float)kEngineOn;
      fslider17 = 0.0f;
      zeroize(fRec46, 2);
      zeroize(fRec47, 2);

      fslider24 = 0.0f;
      fslider25 = 0.0f;
      // lets init the variables for the tonesettings
      fSlow_mid_tone = (fslider_tone1*0.5);
      fSlow_tone0  = powf(10, (2.500000e-02f * (fslider_tone0- fSlow_mid_tone)));
      fSlow_tone1  = (1 + fSlow_tone0);
      fSlow_tone2  = (fConst_tone1 * fSlow_tone1);
      fSlow_tone3  = (2 * (0 - ((1 + fSlow_tone2) - fSlow_tone0)));
      fSlow_tone4  = (fConst_tone1 * (fSlow_tone0 - 1));
      fSlow_tone5  = (fConst_tone2 * sqrtf(fSlow_tone0));
      fSlow_tone6  = (fSlow_tone1 - (fSlow_tone5 + fSlow_tone4));
      fSlow_tone7  = powf(10, (2.500000e-02f * fSlow_mid_tone));
      fSlow_tone8  = (1 + fSlow_tone7);
      fSlow_tone9  = (fConst_tone4 * fSlow_tone8);
      fSlow_tone10 = (2 * (0 - ((1 + fSlow_tone9) - fSlow_tone7)));
      fSlow_tone11 = (fSlow_tone7 - 1);
      fSlow_tone12 = (fConst_tone4 * fSlow_tone11);
      fSlow_tone13 = sqrtf(fSlow_tone7);
      fSlow_tone14 = (fConst_tone5 * fSlow_tone13);
      fSlow_tone15 = (fSlow_tone8 - (fSlow_tone14 + fSlow_tone12));
      fSlow_tone16 = (fConst_tone1 * fSlow_tone8);
      fSlow_tone17 = (0 - (2 * ((fSlow_tone7 + fSlow_tone16) - 1)));
      fSlow_tone18 = (fConst_tone2 * fSlow_tone13);
      fSlow_tone19 = (fConst_tone1 * fSlow_tone11);
      fSlow_tone20 = ((1 + (fSlow_tone7 + fSlow_tone19)) - fSlow_tone18);
      fSlow_tone21 = powf(10, (2.500000e-02f * (fslider_tone2-fSlow_mid_tone)));
      fSlow_tone22 = (1 + fSlow_tone21);
      fSlow_tone23 = (fConst_tone4 * fSlow_tone22);
      fSlow_tone24 = (0 - (2 * ((fSlow_tone21 + fSlow_tone23) - 1)));
      fSlow_tone25 = (fConst_tone5 * sqrtf(fSlow_tone21));
      fSlow_tone26 = (fConst_tone4 * (fSlow_tone21 - 1));
      fSlow_tone27 = ((1 + (fSlow_tone21 + fSlow_tone26)) - fSlow_tone25);
      fSlow_tone28 = (2 * (0 - ((1 + fSlow_tone23) - fSlow_tone21)));
      fSlow_tone29 = (fSlow_tone21 + fSlow_tone25);
      fSlow_tone30 = ((1 + fSlow_tone29) - fSlow_tone26);
      fSlow_tone31 = (fSlow_tone22 - (fSlow_tone25 + fSlow_tone26));
      fSlow_tone32 = (1.0f / (1 + (fSlow_tone26 + fSlow_tone29)));
      fSlow_tone33 = (fSlow_tone8 - (fSlow_tone18 + fSlow_tone19));
      fSlow_tone34 = (2 * (0 - ((1 + fSlow_tone16) - fSlow_tone7)));
      fSlow_tone35 = (fSlow_tone7 + fSlow_tone18);
      fSlow_tone36 = ((1 + fSlow_tone35) - fSlow_tone19);
      fSlow_tone37 = (1.0f / (1 + (fSlow_tone19 + fSlow_tone35)));
      fSlow_tone38 = (fSlow_tone7 * ((1 + (fSlow_tone7 + fSlow_tone12)) - fSlow_tone14));
      fSlow_tone39 = (fSlow_tone7 + fSlow_tone14);
      fSlow_tone40 = (fSlow_tone7 * (1 + (fSlow_tone12 + fSlow_tone39)));
      fSlow_tone41 = (((fSlow_tone7 + fSlow_tone9) - 1) * (0 - (2 * fSlow_tone7)));
      fSlow_tone42 = (1.0f / ((1 + fSlow_tone39) - fSlow_tone12));
      fSlow_tone43 = (fSlow_tone0 * ((1 + (fSlow_tone0 + fSlow_tone4)) - fSlow_tone5));
      fSlow_tone44 = (fSlow_tone0 + fSlow_tone5);
      fSlow_tone45 = (fSlow_tone0 * (1 + (fSlow_tone4 + fSlow_tone44)));
      fSlow_tone46 = (((fSlow_tone0 + fSlow_tone2) - 1) * (0 - (2 * fSlow_tone0)));
      fSlow_tone47 = (1.0f / ((1 + fSlow_tone44) - fSlow_tone4));

      fslider_tone_check = (fslider_tone1+fslider_tone0+fslider_tone2)*100;
      fslider_tone_check1 = 0;

      gx_jconv::checkbox7 = 1.0;
      fjc_ingain = 0;
      zeroize(fRecinjc, 2);
      zeroize(fRecinjcr, 2);
      IOTAdel = 0;
      zeroize(fVecdel0,262144);
      fsliderdel0 = 0.0f;
      fConstdel0 = (1.000000e-03f * fSamplingFreq);
      zeroize(fVecdel1,262144);
      fsliderdel1 = 0.0f;
      fjc_ingain1 = 0.0f;
      filebutton = 0;
      fwarn = 0;
      // end engine init
    }

    /* --- adding rest of engine class by file inclusion for readability --- */

    /* audio engine */
#include "gx_engine_audio.cpp"

    /* --- forward definition of useful namespace functions --- */
    void gx_engine_init()
    {
      GxEngine* engine = GxEngine::instance();
      gNumInChans  = engine->getNumInputs();
      gNumOutChans = engine->getNumOutputs();

      //----- lock the buffer for the oscilloscope
      const int frag = (const int)gx_jack::jack_bs;

      get_frame  = new float[frag];
      get_frame1  = new float[frag];

      (void)memset(get_frame,  0, frag*sizeof(float));
      (void)memset(get_frame1,  0, frag*sizeof(float));

      engine->initEngine((int)gx_jack::jack_sr);

      initialized = true;
    }

    /* --- forward definition of useful namespace functions --- */
    void gx_engine_reset()
    {

      if (get_frame)  delete[] get_frame;
      if (get_frame1)  delete[] get_frame1;
      initialized = false;
    }

  } /* end of gx_engine namespace */
