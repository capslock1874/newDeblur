
 
#include <stdio.h>
#include "global.h"
#include "common.h"
#include "encoder.h"

#include "enwindow.h"
#include "video.h"

extern unsigned _stklen = 16384;

extern FILE *hOutput;
extern unsigned long length, last_sample;
extern int channels;
 
void filter(short *work_buffer, unsigned int length, unsigned int stereo, unsigned int mult);

/* 低通滤波程序*/
void low_pass( buffer, length, stereo, mult)
short *buffer;
unsigned int length, stereo, mult;
{
	short work_buffer[PCM_BUFFER + 8];
	unsigned int i;
    static char init = TRUE;
	short pass1[8], pass2[8], pass3[8], pass4[8];

    if (init)
	{
        for(i=0; i<8; i++)
		{
			work_buffer[i]=0; 
			pass1[i]=pass2[i]=pass3[i]=pass4[i]=0;
		}
        init = FALSE;
    }
	
	for(i=0; i<length; i++)
		work_buffer[i+8] = buffer[i]; // 将采样复制到工作缓存

	
	for(i=0; i<8; i++)
		work_buffer[i] = pass1[i];

	
	for(i=0; i<8; i++)
		pass1[i] = buffer[length - 8 + i];

	filter( work_buffer, length, stereo, mult); //采用1阶滤波器



	for(i=0; i<8; i++)
		work_buffer[i] = pass2[i];
	for(i=0; i<8; i++)
		pass2[i] = buffer[length - 8 + i];
	filter( work_buffer, length, stereo, mult); 

	for(i=0; i<8; i++)
		work_buffer[i] = pass3[i];
	for(i=0; i<8; i++)
		pass3[i] = buffer[length - 8 + i];
	filter( work_buffer, length, stereo, mult); 

	for(i=0; i<8; i++)
		work_buffer[i] = pass4[i];
	for(i=0; i<8; i++)
		pass4[i] = buffer[length - 8 + i];
	filter( work_buffer, length, stereo, mult); 

	for(i=0; i<length; i++)
		buffer[i] = work_buffer [i+8];
}

void filter(work_buffer, length, stereo, mult)
short *work_buffer;
unsigned int length, stereo, mult;
{
	short temp_buffer[PCM_BUFFER];
	unsigned int i;

	if(stereo)
	{
		if(mult == 2)
		{
			//立体声22050
			for(i=0; i<length; i++)
				temp_buffer[i] = (short)(0.612626*work_buffer[i+8] +
								 0.375311*work_buffer[i+6]);
		}
		else
		{
			//立体声11025
			for(i=0; i<length; i++)
				temp_buffer[i] = (short)(0.516851*work_buffer[i+8] +
								 0.267135*work_buffer[i+6] +
								 0.138069*work_buffer[i+4] +
								 0.071361*work_buffer[i+2]);
		}
	}
	else // 单声道
	{
		if(mult == 2)
		{
			//单声道22050
			for (i=0; i<length; i++)
				temp_buffer[i] = (short)(0.612626*work_buffer[i+8] +
								 0.375311*work_buffer[i+7]);
		}
		else
		{
			//单声道11025
			for (i=0; i<length; i++)
				temp_buffer[i] = (short)(0.516851*work_buffer[i+8] +
								 0.267135*work_buffer[i+7] +
								 0.138069*work_buffer[i+6] +
								 0.071361*work_buffer[i+5]);
		}
	}
	for(i=0; i<length; i++)
		work_buffer[i+8] = temp_buffer[i];
}

/************************************************************************/
/*
/* read_samples()
/*
/* 功能: 从avi文件中读取PCM采样到缓存
/*
/* 限制条件: 仅适用于未压缩的在11.025, 22.05, and 44.1Khz采样率下的
/*            8 和 16 bit音频 at
/*            
/*
/* 结构描述:
/* 从#pAudioStream#制定的avi文件中读取#samples_read#个片断。如果为8bit则
/*   将其扩展到16bit。如果采样率为11.025或22.05Khz，则将采样复制并进行低通
/*   滤波。最后返回读取的采样数。
  
 
/************************************************************************/

unsigned long read_samples(sample_buffer, num_samples, frame_size)
short sample_buffer[PCM_BUFFER];
unsigned long num_samples, frame_size;
{
    unsigned long samples_read, written, nsamples;
    static unsigned long samples_to_read, avi_samples;
    static char init = TRUE;
	int result;
	unsigned int i;
	short *dest, *source;
	unsigned char *csource;
    int file;
    long file_start=0;

    if (init)
	{
        samples_to_read = num_samples;
        init = FALSE;
    }

	if(bytes_processed == num_samples)
	{
		printf("\nInternal error, bytes_processed = num_samples!!!\n");
		exit(-99); 
	}
	
    if (samples_to_read >= frame_size)
        samples_read = frame_size;
    else
        samples_read = samples_to_read;

	if (channels == 2)
		avi_samples = samples_read/2; 
	else
		avi_samples = samples_read;

	if(pWavFormat.nSamplesPerSec == 22050)
		avi_samples = avi_samples/2; 

	if(pWavFormat.nSamplesPerSec == 11025)
		avi_samples = avi_samples/4; 

    for(file=0;file<numAviFiles;file++)
    {
        if (last_sample < nextFileSamples[file])
            break;
        file_start = nextFileSamples[file];
    }

    result=-1; 
    if (avi_samples==0)
        nsamples=0;
    else if ((last_sample + avi_samples) <= nextFileSamples[file])
        result = AVIStreamRead(pAudioStreams[file],       last_sample-file_start, avi_samples,
            sample_buffer,                   (PCM_BUFFER)*sizeof(short), &written, &nsamples);
    else
    {
        long want_samples1 = nextFileSamples[file] - last_sample;
        long want_samples2 = avi_samples - want_samples1;
        long got_samples1, got_samples2;
        result = AVIStreamRead(pAudioStreams[file],     last_sample-file_start, want_samples1,
          sample_buffer,                  (PCM_BUFFER)*sizeof(short), &written, &got_samples1);
        result = AVIStreamRead(pAudioStreams[file + 1], 0,                      want_samples2,
          &sample_buffer[want_samples1],  (PCM_BUFFER)*sizeof(short), &written, &got_samples2);
        nsamples = got_samples1+got_samples2;
    }
    
    last_sample = last_sample + nsamples;

    if (avi_samples != nsamples)
	{
		if(!fake_bad_frames)
		{
			fprintf(stderr, "\nHit end of audio data, audio length does not match video!\n");
			fprintf(stderr, "avi file may be corrupt, try -e option.\n");
			exit(23);
		}
		else
		{
			csource = (unsigned char*)sample_buffer;
			for(i = nsamples; i < avi_samples; i++)
			{
				bad_audio_count++;
				if(pWavFormat.wBitsPerSample == 8)
				{
					*csource++ = 128;
					i++;
				}
				else
				{
					*csource++ = 0;
					*csource++ = 0;
				}
			}
		}
	}


    if(samples_to_read&&!result)
    {
      samples_to_read -= samples_read;

	    if(pWavFormat.wBitsPerSample == 8)
	    {
		    dest = sample_buffer + written - 1;
		    csource = ((unsigned char*)(sample_buffer)) + written - 1;
		    for(i = 0; i < written; i++)
			    *dest-- = (*csource-- - 128) << 8;
		    written = written * 2;
	    }

	    if((pWavFormat.nSamplesPerSec == 22050)&&(channels == 1))
	    {
		    dest = sample_buffer + PCM_BUFFER - 1;
		    source = sample_buffer + PCM_BUFFER/2 - 1;
		    for(i = 0; i < PCM_BUFFER/2; i++)
		    {
			    *dest-- = *source;
			    *dest-- = *source--;
		    }
		    low_pass(sample_buffer, samples_read, 0, 2);
	    }

	    if((pWavFormat.nSamplesPerSec == 22050)&&(channels == 2))
	    {
		    dest = sample_buffer + PCM_BUFFER - 1;
		    source = sample_buffer + PCM_BUFFER/2 - 1;
		    for(i = 0; i < PCM_BUFFER/4; i++)
		    {
			    *dest = *source;
			    *(dest - 2) = *source--;
			    dest--;
			    *dest = *source;
			    *(dest - 2) = *source--;
			    dest = dest - 3;
		    }
		    low_pass(sample_buffer, samples_read, 1, 2);
	    }

	    if((pWavFormat.nSamplesPerSec == 11025)&&(channels == 1))
	    {
		    dest = sample_buffer + PCM_BUFFER - 1;
		    source = sample_buffer + PCM_BUFFER/4 - 1;
		    for(i = 0; i < PCM_BUFFER/4; i++)
		    {
			    *dest-- = *source;
			    *dest-- = *source;
			    *dest-- = *source;
			    *dest-- = *source--;
		    }
		    low_pass(sample_buffer, samples_read, 0, 4);
	    }

	    if((pWavFormat.nSamplesPerSec == 11025)&&(channels == 2))
	    {
		    dest = sample_buffer + PCM_BUFFER - 1;
		    source = sample_buffer + PCM_BUFFER/4 - 1;
		    for(i = 0; i < PCM_BUFFER/8; i++)
		    {
			    *dest = *source;
			    *(dest - 2) = *source;
			    *(dest - 4) = *source;
			    *(dest - 6) = *source--;
			    dest--;
			    *dest = *source;
			    *(dest - 2) = *source;
			    *(dest - 4) = *source;
			    *(dest - 6) = *source--;
			    dest = dest - 7;
		    }
		    low_pass(sample_buffer, samples_read, 1, 4);
	    }

        if (samples_read < frame_size && samples_read > 0)
	    {
            for (; samples_read < frame_size; sample_buffer[samples_read++] = 0);
            samples_to_read = 0;
        }

	    bytes_processed = samples_to_read;

        return(samples_read);
    }
    else
    return(0);  
}

/************************************************************************/
/*
/* get_audio()
/*
/* 功能:  从一个文件中读取一帧音频数据到缓存区
/*        将数据排序以备后面的处理，并将其分成左右声道
/*

/*
/************************************************************************/
 
unsigned long get_audio(buffer, num_samples, stereo, lay)
short buffer[2][1152];
unsigned long num_samples;
int stereo, lay;
{
   int j;
   short insamp[2304];
   unsigned long samples_read;
 
   if (lay == 1)
   {
      if(stereo == 2)
	  { /* layer 1, stereo */
         samples_read = read_samples(insamp, num_samples,
                                     (unsigned long) 768);
         for(j=0;j<448;j++)
		 {
            if(j<64)
			{
               buffer[0][j] = buffer[0][j+384];
               buffer[1][j] = buffer[1][j+384];
            }
            else
			{
               buffer[0][j] = insamp[2*j-128];
               buffer[1][j] = insamp[2*j-127];
            }
         }
      }
      else
	  { /* layer 1, mono */
         samples_read = read_samples(insamp, num_samples,
                                     (unsigned long) 384);
         for(j=0;j<448;j++)
		 {
            if(j<64)
			{
               buffer[0][j] = buffer[0][j+384];
               buffer[1][j] = 0;
            }
            else
			{
               buffer[0][j] = insamp[j-64];
               buffer[1][j] = 0;
            }
         }
      }
   }
   else {
      if(stereo == 2)
	  { /* layer 2 (or 3), stereo */
         samples_read = read_samples(insamp, num_samples,
                                     (unsigned long) 2304);
         for(j=0;j<1152;j++)
		 {
            buffer[0][j] = insamp[2*j];
            buffer[1][j] = insamp[2*j+1];
         }
      }
      else
	  { /* layer 2 (or 3), mono */
         samples_read = read_samples(insamp, num_samples,
                                     (unsigned long) 1152);
         for(j=0;j<1152;j++)
		 {
            buffer[0][j] = insamp[j];
            buffer[1][j] = 0;
         }
      }
   }
   return(samples_read);
}
 
/************************************************************************/
/*
/* read_ana_window()
/*
/* 功能:  将编码器窗文件 "enwindow" 读入数组#ana_win#
/*
/*
/************************************************************************/
 
void read_ana_window(ana_win)
double ana_win[HAN_SIZE];
{
    int i;
	for(i=0; i<512; i++)
	{
		ana_win[i] = enwindow[i];
	}
}

/************************************************************************/
/*
/* window_subband()
/*
/* 功能:  对PCM采样重叠窗口
/*

/************************************************************************/
 
void window_subband(buffer, z, k)
short **buffer;
double z[HAN_SIZE];
int k;
{
    typedef double XX[2][HAN_SIZE];
    static XX *x;
    int i, j;
    static off[2] = {0,0};
    static char init = 0;
    static double *c;
    if (!init)
	{
        c = (double *) mem_alloc(sizeof(double) * HAN_SIZE, "window");
        read_ana_window(c);
        x = (XX *) mem_alloc(sizeof(XX),"x");
        for (i=0;i<2;i++)
            for (j=0;j<HAN_SIZE;j++)
                (*x)[i][j] = 0;
        init = 1;
    }

    for (i=0;i<32;i++)
		(*x)[k][31-i+off[k]] = (double) *(*buffer)++/SCALE;
    for (i=0;i<HAN_SIZE;i++)
		z[i] = (*x)[k][(i+off[k])&HAN_SIZE-1] * c[i];
    off[k] += 480;              
    off[k] &= HAN_SIZE-1;

}
 
/************************************************************************/
/*
/* create_ana_filter()
/*
/* 功能:  计算分解滤波器组的系数
/* 
/* 结构描述:
/* 计算分解滤波器组的系数并四舍五入到小数点后第9位
/* 系数存储在#filter#中。
/*
/************************************************************************/
 
void create_ana_filter(filter)
double filter[SBLIMIT][64];
{
   register int i,k;
 
   for (i=0; i<32; i++)
      for (k=0; k<64; k++)
	  {
          if ((filter[i][k] = 1e9*cos((double)((2*i+1)*(16-k)*PI64))) >= 0)
             modf(filter[i][k]+0.5, &filter[i][k]);
          else
             modf(filter[i][k]-0.5, &filter[i][k]);
          filter[i][k] *= 1e-9;
	  }
}

/************************************************************************/
/*
/* filter_subband()
/*
/* 功能:  计算分解滤波组系数
/*
/* 结构解释:
/*     加窗的采样值#z#通过由矩阵#m#定义的数字滤波器，产生子带采样值#s#。这
是通过从加窗的样本中取样，并和滤波器的矩阵相乘得到的32位子带采样值。
/*
/************************************************************************/
 
void filter_subband(z,s)
double z[HAN_SIZE], s[SBLIMIT];
{
   double y[64];
   int i,j;
   static char init = 0;
   typedef double MM[SBLIMIT][64];
   static MM *m;
   long    SIZE_OF_MM;
   SIZE_OF_MM      = SBLIMIT*64;
   SIZE_OF_MM      *= 8;
   if (!init)
   {
       m = (MM *) mem_alloc(SIZE_OF_MM, "filter");
       create_ana_filter(*m);
       init = 1;
   }
   for (i=0;i<64;i++)
	   for (j=0, y[i] = 0;j<8;j++)
		   y[i] += z[i+64*j];
   for (i=0;i<SBLIMIT;i++)
       for (j=0, s[i]= 0;j<64;j++)
		   s[i] += (*m)[i][j] * y[j];
}

/************************************************************************/
/*
/* encode_info()
/*
/* 功能:  将同步字节和头信息放入输出比特流
/*
/************************************************************************/
 
void encode_info(fr_ps,bs)
frame_params *fr_ps;
Bit_stream_struc *bs;
{
        layer *info = fr_ps->header;
 
        putabits(bs,0xfff,12);                    /* syncword 12 bits */
        put1bit(bs,info->version);               /* ID        1 bit  */
        putabits(bs,4-info->lay,2);               /* layer     2 bits */
        put1bit(bs,!info->error_protection);     /* bit set => no err prot */
        putabits(bs,info->bitrate_index,4);
        putabits(bs,info->sampling_frequency,2);
        put1bit(bs,info->padding);
        put1bit(bs,info->extension);             /* private_bit */
        putabits(bs,info->mode,2);
        putabits(bs,info->mode_ext,2);
        put1bit(bs,info->copyright);
        put1bit(bs,info->original);
        putabits(bs,info->emphasis,2);
}
 
/************************************************************************/
/*
/* mod()
/*
/* 功能: 返回变量的绝对值
/*
/************************************************************************/
 
double mod(a)
double a;
{
    return (a > 0) ? a : -a;
}
 
/************************************************************************/
/*
/* I_combine_LR    (Layer I)
/* II_combine_LR   (Layer II)
/*
/* 功能:将左右声道组合成一个单声道
/*
/* 结构描述:  左右子带采样的平均值放入#joint_sample#中。
/*
/*
/************************************************************************/
 
void I_combine_LR(sb_sample, joint_sample)
double sb_sample[2][3][SCALE_BLOCK][SBLIMIT];
double joint_sample[3][SCALE_BLOCK][SBLIMIT];
{   
    int sb, smp;
 
   for(sb = 0; sb<SBLIMIT; ++sb)
      for(smp = 0; smp<SCALE_BLOCK; ++smp)
        joint_sample[0][smp][sb] = .5 *
                    (sb_sample[0][0][smp][sb] + sb_sample[1][0][smp][sb]);
}
 
void II_combine_LR(sb_sample, joint_sample, sblimit)
double sb_sample[2][3][SCALE_BLOCK][SBLIMIT];
double joint_sample[3][SCALE_BLOCK][SBLIMIT];
int sblimit;
{  
   int sb, smp, sufr;
 
   for(sb = 0; sb<sblimit; ++sb)
      for(smp = 0; smp<SCALE_BLOCK; ++smp)
         for(sufr = 0; sufr<3; ++sufr)
            joint_sample[sufr][smp][sb] = .5 * (sb_sample[0][sufr][smp][sb]
                                           + sb_sample[1][sufr][smp][sb]);
}
 
/************************************************************************
/*
/* I_scale_factor_calc     (Layer I)
/* II_scale_factor_calc    (Layer II)
/*
/* 功能:算每个子带中每组12个子带采样的比例因子
/*
/* 结构解释:  从#multiple[]#选择比12个子带抽样值的绝对值的最大值还要大的
   做为比例因子，并将比例因子信息存入#scalar#中
/*
/* Layer II 有3组12子带采样
/*
/************************************************************************/
 
void I_scale_factor_calc(sb_sample,scalar,stereo)
double sb_sample[][3][SCALE_BLOCK][SBLIMIT];
unsigned int scalar[][3][SBLIMIT];
int stereo;
{
   int i,j, k;
   double s[SBLIMIT];
 
   for (k=0;k<stereo;k++)
   {
     for (i=0;i<SBLIMIT;i++)
       for (j=1, s[i] = mod(sb_sample[k][0][0][i]);j<SCALE_BLOCK;j++)
         if (mod(sb_sample[k][0][j][i]) > s[i])
            s[i] = mod(sb_sample[k][0][j][i]);
 
     for (i=0;i<SBLIMIT;i++)
       for (j=SCALE_RANGE-2,scalar[k][0][i]=0;j>=0;j--) /* $A 6/16/92 */
         if (s[i] <= multiple[j])
		 {
            scalar[k][0][i] = j;
            break;
         }
   }
}

/******************************** Layer II ******************************/
 
void II_scale_factor_calc(sb_sample,scalar,stereo,sblimit)
double sb_sample[][3][SCALE_BLOCK][SBLIMIT];
unsigned int scalar[][3][SBLIMIT];
int stereo,sblimit;
{
  int i,j, k,t;
  double s[SBLIMIT];
 
  for (k=0;k<stereo;k++) for (t=0;t<3;t++)
  {
    for (i=0;i<sblimit;i++)
      for (j=1, s[i] = mod(sb_sample[k][t][0][i]);j<SCALE_BLOCK;j++)
        if (mod(sb_sample[k][t][j][i]) > s[i])
             s[i] = mod(sb_sample[k][t][j][i]);
 
    for (i=0;i<sblimit;i++)
       for (j=SCALE_RANGE-2,scalar[k][t][i]=0;j>=0;j--)    /* $A 6/16/92 */
         if (s[i] <= multiple[j])
		 {
            scalar[k][t][i] = j;
            break;
		 }
      for (i=sblimit;i<SBLIMIT;i++) scalar[k][t][i] = SCALE_RANGE-1;
  }
}

/************************************************************************
/*
/* pick_scale  (Layer II)
/*
/* 功能: 对每个子带，将3各比例因子中最小的和一个帧放入#max_sc#.  
/* 这用于心理声学模型I.
/*
/************************************************************************/
 
void pick_scale(scalar, fr_ps, max_sc)
unsigned int scalar[2][3][SBLIMIT];
frame_params *fr_ps;
double max_sc[2][SBLIMIT];
{
  int i,j,k;
  unsigned int max;
  int stereo  = fr_ps->stereo;
  int sblimit = fr_ps->sblimit;
 
  for (k=0;k<stereo;k++)
    for (i=0;i<sblimit;max_sc[k][i] = multiple[max],i++)
      for (j=1, max = scalar[k][0][i];j<3;j++)
         if (max > scalar[k][j][i])
			 max = scalar[k][j][i];
  for (i=sblimit;i<SBLIMIT;i++)
	  max_sc[0][i] = max_sc[1][i] = 1E-20;
}

/************************************************************************
/*
/* put_scale   (Layer I)
/*
/* 功能: 将#max_sc#设置为#scalar中的比例因子索引.
/*
/************************************************************************/
 
void put_scale(scalar, fr_ps, max_sc)
unsigned int scalar[2][3][SBLIMIT];
frame_params *fr_ps;
double max_sc[2][SBLIMIT];
{
   int i,k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
 
   for (k=0;k<stereo;k++) for (i=0;i<SBLIMIT;i++)
        max_sc[k][i] = multiple[scalar[k][0][i]];
}
 
/************************************************************************
/*
/* II_transmission_pattern (Layer II only)
/*
/* 功能:对给定的子带传送1个或2个或全部3个比例因子并填充相应的比例因子选择
        信息/*
/* 结构描述:  子带和信道根据三个比例因子的变化划分。如果三个比例因子变化
      不大，则仅传送1个或2个比例因子。
/*
/************************************************************************/
 
void II_transmission_pattern(scalar, scfsi, fr_ps)
unsigned int scalar[2][3][SBLIMIT];
unsigned int scfsi[2][SBLIMIT];
frame_params *fr_ps;
{
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int dscf[2];
   int sclass[2],i,j,k;
   static int pattern[5][5] = {0x123, 0x122, 0x122, 0x133, 0x123,
                               0x113, 0x111, 0x111, 0x444, 0x113,
                               0x111, 0x111, 0x111, 0x333, 0x113,
                               0x222, 0x222, 0x222, 0x333, 0x123,
                               0x123, 0x122, 0x122, 0x133, 0x123};
 
   for (k=0;k<stereo;k++)
     for (i=0;i<sblimit;i++)
	 {
       dscf[0] =  (scalar[k][0][i]-scalar[k][1][i]);
       dscf[1] =  (scalar[k][1][i]-scalar[k][2][i]);
       for (j=0;j<2;j++)
	   {
         if (dscf[j]<=-3)
			 sclass[j] = 0;
         else if (dscf[j] > -3 && dscf[j] <0)
			 sclass[j] = 1;
              else if (dscf[j] == 0)
				  sclass[j] = 2;
                   else if (dscf[j] > 0 && dscf[j] < 3)
					   sclass[j] = 3;
                        else sclass[j] = 4;
       }
       switch (pattern[sclass[0]][sclass[1]])
	   {
         case 0x123 :
			 scfsi[k][i] = 0;
             break;
         case 0x122 :
			 scfsi[k][i] = 3;
             scalar[k][2][i] = scalar[k][1][i];
             break;
         case 0x133 :
			 scfsi[k][i] = 3;
             scalar[k][1][i] = scalar[k][2][i];
             break;
         case 0x113 :
			 scfsi[k][i] = 1;
             scalar[k][1][i] = scalar[k][0][i];
             break;
         case 0x111 :
			 scfsi[k][i] = 2;
             scalar[k][1][i] = scalar[k][2][i] = scalar[k][0][i];
             break;
         case 0x222 :
			 scfsi[k][i] = 2;
             scalar[k][0][i] = scalar[k][2][i] = scalar[k][1][i];
             break;
         case 0x333 :
			 scfsi[k][i] = 2;
             scalar[k][0][i] = scalar[k][1][i] = scalar[k][2][i];
             break;
         case 0x444 :
			 scfsi[k][i] = 2;
             if (scalar[k][0][i] > scalar[k][2][i])
                 scalar[k][0][i] = scalar[k][2][i];
             scalar[k][1][i] = scalar[k][2][i] = scalar[k][0][i];
	   }
	 }
}
 
/************************************************************************
/*
/* I_encode_scale  (Layer I)
/* II_encode_scale (Layer II)
/*
/* 功能: 编码后的比例因子信息放入输出队列等待传送
/*

/************************************************************************/
 
void I_encode_scale(scalar, bit_alloc, fr_ps, bs)
unsigned int scalar[2][3][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int i,j;
 
   for (i=0;i<SBLIMIT;i++)
	   for (j=0;j<stereo;j++)
           if (bit_alloc[j][i])
			   putabits(bs,scalar[j][0][i],6);
}
 
/***************************** Layer II  ********************************/
 
void II_encode_scale(bit_alloc, scfsi, scalar, fr_ps, bs)
unsigned int bit_alloc[2][SBLIMIT], scfsi[2][SBLIMIT];
unsigned int scalar[2][3][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   int i,j,k;
 
   for (i=0;i<sblimit;i++)
	   for (k=0;k<stereo;k++)
           if (bit_alloc[k][i])
			   putabits(bs,scfsi[k][i],2);
 
   for (i=0;i<sblimit;i++)
	   for (k=0;k<stereo;k++)
           if (bit_alloc[k][i])  
        switch (scfsi[k][i]) {
           case 0: for (j=0;j<3;j++)
                     putabits(bs,scalar[k][j][i],6);
                   break;
           case 1:
           case 3: putabits(bs,scalar[k][0][i],6);
                   putabits(bs,scalar[k][2][i],6);
                   break;
           case 2: putabits(bs,scalar[k][0][i],6);
        }
}
 
/*=======================================================================\
|      下面的程序是在心理声学模型已经计算出掩蔽阈值后进行的              |
|                                                                        |
\=======================================================================*/
 
/************************************************************************
/*
/* I_bits_for_nonoise  (Layer I)
/* II_bits_for_nonoise (Layer II)
/*
/* 功能:用计算出的掩蔽噪声比可以计算出分配给子带信号的量化位数
/*
/* 结构描述:
/* bbal = # 用来为比特分配进行编码所需的bits 
/* bsel = # 用来为比例因子选择信息进行编码所需的bits
/* banc = # 用来为辅助信息进行编码所需的bits
/*
/* 对每个子带和信道，我们累加bit的值直到出现以下情况：
/* －达到该信道能允许分配的最大bit位数
/* －掩蔽噪声比（MNR）好于或等于最小掩蔽水平
/*
/* 支持联合立体声
/*
/************************************************************************/

static double snr[18] = {0.00, 7.00, 11.00, 16.00, 20.84,
                         25.28, 31.59, 37.75, 43.84,
                         49.89, 55.93, 61.96, 67.98, 74.01,
                         80.03, 86.05, 92.01, 98.01};

int I_bits_for_nonoise(perm_smr, fr_ps)
double perm_smr[2][SBLIMIT];
frame_params *fr_ps;
{
   int i,j,k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   int req_bits = 0;
 
   
   req_bits = 32 + 4 * ( (jsbound * stereo) + (SBLIMIT-jsbound) );
 
   for(i=0; i<SBLIMIT; ++i)
     for(j=0; j<((i<jsbound)?stereo:1); ++j) {
       for(k=0;k<14; ++k)
         if( (-perm_smr[j][i] + snr[k]) >= NOISY_MIN_MNR)
           break; /* 已找到足够的bit了 */
         if(stereo == 2 && i >= jsbound)     
           for(;k<14; ++k)
             if( (-perm_smr[1-j][i] + snr[k]) >= NOISY_MIN_MNR) break;
         if(k>0) req_bits += (k+1)*SCALE_BLOCK + 6*((i>=jsbound)?stereo:1);
   }
   return req_bits;
}
 
/***************************** Layer II  ********************************/
 
int II_bits_for_nonoise(perm_smr, scfsi, fr_ps)
double perm_smr[2][SBLIMIT];
unsigned int scfsi[2][SBLIMIT];
frame_params *fr_ps;
{
   int sb,ch,ba;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   al_table *alloc = fr_ps->alloc;
   int req_bits = 0, bbal = 0, berr = 0, banc = 32;
   int maxAlloc, sel_bits, sc_bits, smp_bits;
static int sfsPerScfsi[] = { 3,2,1,2 };    
   /* added 92-08-11 shn */
   if (fr_ps->header->error_protection) berr=16; else berr=0; 
 
   for (sb=0; sb<jsbound; ++sb)
     bbal += stereo * (*alloc)[sb][0].bits;
   for (sb=jsbound; sb<sblimit; ++sb)
     bbal += (*alloc)[sb][0].bits;
   req_bits = banc + bbal + berr;
 
   for(sb=0; sb<sblimit; ++sb)
     for(ch=0; ch<((sb<jsbound)?stereo:1); ++ch) {
       maxAlloc = (1<<(*alloc)[sb][0].bits)-1;
       sel_bits = sc_bits = smp_bits = 0;
       for(ba=0;ba<maxAlloc-1; ++ba)
         if( (-perm_smr[ch][sb] + snr[(*alloc)[sb][ba].quant+((ba>0)?1:0)])
             >= NOISY_MIN_MNR)
            break;      /* we found enough bits */
       if(stereo == 2 && sb >= jsbound) /* check other JS channel */
         for(;ba<maxAlloc-1; ++ba)
           if( (-perm_smr[1-ch][sb]+ snr[(*alloc)[sb][ba].quant+((ba>0)?1:0)])
               >= NOISY_MIN_MNR)
             break;
       if(ba>0) {
         smp_bits = SCALE_BLOCK * ((*alloc)[sb][ba].group * (*alloc)[sb][ba].bits);
         /* scale factor bits required for subband */
         sel_bits = 2;
         sc_bits  = 6 * sfsPerScfsi[scfsi[ch][sb]];
         if(stereo == 2 && sb >= jsbound) {
           /* each new js sb has L+R scfsis */
           sel_bits += 2;
           sc_bits  += 6 * sfsPerScfsi[scfsi[1-ch][sb]];
         }
         req_bits += smp_bits+sel_bits+sc_bits;
       }
   }
   return req_bits;
}
 
/************************************************************************
/*
/* I_main_bit_allocation   (Layer I)
/* II_main_bit_allocation  (Layer II)
/*
/* 功能: 对联合立体声模式，决定使用4中联合立体声中的哪种。然后调用 *_a_bit_allocation()
/* 它将为每个子带分配比特，知道比特用完和MNR达到阈值/*

/*
/************************************************************************/
 
void I_main_bit_allocation(perm_smr, bit_alloc, adb, fr_ps)
double perm_smr[2][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
int *adb;
frame_params *fr_ps;
{
   int  noisy_sbs;
   int  mode, mode_ext, lay, i;
   int  rq_db, av_db = *adb;
static  int init = 0;
 
   if(init == 0) {
     /* rearrange snr for layer I */
     snr[2] = snr[3];
     for (i=3;i<16;i++) snr[i] = snr[i+2];
     init = 1;
   }
 
   if((mode = fr_ps->actual_mode) == MPG_MD_JOINT_STEREO) {
     fr_ps->header->mode = MPG_MD_STEREO;
     fr_ps->header->mode_ext = 0;
     fr_ps->jsbound = fr_ps->sblimit;
     if(rq_db = I_bits_for_nonoise(perm_smr, fr_ps) > *adb) {
       fr_ps->header->mode = MPG_MD_JOINT_STEREO;
       mode_ext = 4;           /* 3 is least severe reduction */
       lay = fr_ps->header->lay;
       do {
          --mode_ext;
          fr_ps->jsbound = js_bound(lay, mode_ext);
          rq_db = I_bits_for_nonoise(perm_smr, fr_ps);
       } while( (rq_db > *adb) && (mode_ext > 0));
       fr_ps->header->mode_ext = mode_ext;
     }    
   }
   noisy_sbs = I_a_bit_allocation(perm_smr, bit_alloc, adb, fr_ps);
}
 
/***************************** Layer II  ********************************/
 
void II_main_bit_allocation(perm_smr, scfsi, bit_alloc, adb, fr_ps)
double perm_smr[2][SBLIMIT];
unsigned int scfsi[2][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
int *adb;
frame_params *fr_ps;
{
   int  noisy_sbs;
   int  mode, mode_ext, lay;
   int  rq_db, av_db = *adb;
 
   if((mode = fr_ps->actual_mode) == MPG_MD_JOINT_STEREO) {
     fr_ps->header->mode = MPG_MD_STEREO;
     fr_ps->header->mode_ext = 0;
     fr_ps->jsbound = fr_ps->sblimit;
     if((rq_db=II_bits_for_nonoise(perm_smr, scfsi, fr_ps)) > *adb) {
       fr_ps->header->mode = MPG_MD_JOINT_STEREO;
       mode_ext = 4;           /* 3 is least severe reduction */
       lay = fr_ps->header->lay;
       do {
         --mode_ext;
         fr_ps->jsbound = js_bound(lay, mode_ext);
         rq_db = II_bits_for_nonoise(perm_smr, scfsi, fr_ps);
       } while( (rq_db > *adb) && (mode_ext > 0));
       fr_ps->header->mode_ext = mode_ext;
     }   
   }
   noisy_sbs = II_a_bit_allocation(perm_smr, scfsi, bit_alloc, adb, fr_ps);
}
 
/************************************************************************
/*
/* I_a_bit_allocation  (Layer I)
/* II_a_bit_allocation (Layer II)
/*
/* 功能:将bit加到掩蔽噪声比最低的子带上，直到达到子带所能容忍的最大bit数
/*
/* 结构描述:
/* 1. 找到有最小掩蔽噪声比的子带和信道
/* 2. 计算若将bit分配提高到更高层次所需的bit数目
/* 3. 如果有足够的bit，并且子带没有达到最大的可分配数，则更新bit分配，
/*    掩蔽噪声比和相应的可供分配的位数
/* 4. 重复进行上面的过程，知道没有bit剩余或没有满足条件的子带
/*
/************************************************************************/
 
int I_a_bit_allocation(perm_smr, bit_alloc, adb, fr_ps) /* return noisy sbs */
double perm_smr[2][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
int *adb;
frame_params *fr_ps;
{
   int i, k, smpl_bits, scale_bits, min_sb, min_ch, oth_ch;
   int bspl, bscf, ad, noisy_sbs, done = 0, bbal ;
   double mnr[2][SBLIMIT];
   double smallm;
   char used[2][SBLIMIT];
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   al_table *alloc = fr_ps->alloc;
   static char init= 0;
   static int banc=32, berr=0;
 
   if (!init)
   {
      init = 1;
      if (fr_ps->header->error_protection) berr = 16;  /* added 92-08-11 shn */
   }
   bbal = 4 * ( (jsbound * stereo) + (SBLIMIT-jsbound) );
   *adb -= bbal + berr + banc;
   ad= *adb;
 
   for (i=0;i<SBLIMIT;i++) for (k=0;k<stereo;k++)
   {
     mnr[k][i]=snr[0]-perm_smr[k][i];
     bit_alloc[k][i] = 0;
     used[k][i] = 0;
   }
   bspl = bscf = 0;
 
   do
   {
      /* 找到有最小信掩（SMR）比的子带*/
     smallm = mnr[0][0]+1;    min_sb = -1; min_ch = -1;
     for (i=0;i<SBLIMIT;i++) for (k=0;k<stereo;k++)
       /* 继续直到没有bit剩余 */
       if (used[k][i] != 2 && smallm > mnr[k][i])
	   {
         smallm = mnr[k][i];
         min_sb = i;  min_ch = k;
       }
     if(min_sb > -1) {  
       if (used[min_ch][min_sb])
	   {
		   smpl_bits = SCALE_BLOCK;
		   scale_bits = 0;
	   }
       else
	   {
		   smpl_bits = 24;
		   scale_bits = 6;
	   }
       if(min_sb >= jsbound)
		   scale_bits *= stereo;
 
       /* 检查是否还有足够的bit用来增加在最小的带宽中分辨率*/
 
       if (ad >= bspl + bscf + scale_bits + smpl_bits)
	   {
         bspl += smpl_bits; /* 子带抽样bit */
         bscf += scale_bits; /* 比例因子bit */
         bit_alloc[min_ch][min_sb]++;
         used[min_ch][min_sb] = 1; 
         mnr[min_ch][min_sb] = -perm_smr[min_ch][min_sb]
                               + snr[bit_alloc[min_ch][min_sb]];
         
         if (bit_alloc[min_ch][min_sb] ==  14 )
			 used[min_ch][min_sb] = 2;
       }
       else            
         used[min_ch][min_sb] = 2; 
       if(stereo == 2 && min_sb >= jsbound)
	   {
         oth_ch = 1-min_ch;  
         bit_alloc[oth_ch][min_sb] = bit_alloc[min_ch][min_sb];
         used[oth_ch][min_sb] = used[min_ch][min_sb];
         mnr[oth_ch][min_sb] = -perm_smr[oth_ch][min_sb]
                               + snr[bit_alloc[oth_ch][min_sb]];
       }
     }
   } while(min_sb>-1);     

   
   ad -= bspl+bscf;
   *adb = ad;

   
   noisy_sbs = 0; smallm = mnr[0][0];
   for(k=0; k<stereo; ++k)
   {
     for(i = 0; i< SBLIMIT; ++i)
	 {
       if(mnr[k][i] < NOISY_MIN_MNR)
		   ++noisy_sbs;
       if(smallm > mnr[k][i])
		   smallm = mnr[k][i];
     }
   }
   return noisy_sbs;
}

/***************************** Layer II  ********************************/
 
int II_a_bit_allocation(perm_smr, scfsi, bit_alloc, adb, fr_ps)
double perm_smr[2][SBLIMIT];
unsigned int scfsi[2][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
int *adb;
frame_params *fr_ps;
{
   int i, min_ch, min_sb, oth_ch, k, increment, scale, seli, ba;
   int bspl, bscf, bsel, ad, noisy_sbs, bbal=0;
   double mnr[2][SBLIMIT], smallm;
   char used[2][SBLIMIT];
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   al_table *alloc = fr_ps->alloc;
static char init= 0;
static int banc=32, berr=0;
static int sfsPerScfsi[] = { 3,2,1,2 };   
 
   if (!init) { 
       init = 1;  
       if (fr_ps->header->error_protection) berr=16; 
   }
   for (i=0; i<jsbound; ++i)
     bbal += stereo * (*alloc)[i][0].bits;
   for (i=jsbound; i<sblimit; ++i)
     bbal += (*alloc)[i][0].bits;
   *adb -= bbal + berr + banc;
   ad = *adb;
 
   for (i=0;i<sblimit;i++) for (k=0;k<stereo;k++) {
     mnr[k][i]=snr[0]-perm_smr[k][i];
     bit_alloc[k][i] = 0;
     used[k][i] = 0;
   }
   bspl = bscf = bsel = 0;
 
   do  {
     smallm = 999999.0; min_sb = -1; min_ch = -1;
     for (i=0;i<sblimit;i++) for(k=0;k<stereo;++k)
       if (used[k][i]  != 2 && smallm > mnr[k][i]) {
         smallm = mnr[k][i];
         min_sb = i;  min_ch = k;
     }
     if(min_sb > -1) {   
       increment = SCALE_BLOCK * ((*alloc)[min_sb][bit_alloc[min_ch][min_sb]+1].group *
                        (*alloc)[min_sb][bit_alloc[min_ch][min_sb]+1].bits);
       if (used[min_ch][min_sb])
         increment -= SCALE_BLOCK * ((*alloc)[min_sb][bit_alloc[min_ch][min_sb]].group*
                           (*alloc)[min_sb][bit_alloc[min_ch][min_sb]].bits);
 
       oth_ch = 1 - min_ch;   
       if (used[min_ch][min_sb]) scale = seli = 0;
       else {          /* this channel had no bits or scfs before */
         seli = 2;
         scale = 6 * sfsPerScfsi[scfsi[min_ch][min_sb]];
         if(stereo == 2 && min_sb >= jsbound) {
           /* each new js sb has L+R scfsis */
           seli += 2;
           scale += 6 * sfsPerScfsi[scfsi[oth_ch][min_sb]];
         }
       }
      
       if (ad >= bspl + bscf + bsel + seli + scale + increment) {
         ba = ++bit_alloc[min_ch][min_sb]; /* next up alloc */
         bspl += increment;  /* bits for subband sample */
         bscf += scale;      /* bits for scale factor */
         bsel += seli;       /* bits for scfsi code */
         used[min_ch][min_sb] = 1; /* subband has bits */
         mnr[min_ch][min_sb] = -perm_smr[min_ch][min_sb] +
                               snr[(*alloc)[min_sb][ba].quant+1];
         /* Check if subband has been fully allocated max bits */
         if (ba >= (1<<(*alloc)[min_sb][0].bits)-1) used[min_ch][min_sb] = 2;
       }
       else used[min_ch][min_sb] = 2; /* can't increase this alloc */
       if(min_sb >= jsbound && stereo == 2) {
         /* above jsbound, alloc applies L+R */
         ba = bit_alloc[oth_ch][min_sb] = bit_alloc[min_ch][min_sb];
         used[oth_ch][min_sb] = used[min_ch][min_sb];
         mnr[oth_ch][min_sb] = -perm_smr[oth_ch][min_sb] +
                               snr[(*alloc)[min_sb][ba].quant+1];
       }
     }
   } while(min_sb > -1);  
   ad -= bspl+bscf+bsel;   *adb = ad;
   for (i=sblimit;i<SBLIMIT;i++) for (k=0;k<stereo;k++) bit_alloc[k][i]=0;
 
   noisy_sbs = 0;  smallm = mnr[0][0];      /* calc worst noise in case */
   for(k=0;k<stereo;++k) {
     for (i=0;i<sblimit;i++) {
       if (smallm > mnr[k][i]) smallm = mnr[k][i];
       if(mnr[k][i] < NOISY_MIN_MNR) ++noisy_sbs; /* noise is not masked */

     }
   }
   return noisy_sbs;
}
 
/************************************************************************
/*
/* I_subband_quantization  (Layer I)
/* II_subband_quantization (Layer II)
/*
/* 功能 : 根据计算的比特分配进行量化子带
/*
/* 结构描述:  除以比例因子后的采样根据函数a×x＋b进行量化，其中a和b是量化
  等级的函数。结果将根据量化位数进行截断，并将最高有效字节取反。
/*
/* 
/*
/************************************************************************/
 
static double a[17] = {
  0.750000000, 0.625000000, 0.875000000, 0.562500000, 0.937500000,
  0.968750000, 0.984375000, 0.992187500, 0.996093750, 0.998046875,
  0.999023438, 0.999511719, 0.999755859, 0.999877930, 0.999938965,
  0.999969482, 0.999984741 };
 
static double b[17] = {
  -0.250000000, -0.375000000, -0.125000000, -0.437500000, -0.062500000,
  -0.031250000, -0.015625000, -0.007812500, -0.003906250, -0.001953125,
  -0.000976563, -0.000488281, -0.000244141, -0.000122070, -0.000061035,
  -0.000030518, -0.000015259 };
 
void I_subband_quantization(scalar, sb_samples, j_scale, j_samps,
                            bit_alloc, sbband, fr_ps)
unsigned int scalar[2][3][SBLIMIT];
double sb_samples[2][3][SCALE_BLOCK][SBLIMIT];
unsigned int j_scale[3][SBLIMIT];
double j_samps[3][SCALE_BLOCK][SBLIMIT]; /* L+R for j-stereo if necess */
unsigned int bit_alloc[2][SBLIMIT];
unsigned int sbband[2][3][SCALE_BLOCK][SBLIMIT];
frame_params *fr_ps;
{
   int i, j, k, n, sig;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   double d;
static char init = 0;

   if (!init) {
     init = 1;
     a[1] = a[2]; b[1] = b[2];
     for (i=2;i<15;i++) { a[i] = a[i+2]; b[i] = b[i+2]; }
   }
   for (j=0;j<SCALE_BLOCK;j++) for (i=0;i<SBLIMIT;i++)
     for (k=0;k<((i<jsbound)?stereo:1);k++)
       if (bit_alloc[k][i]) {
         
         if(stereo == 2 && i>=jsbound)
           d = j_samps[0][j][i] / multiple[j_scale[0][i]];
         else
           d = sb_samples[k][0][j][i] / multiple[scalar[k][0][i]];
         n = bit_alloc[k][i];
         d = d * a[n-1] + b[n-1];
         if (d >= 0) sig = 1;
         else { sig = 0; d += 1.0; }
         sbband[k][0][j][i] = (unsigned int) (d * (double) (1L<<n));
         if (sig) sbband[k][0][j][i] |= 1<<n;
       }
}
 
/***************************** Layer II  ********************************/
 
void II_subband_quantization(scalar, sb_samples, j_scale, j_samps,
                             bit_alloc, sbband, fr_ps)
unsigned int scalar[2][3][SBLIMIT];
double sb_samples[2][3][SCALE_BLOCK][SBLIMIT];
unsigned int j_scale[3][SBLIMIT];
double j_samps[3][SCALE_BLOCK][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
unsigned int sbband[2][3][SCALE_BLOCK][SBLIMIT];
frame_params *fr_ps;
{
   int i, j, k, s, n, qnt, sig;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   double d;
   al_table *alloc = fr_ps->alloc;

   for (s=0;s<3;s++)
     for (j=0;j<SCALE_BLOCK;j++)
       for (i=0;i<sblimit;i++)
         for (k=0;k<((i<jsbound)?stereo:1);k++)
           if (bit_alloc[k][i]) {
             
             if(stereo == 2 && i>=jsbound)       /* 使用联合立体声模式*/
               d = j_samps[s][j][i] / multiple[j_scale[s][i]];
             else
               d = sb_samples[k][s][j][i] / multiple[scalar[k][s][i]];
             if (mod(d) > 1.0)
               printf("Not scaled properly %d %d %d %d\n",k,s,j,i);
             qnt = (*alloc)[i][bit_alloc[k][i]].quant;
             d = d * a[qnt] + b[qnt];
             
             if (d >= 0) sig = 1;
             else { sig = 0; d += 1.0; }
             n = 0;
#ifndef MS_DOS
             stps = (*alloc)[i][bit_alloc[k][i]].steps;
             while ((1L<<n) < stps) n++;
#else
             while  ( ( (unsigned long)(1L<<(long)n) <
                       ((unsigned long) ((*alloc)[i][bit_alloc[k][i]].steps)
                        & 0xffff
                        )
                       ) && ( n <16)
                     ) n++;
#endif
             n--;
             sbband[k][s][j][i] = (unsigned int) (d * (double) (1L<<n));
             
             if (sig) sbband[k][s][j][i] |= 1<<n;
           }
           for (s=0;s<3;s++)
             for (j=sblimit;j<SBLIMIT;j++)
               for (i=0;i<SCALE_BLOCK;i++) for (k=0;k<stereo;k++) sbband[k][s][i][j] = 0;
}
 
/************************************************************************
/*
/* I_encode_bit_alloc  (Layer I)
/* II_encode_bit_alloc (Layer II)
/*
/* 功能 : 向比特流中写入比特分配信息
/*
/
/************************************************************************/
 
void I_encode_bit_alloc(bit_alloc, fr_ps, bs)
unsigned int bit_alloc[2][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   int i,k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
 
   for (i=0;i<SBLIMIT;i++)
     for (k=0;k<((i<jsbound)?stereo:1);k++) putabits(bs,bit_alloc[k][i],4);
}
 
/***************************** Layer II  ********************************/
 
void II_encode_bit_alloc(bit_alloc, fr_ps, bs)
unsigned int bit_alloc[2][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   int i,k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   al_table *alloc = fr_ps->alloc;
 
   for (i=0;i<sblimit;i++)
     for (k=0;k<((i<jsbound)?stereo:1);k++)
       putabits(bs,bit_alloc[k][i],(*alloc)[i][0].bits);
}
 
/************************************************************************
/*
/* I_sample_encoding   (Layer I)
/* II_sample_encoding  (Layer II)
/*
/* 功能 :将一帧子带采样信号送入比特流
/*
/* 结构描述:  每一个抽样的bit数由#bit_alloc#提供。层2支持非2的幂的量化步长。
/*
/************************************************************************/
 
void I_sample_encoding(sbband, bit_alloc, fr_ps, bs)
unsigned int sbband[2][3][SCALE_BLOCK][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   int i,j,k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
 
   for(j=0;j<SCALE_BLOCK;j++) {
     for(i=0;i<SBLIMIT;i++)
       for(k=0;k<((i<jsbound)?stereo:1);k++)
         if(bit_alloc[k][i]) putabits(bs,sbband[k][0][j][i],bit_alloc[k][i]+1);
   }
}
 
/***************************** Layer II  ********************************/
 
void II_sample_encoding(sbband, bit_alloc, fr_ps, bs)
unsigned int sbband[2][3][SCALE_BLOCK][SBLIMIT];
unsigned int bit_alloc[2][SBLIMIT];
frame_params *fr_ps;
Bit_stream_struc *bs;
{
   unsigned int temp;
   unsigned int j,s,x,y;
   int i, k;
   int stereo  = fr_ps->stereo;
   int sblimit = fr_ps->sblimit;
   int jsbound = fr_ps->jsbound;
   al_table *alloc = fr_ps->alloc;
 
   for (s=0;s<3;s++)
     for (j=0;j<SCALE_BLOCK;j+=3)
       for (i=0;i<sblimit;i++)
         for (k=0;k<((i<jsbound)?stereo:1);k++)
           if (bit_alloc[k][i]) {
             if ((*alloc)[i][bit_alloc[k][i]].group == 3) {
               for (x=0;x<3;x++) putabits(bs,sbband[k][s][j+x][i],
                                         (*alloc)[i][bit_alloc[k][i]].bits);
             }
             else {
               y =(*alloc)[i][bit_alloc[k][i]].steps;
               temp = sbband[k][s][j][i] +
                      sbband[k][s][j+1][i] * y +
                      sbband[k][s][j+2][i] * y * y;
               putabits(bs,temp,(*alloc)[i][bit_alloc[k][i]].bits);
             }
           }
}
 
/************************************************************************
/*
/* encode_CRC
/*
/************************************************************************/
 
void encode_CRC(crc, bs)
unsigned int crc;
Bit_stream_struc *bs;
{
   putabits(bs, crc, 16);
}
