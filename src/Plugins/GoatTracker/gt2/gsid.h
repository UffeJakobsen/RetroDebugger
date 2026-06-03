#ifndef GSID_H
#define GSID_H

#define NUMSIDREGS 0x19
#define SIDWRITEDELAY 14 // lda $xxxx,x 4 cycles, sta $d400,x 5 cycles, dex 2 cycles, bpl 3 cycles

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  float distortionrate;
  float distortionpoint;
  float distortioncfthreshold;
  float type3baseresistance;
  float type3offset;
  float type3steepness;
  float type3minimumfetresistance;
  float type4k;
  float type4b;
  float voicenonlinearity;
} FILTERPARAMS;

void sid_init(int speed, unsigned m, unsigned ntsc, unsigned interpolate, unsigned customclockrate, unsigned usefp);
int sid_fillbuffer(short *ptr, int samples);
unsigned char sid_getorder(unsigned char index);

#ifndef GSID_C
extern unsigned char sidreg[NUMSIDREGS];
extern FILTERPARAMS filterparams;
#endif

// Per-voice levels, volume, and mute flags for GT2 mixer
extern float gt2_voice_level[3];
extern float gt2_voice_volume[3];   // 0.0-1.0, per-voice gain applied in SID
extern float gt2_master_volume;     // 0.0-1.0, scales final output
extern unsigned char gt2_voice_mute[3];

#ifdef __cplusplus
}
#endif

#endif
