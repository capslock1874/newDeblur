/* avi2mp2.c - 
  音频编码文件*/


#include <dos.h>
#include "common.h"
#include "encoder.h"
#include "global.h"
#include "video.h"

/* 定义全局变量 */

Bit_stream_struc   bs;
char             programName[] = "avi2mpg1";
int	channels;

FILE *hOutput;
unsigned long length, last_sample = 0;





/************************************************************************
/*
/* set_parms
/*
/* 功能:  根据命令行的输入为编码设置参数。如果命令行中没有指定，则使用缺省参数
/*
/************************************************************************/
 
void set_parms(fr_ps, num_samples, tempfile)
frame_params  *fr_ps;
unsigned long *num_samples;
char * tempfile;
{
   layer *info = fr_ps->header;
   int   err = 0, i = 0;
   int   file;
   
   info->lay = audio_layer_parm;
 
   if(pWavFormat.nChannels == 1)
   {
	    info->mode = MPG_MD_MONO;
		info->mode_ext = 0;
   }
   else
   {
		if(joint_stereo_parm == 1)
		{
			info->mode = MPG_MD_JOINT_STEREO;
		}
		else
		{
			info->mode = MPG_MD_STEREO;
			info->mode_ext = 0;
		}
   }

   if((info->sampling_frequency = SmpFrqIndex((long)(1000*DFLT_SFQ))) < 0)
   {
      fprintf(stderr, "%s: bad sfrq default %.2f\n", programName, DFLT_SFQ);
      abort();
   }
   if((info->bitrate_index = BitrateIndex(info->lay, audio_bitrate_parm)) < 0)
   {
      fprintf(stderr, "%s: bad default bitrate %u\n", programName, DFLT_BRT);
      abort();
   }
 
 switch(DFLT_EMP)
   {
      case 'n': info->emphasis = 0; break;
      case '5': info->emphasis = 1; break;
      case 'c': info->emphasis = 3; break;
      default: 
	 fprintf(stderr, "%s: Bad emph dflt %c\n", programName, DFLT_EMP);
	 abort();
   }
 
   info->copyright = 0; info->original = 0; info->error_protection = FALSE;
 
	length = sizeof(pWavFormat);

	channels = pWavFormat.nChannels; // store for later

	if ((pWavFormat.nSamplesPerSec != 11025)&&(pWavFormat.nSamplesPerSec != 22050)&&(pWavFormat.nSamplesPerSec != 44100))
	{
		fprintf(stderr, "Samples per Sec = %i\n", pWavFormat.nSamplesPerSec);
		fprintf(stderr, "input avi audio sample rate must = 11.025, 22.05, or 44.1 kHz!");
		exit(4);
	}

	if ((pWavFormat.wBitsPerSample != 8)&&(pWavFormat.wBitsPerSample != 16))
	{
		fprintf(stderr, "Bits per Sample = %i\n", pWavFormat.wBitsPerSample);
		fprintf(stderr, "input avi audio must be 8 or 16 bit!");
		exit(5);
	}

    
   open_bit_stream_w(&bs, tempfile, BUFFER_SIZE);

   *num_samples=0;
   for(file=0;file<numAviFiles;file++)
   {
      if(pWavFormat.nChannels == 2)
	     *num_samples += pAudioStreamInfo[file].dwLength * 2;
      else
	     *num_samples += pAudioStreamInfo[file].dwLength;
   }

   if(pWavFormat.nSamplesPerSec == 22050)
	   *num_samples = *num_samples * 2;

   if(pWavFormat.nSamplesPerSec == 11025)
	   *num_samples = *num_samples * 4;

}

 
/************************************************************************
/*
/* avi2mp2
/*
/* 功能 :  MPEG I编码器，支持层1和2以及心理声学模型2 (AT&T)
/*

/************************************************************************/

avi2mp2(tempfile)
char * tempfile;
{
typedef double SBS[2][3][SCALE_BLOCK][SBLIMIT];
    SBS        *sb_sample;
typedef double JSBS[3][SCALE_BLOCK][SBLIMIT];
    JSBS        *j_sample;
typedef double INN[2][HAN_SIZE];
    INN          *win_que;
typedef unsigned int SUB[2][3][SCALE_BLOCK][SBLIMIT];
    SUB         *subband;
 
    frame_params fr_ps;
    layer info;
    short **win_buf;
static short buffer[2][1152];
static unsigned int bit_alloc[2][SBLIMIT], scfsi[2][SBLIMIT];
static unsigned int scalar[2][3][SBLIMIT], j_scale[3][SBLIMIT];
static double ltmin[2][SBLIMIT], lgmin[2][SBLIMIT], max_sc[2][SBLIMIT];
    FLOAT snr32[32];
    short sam[2][1056];
    int whole_SpF, extra_slot = 0;
    double avg_slots_per_frame, frac_SpF, slot_lag;
    int stereo, error_protection;
static unsigned int crc;
    int i, j, k, adb;
    unsigned long bitsPerSlot, samplesPerFrame, frameNum = 0;
    unsigned long frameBits, sentBits = 0;
    unsigned long num_samples;

   

    sb_sample = (SBS *) mem_alloc(sizeof(SBS), "sb_sample");
    j_sample = (JSBS *) mem_alloc(sizeof(JSBS), "j_sample");
    win_que = (INN *) mem_alloc(sizeof(INN), "Win_que");
    subband = (SUB *) mem_alloc(sizeof(SUB),"subband");
    win_buf = (short **) mem_alloc(sizeof(short *)*2, "win_buf");
 
    memset((char *) buffer, 0, sizeof(buffer));
    memset((char *) bit_alloc, 0, sizeof(bit_alloc));
    memset((char *) scalar, 0, sizeof(scalar));
    memset((char *) j_scale, 0, sizeof(j_scale));
    memset((char *) scfsi, 0, sizeof(scfsi));
    memset((char *) ltmin, 0, sizeof(ltmin));
    memset((char *) lgmin, 0, sizeof(lgmin));
    memset((char *) max_sc, 0, sizeof(max_sc));
    memset((char *) snr32, 0, sizeof(snr32));
    memset((char *) sam, 0, sizeof(sam));

    fr_ps.header = &info;
    fr_ps.tab_num = -1;             
    fr_ps.alloc = NULL;
    info.version = MPEG_AUDIO_ID;

    set_parms(&fr_ps, &num_samples, tempfile);

	hdr_to_frps(&fr_ps);
    stereo = fr_ps.stereo;
    error_protection = info.error_protection;
 
    if (info.lay == 1) { bitsPerSlot = 32; samplesPerFrame = 384;  }
    else               { bitsPerSlot = 8;  samplesPerFrame = 1152; }
    
    avg_slots_per_frame = ((double)samplesPerFrame /
			   s_freq[info.sampling_frequency]) *
			  ((double)bitrate[info.lay-1][info.bitrate_index] /
			   (double)bitsPerSlot);
    whole_SpF = (int) avg_slots_per_frame;
    frac_SpF  = avg_slots_per_frame - (double)whole_SpF;
    slot_lag  = -frac_SpF;

 
    if (frac_SpF != 0)
	   {}
    else
		info.padding = 0;
 
    while (get_audio(buffer, num_samples, stereo, info.lay) > 0) {

		fprintf(stderr, "\r%5.1f%% complete", ((float)(num_samples - bytes_processed)/(float)num_samples)*100.0);

       win_buf[0] = &buffer[0][0];
       win_buf[1] = &buffer[1][0];
       if (frac_SpF != 0) {
	  if (slot_lag > (frac_SpF-1.0) ) {
	     slot_lag -= frac_SpF;
	     extra_slot = 0;
	     info.padding = 0;
	  }
	  else {
	     extra_slot = 1;
	     info.padding = 1;
	     slot_lag += (1-frac_SpF);
	  }
       }
       adb = (whole_SpF+extra_slot) * bitsPerSlot;

       switch (info.lay)
	   {
 
/***************************** Layer I **********************************/
 
	   case 1 :
	      for (j=0;j<SCALE_BLOCK;j++)
	         for (k=0;k<stereo;k++)
			 {
		         window_subband(&win_buf[k], &(*win_que)[k][0], k);
		         filter_subband(&(*win_que)[k][0], &(*sb_sample)[k][0][j][0]);
			 }

	      I_scale_factor_calc(*sb_sample, scalar, stereo);
	      if(fr_ps.actual_mode == MPG_MD_JOINT_STEREO)
		  {
		     I_combine_LR(*sb_sample, *j_sample);
		     I_scale_factor_calc(j_sample, &j_scale, 1);
		  }
 
	      put_scale(scalar, &fr_ps, max_sc);
 
		  for (k=0;k<stereo;k++)
		  {
		      psycho_anal(&buffer[k][0],&sam[k][0], k, info.lay, snr32,
			              (FLOAT)s_freq[info.sampling_frequency]*1000);
		      for (i=0;i<SBLIMIT;i++)
				 ltmin[k][i] = (double) snr32[i];
		  }
 
	      I_main_bit_allocation(ltmin, bit_alloc, &adb, &fr_ps);
 
	      if (error_protection)
			 I_CRC_calc(&fr_ps, bit_alloc, &crc);
 
	      encode_info(&fr_ps, &bs);
 
	      if (error_protection)
			 encode_CRC(crc, &bs);
 
	      I_encode_bit_alloc(bit_alloc, &fr_ps, &bs);
	      I_encode_scale(scalar, bit_alloc, &fr_ps, &bs);
	      I_subband_quantization(scalar, *sb_sample, j_scale, *j_sample,
				    bit_alloc, *subband, &fr_ps);
	      I_sample_encoding(*subband, bit_alloc, &fr_ps, &bs);
	      for (i=0;i<adb;i++)
			 put1bit(&bs, 0);
	  break;
 
/***************************** Layer 2 **********************************/
 
	  case 2 :
		for (i=0;i<3;i++)
			 for (j=0;j<SCALE_BLOCK;j++)
		         for (k=0;k<stereo;k++)
				 {
		             window_subband(&win_buf[k], &(*win_que)[k][0], k);
		             filter_subband(&(*win_que)[k][0], &(*sb_sample)[k][i][j][0]);
				 }
 
		         II_scale_factor_calc(*sb_sample, scalar, stereo, fr_ps.sblimit);
		         pick_scale(scalar, &fr_ps, max_sc);
		         if(fr_ps.actual_mode == MPG_MD_JOINT_STEREO)
				 {
		             II_combine_LR(*sb_sample, *j_sample, fr_ps.sblimit);
		             II_scale_factor_calc(j_sample, &j_scale, 1, fr_ps.sblimit);
				 }       /* this way we calculate more mono than we need */
		            	 /* but it is cheap */
 
		         for (k=0;k<stereo;k++)
				 {
		             psycho_anal(&buffer[k][0],&sam[k][0], k, 
				                 info.lay, snr32,
				                 (FLOAT)s_freq[info.sampling_frequency]*1000);
		             for (i=0;i<SBLIMIT;i++)
				          ltmin[k][i] = (double) snr32[i];
				 }

		     II_transmission_pattern(scalar, scfsi, &fr_ps);
		     II_main_bit_allocation(ltmin, scfsi, bit_alloc, &adb, &fr_ps);
 
		     if (error_protection)
		         II_CRC_calc(&fr_ps, bit_alloc, scfsi, &crc);
 
		     encode_info(&fr_ps, &bs);
 
		     if (error_protection)
				 encode_CRC(crc, &bs);
 
		     II_encode_bit_alloc(bit_alloc, &fr_ps, &bs);
		     II_encode_scale(bit_alloc, scfsi, scalar, &fr_ps, &bs);
		     II_subband_quantization(scalar, *sb_sample, j_scale,
				      *j_sample, bit_alloc, *subband, &fr_ps);
		     II_sample_encoding(*subband, bit_alloc, &fr_ps, &bs);
		     for (i=0;i<adb;i++)
				 put1bit(&bs, 0);
	  break;
 
      }
      frameBits = sstell(&bs) - sentBits;
      if(frameBits%bitsPerSlot)   
		fprintf(stderr,"Sent %ld bits = %ld slots plus %ld\n",
				frameBits, frameBits/bitsPerSlot,
				frameBits%bitsPerSlot);
      sentBits += frameBits;

    }

    close_bit_stream_w(&bs);

    

    
    return(0);
}
 

