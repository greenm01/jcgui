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

/* ------- This is the Jc_Gui Engine namespace ------- */

#pragma once

// --- defines the processing type
#define ZEROIZE_BUFFERS  (0)
#define JUSTCOPY_BUFFERS (1)
#define PROCESS_BUFFERS  (2)

// --- engine interface defines
#define stackSize 256
#define kSingleMode 0
#define kBoxMode 1
#define kTabMode 2

#define max(x,y) (((x)>(y)) ? (x) : (y))
#define min(x,y) (((x)<(y)) ? (x) : (y))

class GxMainInterface;

namespace gx_engine
{
  /* --------------- function declarations --------------- */


  /* function declarations  */
  void gx_engine_init();
  void gx_engine_reset();

  /* ------------- Engine Processing Classes ------------- */
  // metadata
  struct Meta : map<const char*, const char*>
  {
    void declare (const char* key, const char* value)
    {
      (*this)[key] = value;
    }
  };

  /* base engine class definition, not to be used directly */
  class dsp
  {
  protected:
    // Note: an instance of a dsp class with a sample rate = 0 is NOT initialized

    // sample rate given by jack.
    int fSamplingFreq;


  public:
    dsp() {}

    virtual ~dsp() {}

    virtual void compute(int len, float** inputs, float** outputs) = 0;

    bool isInitialized()
    {
      return fSamplingFreq != 0;
    }

  private:
    virtual void process_buffers(const int len, float** inputs, float** outputs) = 0;

  };


  /* -- Jc_Gui main engine class -- */
  class GxEngine : public dsp
  {
  public:

    // register all variables

    //----- tone
    float fslider_tone0; // tone treble controller
    float fConst_tone0;
    float fConst_tone1;
    float fConst_tone2;
    float fslider_tone1; // tone middle controller
    float fConst_tone3;
    float fConst_tone4;
    float fConst_tone5;
    float fslider_tone2; // tone middle controller
    float fVec_tone0[3];
    float fRec_tone3[3];
    float fRec_tone2[3];
    float fRec_tone1[3];
    float fRec_tone0[3];
    // tone end
    float fVec_ltone0[3];
    float fRec_ltone3[3];
    float fRec_ltone2[3];
    float fRec_ltone1[3];
    float fRec_ltone0[3];

    float fslider17;
    float fRec46[2];
    float fRec47[2];

    float fslider24;
    float fslider25;
    // lets init the variable for the tone settings
    float fSlow_mid_tone ;
    float fSlow_tone0;
    float fSlow_tone1 ;
    float fSlow_tone2 ;
    float fSlow_tone3 ;
    float fSlow_tone4 ;
    float fSlow_tone5 ;
    float fSlow_tone6 ;
    float fSlow_tone7 ;
    float fSlow_tone8 ;
    float fSlow_tone9 ;
    float fSlow_tone10 ;
    float fSlow_tone11 ;
    float fSlow_tone12 ;
    float fSlow_tone13 ;
    float fSlow_tone14 ;
    float fSlow_tone15 ;
    float fSlow_tone16 ;
    float fSlow_tone17 ;
    float fSlow_tone18 ;
    float fSlow_tone19 ;
    float fSlow_tone20 ;
    float fSlow_tone21 ;
    float fSlow_tone22 ;
    float fSlow_tone23 ;
    float fSlow_tone24 ;
    float fSlow_tone25 ;
    float fSlow_tone26 ;
    float fSlow_tone27 ;
    float fSlow_tone28 ;
    float fSlow_tone29 ;
    float fSlow_tone30 ;
    float fSlow_tone31 ;
    float fSlow_tone32 ;
    float fSlow_tone33 ;
    float fSlow_tone34 ;
    float fSlow_tone35 ;
    float fSlow_tone36 ;
    float fSlow_tone37 ;
    float fSlow_tone38 ;
    float fSlow_tone39 ;
    float fSlow_tone40 ;
    float fSlow_tone41 ;
    float fSlow_tone42 ;
    float fSlow_tone43 ;
    float fSlow_tone44 ;
    float fSlow_tone45 ;
    float fSlow_tone46 ;
    float fSlow_tone47 ;
    int   fslider_tone_check;
    int   fslider_tone_check1;
    // tone end

    float fjc_ingain;
    float fRecinjc[2];
    int   IOTAdel;
	float fVecdel0[262144];
	float fsliderdel0;
	float fConstdel0;
	float fVecdel1[262144];
	float fsliderdel1;
	float fjc_ingain1;
	float fRecinjcr[2];
    float filebutton;
    float fwarn;

    // private constructor
    GxEngine() {}

    // private audio processing
    void process_buffers(int count, float** input, float** output);


    friend class GxMainInterface;

  public:
    // unique instance : use this instead of constructor
    static inline GxEngine* instance()
    {
      static GxEngine engine;
      return &engine;
    }

    static void metadata(Meta* m);

     int getNumInputs()
    {
      return 2;
    }
     int getNumOutputs()
    {
      return 2;
    }

    void initEngine(int samplingFreq);

    // wrap the state of the latency change warning (dis/enable) to
    // the interface settings to load and save it
    void set_latency_warning_change()
    {
      fwarn_swap = fwarn;
    }
    void get_latency_warning_change()
    {
      fwarn = fwarn_swap;
    }

    // zeroize an array of float using memset
    static void zeroize(int array[], int array_size)
    {
      (void)memset(array, 0, sizeof(array[0])*array_size);
    }

    static void zeroize(float array[], int array_size)
    {
      (void)memset(array, 0, sizeof(array[0])*array_size);
    }

    void  get_jconv_output(float **input,float **output,int sf);
    // main audio processing
    void compute (int count, float** input, float** output);

  };

  /* ------------------------------------------------------------------- */
} /* end of gx_engine namespace */
