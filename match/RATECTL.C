/* ratectl.c, 比特率控制程序*/



#include <stdio.h>
#include <math.h>

#include "global.h"

static short PACKED_0[4] = { 0, 0, 0, 0 };

/* 函数声明 */
static void calc_actj(unsigned char *frame);
static double var_sblk(unsigned char *p, int lx);

/* 速率控制变量*/
int Xi, Xp, Xb, r, d0i, d0p, d0b;
double avg_act;
static int R, T, d;
static double actsum;
static int Np, Nb, S, Q;
static int prev_mquant;

void rc_init_seq()
{
  if (r==0)  r = (int)floor(2.0*bit_rate/frame_rate + 0.5);

  if (avg_act==0.0)  avg_act = 400.0;

  R = 0;

  /* global complexity measure */
  if (Xi==0) Xi = (int)floor(160.0*bit_rate/115.0 + 0.5);
  if (Xp==0) Xp = (int)floor( 60.0*bit_rate/115.0 + 0.5);
  if (Xb==0) Xb = (int)floor( 42.0*bit_rate/115.0 + 0.5);

  /* virtual buffer fullness */
  if (d0i==0) d0i = (int)floor(10.0*r/31.0 + 0.5);
  if (d0p==0) d0p = (int)floor(10.0*r/31.0 + 0.5);
  if (d0b==0) d0b = (int)floor(1.4*10.0*r/31.0 + 0.5);

#ifdef DEBUG
  fprintf(statfile,"\nrate control: sequence initialization\n");
  fprintf(statfile,
    " initial global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
    Xi, Xp, Xb);
  fprintf(statfile," reaction parameter: r=%d\n", r);
  fprintf(statfile,
    " initial virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
    d0i, d0p, d0b);
  fprintf(statfile," initial average activity: avg_act=%.1f\n", avg_act);
#endif
}

void rc_init_GOP(np,nb)
int np,nb;
{
  R += (int) floor((1 + np + nb) * bit_rate / frame_rate + 0.5);
  Np = fieldpic ? 2*np+1 : np;
  Nb = fieldpic ? 2*nb : nb;

#ifdef DEBUG
  fprintf(statfile,"\nrate control: new group of pictures (GOP)\n");
  fprintf(statfile," target number of bits for GOP: R=%d\n",R);
  fprintf(statfile," number of P pictures in GOP: Np=%d\n",Np);
  fprintf(statfile," number of B pictures in GOP: Nb=%d\n",Nb);
#endif
}


/* Step 1: 为正在编码的帧计算目标比特数*/
void rc_init_pict(frame)
unsigned char *frame;
{
  double Tmin;

  switch (pict_type)
  {
  case I_TYPE:
    T = (int) floor(R/(1.0+Np*Xp/(Xi*1.0)+Nb*Xb/(Xi*1.4)) + 0.5);
    d = d0i;
    break;
  case P_TYPE:
    T = (int) floor(R/(Np+Nb*1.0*Xb/(1.4*Xp)) + 0.5);
    d = d0p;
    break;
  case B_TYPE:
    T = (int) floor(R/(Nb+Np*1.4*Xp/(1.0*Xb)) + 0.5);
    d = d0b;
    break;
  }

  Tmin = (int) floor(bit_rate/(8.0*frame_rate) + 0.5);

  if (T<Tmin)
    T = (int)Tmin;

  S = bitcount();
  Q = 0;

  calc_actj(frame);
  actsum = 0.0;

#ifdef DEBUG
  fprintf(statfile,"\nrate control: start of picture\n");
  fprintf(statfile," target number of bits: T=%d\n",T);
#endif
}

static void calc_actj(frame)
unsigned char *frame;
{
  int i,j,k;
  unsigned char *p;
  double actj,var;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      p = frame + ((pict_struct==BOTTOM_FIELD)?width:0) + i + width2*j;


      actj = var_sblk(p,width2);
      var = var_sblk(p+8,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2+8,width2);
      if (var<actj) actj = var;

      if (!fieldpic && !prog_seq)
      {
        /* field */
        var = var_sblk(p,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+8,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width+8,width<<1);
        if (var<actj) actj = var;
      }

      actj+= 1.0;

      mbinfo[k++].act = actj;
    }
}

void rc_update_pict()
{
  double X;

  S = bitcount() - S; /* total # of bits in picture */
  R-= S; /* remaining # of bits in GOP */
  X = (int) floor(S*((0.5*(double)Q)/(mb_width*mb_height2)) + 0.5);
  d+= S - T;
  avg_act = actsum/(mb_width*mb_height2);

  switch (pict_type)
  {
  case I_TYPE:
    Xi = (int)X;
    d0i = d;
    break;
  case P_TYPE:
    Xp = (int)X;
    d0p = d;
    Np--;
    break;
  case B_TYPE:
    Xb = (int)X;
    d0b = d;
    Nb--;
    break;
  }

#ifdef DEBUG
  fprintf(statfile,"\nrate control: end of picture\n");
  fprintf(statfile," actual number of bits: S=%d\n",S);
  fprintf(statfile," average quantization parameter Q=%.1f\n",
    (double)Q/(mb_width*mb_height2));
  fprintf(statfile," remaining number of bits in GOP: R=%d\n",R);
  fprintf(statfile,
    " global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
    Xi, Xp, Xb);
  fprintf(statfile,
    " virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
    d0i, d0p, d0b);
  fprintf(statfile," remaining number of P pictures in GOP: Np=%d\n",Np);
  fprintf(statfile," remaining number of B pictures in GOP: Nb=%d\n",Nb);
  fprintf(statfile," average activity: avg_act=%.1f\n", avg_act);
#endif
}

/* 计算最初的量化步长*/
int rc_start_mb()
{
  int mquant;

  if (q_scale_type)
  {
    mquant = (int) floor(2.0*d*31.0/r + 0.5);

    if (mquant<1)
      mquant = 1;
    if (mquant>112)
      mquant = 112;

    mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
  }
  else
  {
    mquant = (int) floor(d*31.0/r + 0.5);
    mquant <<= 1;

    if (mquant<2)
      mquant = 2;
    if (mquant>62)
      mquant = 62;

    prev_mquant = mquant;
  }


  return mquant;
}

/* Step 2: 测量虚拟缓存的大小*/
int rc_calc_mquant(j)
int j;
{
  int mquant;
  double dj, Qj, actj, N_actj;

  dj = d + (bitcount()-S) - j*(T/(mb_width*mb_height2));

  Qj = dj*31.0/r;

  actj = mbinfo[j].act;
  actsum+= actj;

  N_actj = (2.0*actj+avg_act)/(actj+2.0*avg_act);

  if (q_scale_type)
  {
    mquant = (int) floor(2.0*Qj*N_actj + 0.5);

    if (mquant<1)
      mquant = 1;
    if (mquant>112)
      mquant = 112;

    mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
  }
  else
  {
    mquant = (int) floor(Qj*N_actj + 0.5);
    mquant <<= 1;

    /* clip mquant to legal (linear) range */
    if (mquant<2)
      mquant = 2;
    if (mquant>62)
      mquant = 62;

    /* ignore small changes in mquant */
    if (mquant>=8 && (mquant-prev_mquant)>=-4 && (mquant-prev_mquant)<=4)
      mquant = prev_mquant;

    prev_mquant = mquant;
  }

  Q+= mquant; 


  return mquant;
}

/* 计算8x8 块的方差*/
static double var_sblk(p,lx)
unsigned char *p;
int lx;
{
	int i, j;
	unsigned int v, s, s2;

	s = s2 = 0;

	if(cpu_MMX)
	{
		_asm
		{
			mov esi, p					;// esi = p
			mov edx, lx					;// edx = lx
			mov eax, 0					;// eax = s
			mov edi, 0					;// edi = s2
			mov ecx, 16					;// ecx = 16
var_sblk__l1:
			movd mm0, [esi]				;// lower 4 bytes of mm0 = esi[0..3]
			movd mm1, [esi+4]			;// lower 4 bytes of mm1 = esi[4..7]
			movd mm2, [esi+8]			;// lower 4 bytes of mm2 = esi[8..11]
			movd mm3, [esi+12]			;// lower 4 bytes of mm3 = esi[12..15]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			movq mm4, mm0
			movq mm5, mm1
			movq mm6, mm2
			movq mm7, mm3
			paddw mm0, mm1
			paddw mm2, mm3
			pmaddwd mm4, mm4			;// mm4 = sqr for each word in mm4 and add them to make two dwords
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			pmaddwd mm5, mm5			;// mm5 = sqr for each word in mm5 and add them to make two dwords
			movq mm1, mm0
			pmaddwd mm6, mm6			;// mm6 = sqr for each word in mm6 and add them to make two dwords
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			pmaddwd mm7, mm7			;// mm7 = sqr for each word in mm6 and add them to make two dwords
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm4, mm5
			paddd mm0, mm1				;// mm0 += mm1
			paddd mm6, mm7
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			paddd mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			movd ebx, mm4				;// load lower dword of mm4 into ebx
			add edi, ebx				;// edi += ebx
			psrlq mm4, 32				;// shift mm4 to get upper dword
			movd ebx, mm4				;// load lower dword of mm4 into ebx
			add edi, ebx				;// edi += ebx

			add esi, edx				;// esi += edx
			dec ecx						;// decrement ecs
			jnz var_sblk__l1			;// loop while not zero
			mov s, eax					;// s = eax
			mov s2, edi					;// s2 = edi
			emms						;// empty MMX state
		}
	}
	else
	{
		for (j=0; j<8; j++)
		{
			for (i=0; i<8; i++)
			{
				v = *p++;
				s+= v;
				s2+= v*v;
			}
			p+= lx - 8;
		}
	}

	return s2/64.0 - (s/64.0)*(s/64.0);
}

/* 计算VBV 
 *
 * 如果溢出则产生警告信息
 */


static int bitcnt_EOP;

void vbv_end_of_picture()
{
  bitcnt_EOP = bitcount();
  bitcnt_EOP = (bitcnt_EOP + 7) & ~7; /* account for bit stuffing */
}


void calc_vbv_delay()
{
  double picture_delay;
  static double next_ip_delay; 
  static double decoding_time;

  if (pict_type == B_TYPE)
  {
    if (prog_seq)
    {
      if (!repeatfirst)
        picture_delay = 90000.0/frame_rate; /* 1 帧*/
      else
      {
        if (!topfirst)
          picture_delay = 90000.0*2.0/frame_rate; /* 2 帧 */
        else
          picture_delay = 90000.0*3.0/frame_rate; /* 3 帧 */
      }
    }
    else
    {
      /* interlaced */
      if (fieldpic)
        picture_delay = 90000.0/(2.0*frame_rate); /* 1 field */
      else
      {
        if (!repeatfirst)
          picture_delay = 90000.0*2.0/(2.0*frame_rate); /* 2 flds */
        else
          picture_delay = 90000.0*3.0/(2.0*frame_rate); /* 3 flds */
      }
    }
  }
  else
  {
    /* I o或 P 图像 */
    if (fieldpic)
    {
      if(topfirst==(pict_struct==TOP_FIELD))
      {
        /* first field */
        picture_delay = 90000.0/(2.0*frame_rate);
      }
      else
      {
        /* second field */
        picture_delay = next_ip_delay - 90000.0/(2.0*frame_rate);
      }
    }
    else
    {
      /* frame picture */
      picture_delay = next_ip_delay;
    }

    if (!fieldpic || topfirst!=(pict_struct==TOP_FIELD))
    {
      if (prog_seq)
      {
        if (!repeatfirst)
          next_ip_delay = 90000.0/frame_rate;
        else
        {
          if (!topfirst)
            next_ip_delay = 90000.0*2.0/frame_rate;
          else
            next_ip_delay = 90000.0*3.0/frame_rate;
        }
      }
      else
      {
        if (fieldpic)
          next_ip_delay = 90000.0/(2.0*frame_rate);
        else
        {
          if (!repeatfirst)
            next_ip_delay = 90000.0*2.0/(2.0*frame_rate);
          else
            next_ip_delay = 90000.0*3.0/(2.0*frame_rate);
        }
      }
    }
  }

  if (decoding_time==0.0)
  {
    picture_delay = ((vbv_buffer_size*16384*7)/8)*90000.0/bit_rate;
    if (fieldpic)
      next_ip_delay = (int)(90000.0/frame_rate+0.5);
  }

  /* VBV 检测 */

  if (!low_delay && (decoding_time < bitcnt_EOP*90000.0/bit_rate))
  {
    if (!quiet)
	{
		vbv_unflow++;
	}
  }

  decoding_time += picture_delay;

  vbv_delay = (int)(decoding_time - bitcount()*90000.0/bit_rate);

  if ((decoding_time - bitcnt_EOP*90000.0/bit_rate)
      > (vbv_buffer_size*16384)*90000.0/bit_rate)
  {
    if (!quiet)
	  vbv_ovflow++;
  }

#ifdef DEBUG
  fprintf(statfile,
    "\nvbv_delay=%d (bitcount=%d, decoding_time=%.2f, bitcnt_EOP=%d)\n",
    vbv_delay,bitcount(),decoding_time,bitcnt_EOP);
#endif

  if (vbv_delay<0)
  {
    if (!quiet)
		vbv_unflow++;
    vbv_delay = 0;
  }

  if (vbv_delay>65535)
  {
    if (!quiet)
		vbv_ovflow++;
    vbv_delay = 65535;
  }
}
