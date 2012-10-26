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
 *    This is the Jc_Gui Audio Engine
 *
 *
 * --------------------------------------------------------------------------
 */


void GxEngine::get_jconv_output(float **input,float **output,int sf)
{
  float*  input1 = input[2];
  float*  input2 = input[3];
  float*  out1 = output[0];
  float*  out2 = output[1];

  for (int i=0; i<sf; i++)
    {
      *out1++ +=  *input1++ ;
      *out2++ +=  *input2++ ;
    }

}

//==============================================================================
//
//             this is the process callback called from jack
//
//==============================================================================
void GxEngine::compute (int count, float** input, float** output)
{
  // retrieve engine state
  const GxEngineState estate = (GxEngineState)checky;

  //------------ determine processing type
  unsigned short process_type = ZEROIZE_BUFFERS;

  if (gx_jack::NO_CONNECTION == 0) // ports connected
    {
      switch (estate)
        {
        case kEngineOn:
          process_type = PROCESS_BUFFERS;
          break;
        default: // engine off or whatever: zeroize
          break;
        }
    }

  switch (process_type)
    {

    case PROCESS_BUFFERS:
      process_buffers(count, input, output);
      break;

      // ------- zeroize buffers
    case ZEROIZE_BUFFERS:
    default:

      // no need of loop.
      // You will avoid triggering an if statement for each frame
      (void)memset(output[0], 0, count*sizeof(float));
      (void)memset(output[1], 0, count*sizeof(float));

      // only when jconv is running
      if (gx_jconv::jconv_is_running)
        {
          (void)memset(output[2], 0, count*sizeof(float));
          (void)memset(output[3], 0, count*sizeof(float));
        }

      break;
    }


}


//======== private method: process buffers on demand
void GxEngine::process_buffers(int count, float** input, float** output)
{
  // precalculate values with need update peer frame


  float fSlow72 = (9.999871e-04f * powf(10, (5.000000e-02f * fslider17)));

  float fSlow81 = fslider24;
  float fSlow82 = (1 - max(0, (0 - fSlow81)));
  float fSlow83 = fslider25;
  float fSlow84 = (1 - max(0, fSlow83));
  float fSlow85 = (fSlow84 * fSlow82);
  float fSlow86 = (1 - max(0, fSlow81));
  float fSlow87 = (fSlow84 * fSlow86);
  float fSlow89 = (1 - max(0, (0 - fSlow83)));
  float fSlow90 = (fSlow89 * fSlow82);
  float fSlow91 = (fSlow89 * fSlow86);
  float fSlowinjc = (9.999871e-04f * powf(10, (5.000000e-02f * fjc_ingain)));
  float fSlowinjcr = (9.999871e-04f * powf(10, (5.000000e-02f * fjc_ingain1)));



  //----- tone only reset when value have change
  fslider_tone_check1 = (fslider_tone1+fslider_tone0+fslider_tone2)*100;
  if (fslider_tone_check1 != fslider_tone_check)
    {
      fSlow_mid_tone = (fslider_tone1*0.5);
      fSlow_tone0 = powf(10, (2.500000e-02f * (fslider_tone0- fSlow_mid_tone)));
      fSlow_tone1 = (1 + fSlow_tone0);
      fSlow_tone2 = (fConst_tone1 * fSlow_tone1);
      fSlow_tone3 = (2 * (0 - ((1 + fSlow_tone2) - fSlow_tone0)));
      fSlow_tone4 = (fConst_tone1 * (fSlow_tone0 - 1));
      fSlow_tone5 = (fConst_tone2 * sqrtf(fSlow_tone0));
      fSlow_tone6 = (fSlow_tone1 - (fSlow_tone5 + fSlow_tone4));
      fSlow_tone7 = powf(10, (2.500000e-02f * fSlow_mid_tone));
      fSlow_tone8 = (1 + fSlow_tone7);
      fSlow_tone9 = (fConst_tone4 * fSlow_tone8);
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
    }
  // tone end



  int iSlow88 = int(gx_jconv::checkbox7);

  int 	iSlowdel0 = int((int((fConstdel0 * fsliderdel0)) & 262143));
  int 	iSlowdel1 = int((int((fConstdel0 * fsliderdel1)) & 262143));


  // pointer to the jack_buffer
  float*  input0 = input[0];
  float*  input1 = input[1];
  // pointers to the jack_output_buffers
  float* output0 = output[2];
  float* output1 = output[0];
  float* output2 = output[3];
  float* output3 = output[1];
  register  float fTemp0 = input0[0];
  register  float fTemp1 = input1[0];

  // start the inner loop count = jack_frame
  for (int i=0; i<count; i++)
    {

      fTemp0 = *input0++ + 1e-20;
      fTemp1 = *input1++ + 1e-20;


      // tone
      fVec_tone0[0] = fTemp0;
      fRec_tone3[0] = (fSlow_tone32 * ((fSlow_tone21 * ((fSlow_tone31 * fVec_tone0[2]) + ((fSlow_tone30 * fVec_tone0[0]) + (fSlow_tone28 * fVec_tone0[1])))) - ((fSlow_tone27 * fRec_tone3[2]) + (fSlow_tone24 * fRec_tone3[1]))));
      fRec_tone2[0] = (fSlow_tone37 * ((fSlow_tone7 * (((fSlow_tone36 * fRec_tone3[0]) + (fSlow_tone34 * fRec_tone3[1])) + (fSlow_tone33 * fRec_tone3[2]))) - ((fSlow_tone20 * fRec_tone2[2]) + (fSlow_tone17 * fRec_tone2[1]))));
      fRec_tone1[0] = (fSlow_tone42 * ((((fSlow_tone41 * fRec_tone2[1]) + (fSlow_tone40 * fRec_tone2[0])) + (fSlow_tone38 * fRec_tone2[2])) + (0 - ((fSlow_tone15 * fRec_tone1[2]) + (fSlow_tone10 * fRec_tone1[1])))));
      fRec_tone0[0] = (fSlow_tone47 * ((((fSlow_tone46 * fRec_tone1[1]) + (fSlow_tone45 * fRec_tone1[0])) + (fSlow_tone43 * fRec_tone1[2])) + (0 - ((fSlow_tone6 * fRec_tone0[2]) + (fSlow_tone3 * fRec_tone0[1])))));
      // tone end

      fTemp0 = fRec_tone0[0];

      // tone
      fVec_ltone0[0] = fTemp1;
      fRec_ltone3[0] = (fSlow_tone32 * ((fSlow_tone21 * ((fSlow_tone31 * fVec_ltone0[2]) + ((fSlow_tone30 * fVec_ltone0[0]) + (fSlow_tone28 * fVec_ltone0[1])))) - ((fSlow_tone27 * fRec_ltone3[2]) + (fSlow_tone24 * fRec_ltone3[1]))));
      fRec_ltone2[0] = (fSlow_tone37 * ((fSlow_tone7 * (((fSlow_tone36 * fRec_ltone3[0]) + (fSlow_tone34 * fRec_ltone3[1])) + (fSlow_tone33 * fRec_ltone3[2]))) - ((fSlow_tone20 * fRec_ltone2[2]) + (fSlow_tone17 * fRec_ltone2[1]))));
      fRec_ltone1[0] = (fSlow_tone42 * ((((fSlow_tone41 * fRec_ltone2[1]) + (fSlow_tone40 * fRec_ltone2[0])) + (fSlow_tone38 * fRec_ltone2[2])) + (0 - ((fSlow_tone15 * fRec_ltone1[2]) + (fSlow_tone10 * fRec_ltone1[1])))));
      fRec_ltone0[0] = (fSlow_tone47 * ((((fSlow_tone46 * fRec_ltone1[1]) + (fSlow_tone45 * fRec_ltone1[0])) + (fSlow_tone43 * fRec_ltone1[2])) + (0 - ((fSlow_tone6 * fRec_ltone0[2]) + (fSlow_tone3 * fRec_ltone0[1])))));
      // tone end

      fTemp1 = fRec_ltone0[0];

      // gain out right
      fRec46[0] = (fSlow72 + (0.999f * fRec46[1]));
      fTemp0 =  (fRec46[0] * fTemp0);

      // gain out left
      fRec47[0] = (fSlow72 + (0.999f * fRec47[1]));
      fTemp1 =  (fRec47[0] * fTemp1);

      // the left output port
      float 	S9[2];
      S9[0] = (fSlow87 * fTemp0);
      S9[1] = (fSlow84 * fTemp0);
      *output1++ = S9[iSlow88];

      // the right output port
      float 	S10[2];
      S10[0] = (fSlow91 *fTemp1 );
      S10[1] = (fSlow89 *fTemp1 );
      *output3++ = S10[iSlow88];

      if (gx_jconv::jconv_is_running)
        {
          // delay to jconv
          fVecdel0[IOTAdel&262143] = fTemp0;
          float out_to_jc1 = fVecdel0[(IOTAdel-iSlowdel0)&262143];
          fVecdel1[IOTAdel&262143] = fTemp1;
          float out_to_jc2 = fVecdel1[(IOTAdel-iSlowdel1)&262143];

          // gain to jconv
          fRecinjc[0] = (fSlowinjc + (0.999f * fRecinjc[1]));
          fRecinjcr[0] = (fSlowinjcr + (0.999f * fRecinjcr[1]));
          // this is the left "extra" port to run jconv in bybass mode
          *output0++ = (fSlow85 * out_to_jc1* fRecinjc[0]);
          // this is the right "extra" port to run jconv in bybass mode
          *output2++ = (fSlow90 * out_to_jc2* fRecinjcr[0]);
        }

      // post processing


      fRec46[1] = fRec46[0];
      fRec47[1] = fRec47[0];
      //----- tone
      fRec_tone0[2] = fRec_tone0[1];
      fRec_tone0[1] = fRec_tone0[0];
      fRec_tone1[2] = fRec_tone1[1];
      fRec_tone1[1] = fRec_tone1[0];
      fRec_tone2[2] = fRec_tone2[1];
      fRec_tone2[1] = fRec_tone2[0];
      fRec_tone3[2] = fRec_tone3[1];
      fRec_tone3[1] = fRec_tone3[0];
      fVec_tone0[2] = fVec_tone0[1];
      fVec_tone0[1] = fVec_tone0[0];
      // tone end
      //----- tone
      fRec_ltone0[2] = fRec_ltone0[1];
      fRec_ltone0[1] = fRec_ltone0[0];
      fRec_ltone1[2] = fRec_ltone1[1];
      fRec_ltone1[1] = fRec_ltone1[0];
      fRec_ltone2[2] = fRec_ltone2[1];
      fRec_ltone2[1] = fRec_ltone2[0];
      fRec_ltone3[2] = fRec_ltone3[1];
      fRec_ltone3[1] = fRec_ltone3[0];
      fVec_ltone0[2] = fVec_ltone0[1];
      fVec_ltone0[1] = fVec_ltone0[0];
      // tone end

      fRecinjc[1] = fRecinjc[0];
      fRecinjcr[1] = fRecinjcr[0];

      IOTAdel = IOTAdel+1;

    }
  output1 = output[0];
  output3 = output[1];
  (void)memcpy(get_frame, output1, sizeof(float)*count);
  (void)memcpy(get_frame1, output3, sizeof(float)*count);

}


