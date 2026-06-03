/*
 * GOATTRACKER reSID interface
 */

#define GSID_C

#include <stdlib.h>
#include <stdio.h>
#include "resid-sid.h"
#include "residfp-sid.h"

extern "C" {

#include "gsid.h"
#include "gsound.h"
#include "../CGT2VoiceWaveforms.h"

int clockrate;
int samplerate;
unsigned char sidreg[NUMSIDREGS];
unsigned char sidorder[] =
  {0x18,0x17,0x16,0x15,
   0x14,0x13,0x12,0x11,0x10,0x0f,0x0e,
   0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,
   0x06,0x05,0x04,0x03,0x02,0x01,0x00};

unsigned char altsidorder[] =
  {0x00,0x01,0x02,0x03,0x04,0x05,0x06,
   0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,
   0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,
   0x15,0x16,0x17,0x18};

reSID::SID *sid = 0;
SIDFP *sidfp = 0;

// Per-voice output levels for GT2 mixer VU meters (updated after each clock)
float gt2_voice_level[3] = {0, 0, 0};
float gt2_voice_volume[3] = {1.0f, 1.0f, 1.0f};  // per-voice gain (0.0-1.0)
float gt2_master_volume = 1.0f;                     // master output gain (0.0-1.0)
unsigned char gt2_voice_mute[3] = {0, 0, 0};       // set by mixer to mute individual voices

FILTERPARAMS filterparams =
  {0.50f, 3.3e6f, 1.0e-4f,
   1147036.4394268463f, 274228796.97550374f, 1.0066634233403395f, 16125.154840564108f,
   5.5f, 20.f,
   0.9613160610660189f};

extern unsigned residdelay;
extern unsigned adparam;

static void gt2_resid_voice_sample_callback(void *userData, int voice0, int voice1, int voice2, short mix)
{
  (void)userData;
  c64d_gt2_capture_voice_samples(voice0, voice1, voice2, mix);
}

void sid_init(int speed, unsigned m, unsigned ntsc, unsigned interpolate, unsigned customclockrate, unsigned usefp)
{
  int c;

  if (ntsc) clockrate = NTSCCLOCKRATE;
    else clockrate = PALCLOCKRATE;

  if (customclockrate)
    clockrate = customclockrate;

  samplerate = speed;

  if (!usefp)
  {
    if (sidfp)
    {
      delete sidfp;
      sidfp = NULL;
    }

    if (!sid) sid = new reSID::SID();
    if (sid) sid->set_voice_sample_callback(gt2_resid_voice_sample_callback, NULL);
  }
  else
  {
    if (sid)
    {
      delete sid;
      sid = NULL;
    }
    
    if (!sidfp) sidfp = new SIDFP;
  }

  switch(interpolate)
  {
    case 0:
    if (sid) sid->set_sampling_parameters(clockrate, reSID::SAMPLE_FAST, speed);
    if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, speed);
    break;

    default:
    if (sid) sid->set_sampling_parameters(clockrate, reSID::SAMPLE_INTERPOLATE, speed);
    if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, speed);
    break;
  }

  if (sid) sid->reset();
  if (sidfp) sidfp->reset();
  for (c = 0; c < NUMSIDREGS; c++)
  {
    sidreg[c] = 0x00;
  }
  if (m == 1)
  {
    if (sid) sid->set_chip_model(reSID::MOS8580);
    if (sidfp) sidfp->set_chip_model(MOS8580FP);
  }
  else
  {
    if (sid) sid->set_chip_model(reSID::MOS6581);
    if (sidfp) sidfp->set_chip_model(MOS6581FP);
  }

  if (sidfp)
  {
	  // TODO: checkme
    sidfp->get_filter().set_distortion_properties(
      filterparams.distortionrate,
	  filterparams.distortionpoint); //,
      //filterparams.distortioncfthreshold);
    sidfp->get_filter().set_type3_properties(
      filterparams.type3baseresistance,
      filterparams.type3offset,
      filterparams.type3steepness,
      filterparams.type3minimumfetresistance);
    sidfp->get_filter().set_type4_properties(
      filterparams.type4k,
      filterparams.type4b);
    sidfp->set_voice_nonlinearity(
      filterparams.voicenonlinearity);
  }
}

unsigned char sid_getorder(unsigned char index)
{
  if (adparam >= 0xf000)
    return altsidorder[index];
  else
    return sidorder[index];
}

int sid_fillbuffer(short *ptr, int samples)
{
  int tdelta;
  int tdelta2;
  int result = 0;
  int total = 0;
  int c;

  short *ptrStart = ptr;

  // Apply per-voice gain and mute to reSID
  if (sid)
  {
    for (int v = 0; v < 3; v++)
    {
      sid->voice_gain[v] = gt2_voice_mute[v] ? 0.0f : gt2_voice_volume[v];
    }
    sid->update_voice_gain();
  }

  int badline = rand() % NUMSIDREGS;

  tdelta = clockrate * samples / samplerate;
  if (tdelta <= 0) return total;

  for (c = 0; c < NUMSIDREGS; c++)
  {
    unsigned char o = sid_getorder(c);

    // Possible random badline delay once per writing
    if ((badline == c) && (residdelay))
    {
      tdelta2 = residdelay;
      if (sid) result = sid->clock(tdelta2, ptr, samples);
      if (sidfp) result = sidfp->clock(tdelta2, ptr, samples);
      total += result;
      ptr += result;
      samples -= result;
      tdelta -= residdelay;
    }

    if (sid) sid->write(o, sidreg[o]);
    if (sidfp) sidfp->write(o, sidreg[o]);

    tdelta2 = SIDWRITEDELAY;
    if (sid) result = sid->clock(tdelta2, ptr, samples);
    if (sidfp) result = sidfp->clock(tdelta2, ptr, samples);
    total += result;
    ptr += result;
    samples -= result;
    tdelta -= SIDWRITEDELAY;

    if (tdelta <= 0) return total;
  }

  // Original batched render — the SID needs enough cycles per call to
  // actually produce samples, so per-sample clocking starved it and
  // killed audio. Per-voice oscilloscope samples are emitted from inside
  // reSID's sample loop via set_voice_sample_callback(), so the batch size
  // does not flatten the displayed voice waveforms.
  if (sid) result = sid->clock(tdelta, ptr, samples);
  if (sidfp) result = sidfp->clock(tdelta, ptr, samples);
  total += result;
  ptr += result;
  samples -= result;

  // Loop extra cycles until all samples produced
  while (samples)
  {
    tdelta = clockrate * samples / samplerate;
    if (tdelta <= 0) return total;

    if (sid) result = sid->clock(tdelta, ptr, samples);
    if (sidfp) result = sidfp->clock(tdelta, ptr, samples);
    total += result;
    ptr += result;
    samples -= result;
  }

  // Read per-voice output levels for VU meters
  if (sid)
  {
    for (int v = 0; v < 3; v++)
    {
      int voiceOut = sid->voice_output(v);
      // Voice::output() = (wave_dac - wave_zero) * envelope_dac
      // wave range ~2048, envelope range ~255 => max ~522240
      float level = (float)abs(voiceOut) / 522240.0f;
      if (level > gt2_voice_level[v])
        gt2_voice_level[v] = level;
      else
        gt2_voice_level[v] *= 0.92f;  // decay
    }

    // Restore voice gain after processing
    for (int v = 0; v < 3; v++)
      sid->voice_gain[v] = 1.0f;
    sid->update_voice_gain();
  }

  // Apply master volume to output buffer
  if (gt2_master_volume < 0.999f)
  {
    for (int s = 0; s < total; s++)
    {
      ptrStart[s] = (short)(ptrStart[s] * gt2_master_volume);
    }
  }

  return total;
}

// extern "C"
}
