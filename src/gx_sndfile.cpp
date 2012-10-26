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

/**************************************************************
	gx_sndfile.cpp

***************************************************************/

// ------------ This is the gx_sdnfile namespace ---------------
// This namespace mainly wraps around sndfile's functions
// And has some native functionality

#include <cstring>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <assert.h>

using namespace std;

#include <libgen.h>
#include <sndfile.h>
#include <jack/jack.h>
#include <gtk/gtk.h>
#include "Jc_Gui.h"

using namespace gx_system;

namespace gx_sndfile
{

// copyed gcd from (zita) resampler.cc to get ratio_a and ratio_b for
// calculate the correct buffer size resulting from resample
    static unsigned int gcd (unsigned int a, unsigned int b)
    {
        if (a == 0) return b;
        if (b == 0) return a;
        while (1)
        {
        if (a > b)
        {
            a = a % b;
            if (a == 0) return b;
            if (a == 1) return 1;
        }
        else
        {
            b = b % a;
            if (b == 0) return a;
            if (b == 1) return 1;
        }
        }    
        return 1; 
    }

    float *BufferResampler::process(int fs_inp, int ilen, float *input, int fs_outp, int *olen, int chan)
    {
        int d = gcd(fs_inp, fs_outp);
        int ratio_a = fs_inp / d;
        int ratio_b = fs_outp / d;
        
        const int qual = 32;
        if (setup(fs_inp, fs_outp, chan, qual) != 0) {
            return 0;
        }
        // pre-fill with k/2-1 zeros
        int k = inpsize();
        inp_count = k/2-1;
        inp_data = 0;
        out_count = 1; // must be at least 1 to get going
        out_data = 0;
        if (Resampler::process() != 0) {
            return 0;
        }
        inp_count = ilen;
        int nout = out_count = (ilen * ratio_b + ratio_a - 1) / ratio_a;
        inp_data = input;
        float *p = out_data = new float[out_count*chan];
        if (Resampler::process() != 0) {
            delete p;
            return 0;
        }
        inp_data = 0;
        inp_count = k/2;
        if (Resampler::process() != 0) {
            delete p;
            return 0;
        }
        assert(inp_count == 0);
        assert(out_count <= 1);
        *olen = nout - out_count;
        return p;
    }

    class CheckResample {
    private:
        float *vec;
        BufferResampler& resamp;
    public:
        CheckResample(BufferResampler& resamp_): vec(0), resamp(resamp_) {}
        float *resample(int *count, float *impresp, unsigned int imprate, unsigned int samplerate, int chan, GxResampleStatus *status) {
        if (imprate != samplerate) {
            vec = resamp.process(imprate, *count, impresp, samplerate, count, chan);
            if (!vec) {
                ostringstream msg;
                msg << "Sorry, resampling failed " ;
                (void)gx_gui::gx_message_popup(msg.str().c_str());
                *status = kErrorResample;
                return 0;
            }
            return vec;
        }
            return impresp;
        }
        ~CheckResample() {
        if (vec) {
            vec = NULL;
            delete vec;
        }
        }
    };


  // --------------- sf_open writer wrapper : returns file desc. and audio file info
  SNDFILE* openOutputSoundFile(const char* name, int chans, int sr, int format)
  {
    
    // initialise the SF_INFO structure
    SF_INFO info;

    info.samplerate = sr;
    info.channels   = chans;
    if (format == 0)
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16 ;
    else if(format == 16)
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16 ;
    else if(format == 24)
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_24 ;
    else if(format == 32)
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_32 ;

    return sf_open(name, SFM_WRITE, &info);
  }

  // --------------- sf_open reader wrapper : returns file desc. and audio file info
  SNDFILE* openInputSoundFile(const char* name, int* chans, int* sr, int* length)
  {
    SF_INFO info;
    SNDFILE *sf = sf_open(name, SFM_READ, &info);

    *sr     = info.samplerate;
    *chans  = info.channels;
    *length = info.frames;

    return sf;
  }
  
  // --------------- sf_open reader wrapper : returns file desc. and audio file info
  SNDFILE* openInfoSoundFile(const char* name, int* chans, int* sr, int* length, int* format)
  {
    SF_INFO info;
    SNDFILE *sf = sf_open(name, SFM_READ, &info);

    *sr     = info.samplerate;
    *chans  = info.channels;
    *length = info.frames;
    
    switch (info.format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_16:
        *format = 16;
        break;
    case SF_FORMAT_PCM_24:
        *format = 24;
        break;
    case SF_FORMAT_PCM_32:
        *format = 32;
        break;
    default :
        *format = 0;
        break;
    }
    
    return sf;
  }

  // --------------- sf_writer wrapper
  sf_count_t writeSoundOutput(SNDFILE *pOutput, float *buffer, int vecsize)
  {
    return sf_writef_float(pOutput, buffer, vecsize);
  }

  // --------------- sf_reader wrapper
  sf_count_t readSoundInput(SNDFILE *pInput, float *buffer, int vecsize)
  {
    return sf_readf_float(pInput, buffer, vecsize);
  }

  // --------------- audio resampler
  GxResampleStatus resampleSoundFile(const char*  pInputi,
				     const char*  pOutputi,
				     int jackframe)
  {
    int chans, length2=0, sr, format;
    GxResampleStatus status = kNoError;

    BufferResampler resamp;
    CheckResample r(resamp);

    // --- open audio files
    SNDFILE* pInput  = openInfoSoundFile (pInputi,  &chans, &sr, &length2, &format);
    SNDFILE* pOutput = openOutputSoundFile(pOutputi, chans, jackframe, format);

    // check input
    if (!pInput)
    {
      ostringstream msg;
      msg << "Error opening input file " << pInputi;
      (void)gx_gui::gx_message_popup(msg.str().c_str());
      status = kErrorInput;
    }

    // check input
    else if (!pOutput)
    {
      ostringstream msg;
      msg << "Error opening output file " << pOutputi;
      (void)gx_gui::gx_message_popup(msg.str().c_str());
      status = kErrorOutput;
    }
    else
    {
      // get buffersize for resampling
      int	vecsize=length2;
      int d = gcd(sr, jackframe);
      int ratio_a = sr / d;
      int ratio_b = jackframe / d;
      int nout =  (vecsize * ratio_b + ratio_a - 1) / ratio_a;
      if(nout < vecsize) nout = vecsize;
      // setup buffer for resampler
      float* sig = new float[nout*chans+2];
      (void)memset(sig,  0, (nout*chans+2)*sizeof(float));
      // do it
	  readSoundInput   (pInput,  sig, vecsize);
      sig = r.resample(&vecsize, sig, sr, jackframe, chans, &status);
      if (!status)
	  writeSoundOutput (pOutput, sig, nout);

      // close files
      sf_close(pInput);
      sf_close(pOutput);
      // delete buffer
      delete[] sig;
    }

    return status;
  }

  // --------------- sf_close wrapper
  void closeSoundFile(SNDFILE *psf_in)
  {
    sf_close(psf_in);
  }
} /* end of gx_sndfile namespace */
