/* predict.c, 运动补偿预测*/


#include <stdio.h>
#include "global.h"

/* 函数声明*/
static void predict_mb(
  unsigned char *oldref[], unsigned char *newref[], unsigned char *cur[],
  int lx, int bx, int by, int pict_type, int pict_struct, int mb_type,
  int motion_type, int secondfield,
  int PMV[2][2][2], int mv_field_sel[2][2], int dmvector[2]);

static void pred(unsigned char *src[], int sfield,
  unsigned char *dst[], int dfield,
  int lx, int w, int h, int x, int y, int dx, int dy, int addflag);

static void pred_comp(unsigned char *src, unsigned char *dst,
  int lx, int w, int h, int x, int y, int dx, int dy, int addflag);

static void calc_DMV(int DMV[][2], int *dmvector, int mvx,
  int mvy);

static void clearblock(unsigned char *cur[], int i0, int j0);

static short PACKED_0[4] = { 0, 0, 0, 0 };
static short PACKED_1[4] = { 1, 1, 1, 1 };
static short PACKED_2[4] = { 2, 2, 2, 2 };


/* 为一个完整的图像形成预测
 *
 * reff: 前向预测参考帧
 * refb: 后向预测参考帧
 * cur:  目标帧
 * secondfield: 预测帧的第二场
 * mbi:  宏块信息
 *
 
 */

void predict(reff,refb,cur,secondfield,mbi)
unsigned char *reff[],*refb[],*cur[3];
int secondfield;
struct mbinfo *mbi;
{
  int i, j, k;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      predict_mb(reff,refb,cur,width,i,j,pict_type,pict_struct,
                 mbi[k].mb_type,mbi[k].motion_type,secondfield,
                 mbi[k].MV,mbi[k].mv_field_sel,mbi[k].dmvector);

      k++;
    }
}

/* 为一个宏块形成预测
 *
 * oldref: 前向预测的参考帧
 * newref: 后向预测的参考帧
 * cur:    目标帧
 * lx:     帧的宽度
 * bx,by:  要预测的宏块的图像 (场或帧) 坐标
 * pict_type: I, P 或 B
 * pict_struct: FRAME_PICTURE, TOP_FIELD, BOTTOM_FIELD
 * mb_type:     MB_FORWARD, MB_BACKWARD, MB_INTRA
 * motion_type: MC_FRAME, MC_FIELD, MC_16X8, MC_DMV
 * secondfield: 预测一帧的第二场
 * PMV[2][2][2]: 运动向量
 * mv_field_sel[2][2]: 运动向量场选择
 * dmvector: 差分运动向量
 *
 
 */

static void predict_mb(oldref,newref,cur,lx,bx,by,pict_type,pict_struct,
  mb_type,motion_type,secondfield,PMV,mv_field_sel,dmvector)
unsigned char *oldref[],*newref[],*cur[];
int lx;
int bx,by;
int pict_type;
int pict_struct;
int mb_type;
int motion_type;
int secondfield;
int PMV[2][2][2], mv_field_sel[2][2], dmvector[2];
{
  int addflag, currentfield;
  unsigned char **predframe;
  int DMV[2][2];

  if (mb_type&MB_INTRA)
  {
    clearblock(cur,bx,by);
    return;
  }

  addflag = 0; /* first prediction is stored, second is added and averaged */

  if ((mb_type & MB_FORWARD) || (pict_type==P_TYPE))
  {
    /* forward prediction, including zero MV in P pictures */

    if (pict_struct==FRAME_PICTURE)
    {
      /* frame picture */

      if ((motion_type==MC_FRAME) || !(mb_type & MB_FORWARD))
      {
        /* frame-based prediction in frame picture */
        pred(oldref,0,cur,0,
          lx,16,16,bx,by,PMV[0][0][0],PMV[0][0][1],0);
      }
      else if (motion_type==MC_FIELD)
      {
       

        /* top field prediction */
        pred(oldref,mv_field_sel[0][0],cur,0,
          lx<<1,16,8,bx,by>>1,PMV[0][0][0],PMV[0][0][1]>>1,0);

        /* bottom field prediction */
        pred(oldref,mv_field_sel[1][0],cur,1,
          lx<<1,16,8,bx,by>>1,PMV[1][0][0],PMV[1][0][1]>>1,0);
      }
      else if (motion_type==MC_DMV)
      {

        /* calculate derived motion vectors */
        calc_DMV(DMV,dmvector,PMV[0][0][0],PMV[0][0][1]>>1);

        /* predict top field from top field */
        pred(oldref,0,cur,0,
          lx<<1,16,8,bx,by>>1,PMV[0][0][0],PMV[0][0][1]>>1,0);

        /* predict bottom field from bottom field */
        pred(oldref,1,cur,1,
          lx<<1,16,8,bx,by>>1,PMV[0][0][0],PMV[0][0][1]>>1,0);

        /* predict and add to top field from bottom field */
        pred(oldref,1,cur,0,
          lx<<1,16,8,bx,by>>1,DMV[0][0],DMV[0][1],1);

        /* predict and add to bottom field from top field */
        pred(oldref,0,cur,1,
          lx<<1,16,8,bx,by>>1,DMV[1][0],DMV[1][1],1);
      }
      else
      {
        /* invalid motion_type in frame picture */
        if (!quiet)
          fprintf(stderr,"invalid motion_type\n");
      }
    }
    else /* TOP_FIELD or BOTTOM_FIELD */
    {
      /* field picture */

      currentfield = (pict_struct==BOTTOM_FIELD);

      if ((pict_type==P_TYPE) && secondfield
          && (currentfield!=mv_field_sel[0][0]))
        predframe = newref; /* same frame */
      else
        predframe = oldref; /* previous frame */

      if ((motion_type==MC_FIELD) || !(mb_type & MB_FORWARD))
      {
        /* field-based prediction in field picture */
        pred(predframe,mv_field_sel[0][0],cur,currentfield,
          lx<<1,16,16,bx,by,PMV[0][0][0],PMV[0][0][1],0);
      }
      else if (motion_type==MC_16X8)
      {
        /* 16 x 8 motion compensation in field picture */

        /* upper half */
        pred(predframe,mv_field_sel[0][0],cur,currentfield,
          lx<<1,16,8,bx,by,PMV[0][0][0],PMV[0][0][1],0);

        if ((pict_type==P_TYPE) && secondfield
            && (currentfield!=mv_field_sel[1][0]))
          predframe = newref; /* same frame */
        else
          predframe = oldref; /* previous frame */

        /* lower half */
        pred(predframe,mv_field_sel[1][0],cur,currentfield,
          lx<<1,16,8,bx,by+8,PMV[1][0][0],PMV[1][0][1],0);
      }
      else if (motion_type==MC_DMV)
      {
        
        /* determine which frame to use for prediction */
        if (secondfield)
          predframe = newref; /* same frame */
        else
          predframe = oldref; /* previous frame */

        /* calculate derived motion vectors */
        calc_DMV(DMV,dmvector,PMV[0][0][0],PMV[0][0][1]);

        /* predict from field of same parity */
        pred(oldref,currentfield,cur,currentfield,
          lx<<1,16,16,bx,by,PMV[0][0][0],PMV[0][0][1],0);

        /* predict from field of opposite parity */
        pred(predframe,!currentfield,cur,currentfield,
          lx<<1,16,16,bx,by,DMV[0][0],DMV[0][1],1);
      }
      else
      {
        if (!quiet)
          fprintf(stderr,"invalid motion_type\n");
      }
    }
    addflag = 1; 
  }

  if (mb_type & MB_BACKWARD)
  {
    /* backward prediction */

    if (pict_struct==FRAME_PICTURE)
    {
      /* frame picture */

      if (motion_type==MC_FRAME)
      {
        /* frame-based prediction in frame picture */
        pred(newref,0,cur,0,
          lx,16,16,bx,by,PMV[0][1][0],PMV[0][1][1],addflag);
      }
      else
      {
        

        /* top field prediction */
        pred(newref,mv_field_sel[0][1],cur,0,
          lx<<1,16,8,bx,by>>1,PMV[0][1][0],PMV[0][1][1]>>1,addflag);

        /* bottom field prediction */
        pred(newref,mv_field_sel[1][1],cur,1,
          lx<<1,16,8,bx,by>>1,PMV[1][1][0],PMV[1][1][1]>>1,addflag);
      }
    }
    else /* TOP_FIELD or BOTTOM_FIELD */
    {
      /* field picture */

      currentfield = (pict_struct==BOTTOM_FIELD);

      if (motion_type==MC_FIELD)
      {
        /* field-based prediction in field picture */
        pred(newref,mv_field_sel[0][1],cur,currentfield,
          lx<<1,16,16,bx,by,PMV[0][1][0],PMV[0][1][1],addflag);
      }
      else if (motion_type==MC_16X8)
      {
        /* 16 x 8 motion compensation in field picture */

        /* upper half */
        pred(newref,mv_field_sel[0][1],cur,currentfield,
          lx<<1,16,8,bx,by,PMV[0][1][0],PMV[0][1][1],addflag);

        /* lower half */
        pred(newref,mv_field_sel[1][1],cur,currentfield,
          lx<<1,16,8,bx,by+8,PMV[1][1][0],PMV[1][1][1],addflag);
      }
      else
      {
        /* invalid motion_type in field picture */
        if (!quiet)
          fprintf(stderr,"invalid motion_type\n");
      }
    }
  }
}

/* 预测矩形块
 *
 * src:     源帧(Y,U,V)
 * sfield:  源场选择(0: 帧或上半场, 1: 下半场)
 * dst:     目标帧(Y,U,V)
 * dfield:  目标场选择
 *
 * 下面的数值是亮度图像的参数
 * lx:      相邻像素的垂直距离
 * w,h:     块的宽度和高度
 * x,y:     目标块的坐标
 * dx,dy:   半像素运动向量
 * addflag: 存储或加预测
 */
static void pred(src,sfield,dst,dfield,lx,w,h,x,y,dx,dy,addflag)
unsigned char *src[];
int sfield;
unsigned char *dst[];
int dfield;
int lx;
int w, h;
int x, y;
int dx, dy;
int addflag;
{
  int cc;

  for (cc=0; cc<3; cc++)
  {
    if (cc==1)
    {
      /* scale for color components */
      if (chroma_format==CHROMA420)
      {
        /* vertical */
        h >>= 1; y >>= 1; dy /= 2;
      }
      if (chroma_format!=CHROMA444)
      {
        /* horizontal */
        w >>= 1; x >>= 1; dx /= 2;
        lx >>= 1;
      }
    }
    pred_comp(src[cc]+(sfield?lx>>1:0),dst[cc]+(dfield?lx>>1:0),
      lx,w,h,x,y,dx,dy,addflag);
  }
}

/* 低等级预测
 *
 * src:     源帧(Y,U,V)
 * dst:     目标帧(Y,U,V)
 *
 * lx:      相邻像素的垂直距离
 * w,h:     块的宽度和高度
 * x,y:     目标块的坐标
 * dx,dy:   半像素运动向量
 * addflag: 存储或加预测
 */

static void pred_comp(src,dst,lx,w,h,x,y,dx,dy,addflag)
unsigned char *src;
unsigned char *dst;
int lx;
int w, h;
int x, y;
int dx, dy;
int addflag;
{
	int xint, xh, yint, yh;
	int i, j;
	unsigned char *s, *d;

	/* half pel scaling */
	xint = dx>>1; /* integer part */
	xh = dx & 1;  /* half pel flag */
	yint = dy>>1;
	yh = dy & 1;

	/* origins */
	s = src + lx*(y+yint) + (x+xint); /* motion vector */
	d = dst + lx*y + x;

	if (!xh && !yh)
	{
		if (addflag)
		{
			if(cpu_MMX)
			{
				if(w == 8)
				{
					_asm
					{
						mov ecx, h					;// ecx = h
						mov edx, lx					;// edx = lx
						mov esi, s					;// esi = s
						mov edi, d					;// edi = d
pred_comp__l1:
						movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
						movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
						movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
						movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
						punpcklbw mm0, PACKED_0		;// unpack the lower 2 words into mm0
						punpcklbw mm1, PACKED_0		;// unpack the lower 2 words into mm1
						punpcklbw mm2, PACKED_0		;// unpack the lower 2 words into mm2
						punpcklbw mm3, PACKED_0		;// unpack the lower 2 words into mm3
						paddw mm0, mm1				;// mm0 += mm1
						paddw mm2, mm3				;// mm2 += mm3
						psrlw mm0, 1				;// mm0 >>= 1
						psrlw mm2, 1				;// mm2 >>= 1
						packuswb mm0, mm2			;// pack mm0 and mm2
						movq [edi], mm0				;// store mm0 into edi[0..7]

						add esi, edx				;// esi += edx
						add edi, edx				;// edi += edx
						dec ecx						;// decrement ecx
						jnz pred_comp__l1			;// loop while not zero
						emms						;// empty MMX state
					}
					return;
				}

				if(w == 16)
				{
					_asm
					{
						mov ecx, h					;// ecx = h
						mov edx, lx					;// edx = lx
						mov esi, s					;// esi = s
						mov edi, d					;// edi = d
pred_comp__l2:
						movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
						movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
						movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
						movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
						movd mm4, [esi+8]			;// lower 4 bytes into mm4 = esi[8..11]
						movd mm5, [edi+8]			;// lower 4 bytes into mm5 = edi[8..11]
						movd mm6, [esi+12]			;// lower 4 bytes into mm6 = esi[12..15]
						movd mm7, [edi+12]			;// lower 4 bytes into mm7 = edi[12..15]
						punpcklbw mm0, PACKED_0		;// unpack the lower 2 words into mm0
						punpcklbw mm1, PACKED_0		;// unpack the lower 2 words into mm1
						punpcklbw mm2, PACKED_0		;// unpack the lower 2 words into mm2
						punpcklbw mm3, PACKED_0		;// unpack the lower 2 words into mm3
						punpcklbw mm4, PACKED_0		;// unpack the lower 2 words into mm4
						punpcklbw mm5, PACKED_0		;// unpack the lower 2 words into mm5
						punpcklbw mm6, PACKED_0		;// unpack the lower 2 words into mm6
						punpcklbw mm7, PACKED_0		;// unpack the lower 2 words into mm7
						paddw mm0, mm1				;// mm0 += mm1
						paddw mm2, mm3				;// mm2 += mm3
						paddw mm4, mm5				;// mm4 += mm5
						paddw mm6, mm7				;// mm6 += mm7
						psrlw mm0, 1				;// mm0 >>= 1
						psrlw mm2, 1				;// mm2 >>= 1
						psrlw mm4, 1				;// mm4 >>= 1
						psrlw mm6, 1				;// mm6 >>= 1
						packuswb mm0, mm2			;// pack mm0 and mm2
						packuswb mm4, mm6			;// pack mm4 and mm6
						movq [edi], mm0				;// store mm0 into edi[0..7]
						movq [edi+8], mm4			;// store mm4 into edi[8..15]

						add esi, edx				;// esi += edx
						add edi, edx				;// edi += edx
						dec ecx						;// decrement ecx
						jnz pred_comp__l2			;// loop while not zero
						emms						;// empty MMX state
					}
					return;
				}
			}

			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (unsigned int)(d[i]+s[i]+1)>>1;

				s+= lx;
				d+= lx;
			}
		}
		else
		{
			if(cpu_MMX)
			{
				if(w == 8)
				{
					_asm
					{
						mov esi, s			;// esi = s
						mov edi, d			;// edi = d
						mov ecx, h			;// ecx = h
						mov edx, lx			;// edx = lx
pred_comp__l3:
						movq mm0, [esi]
						movq [edi], mm0		;// simply move from esi[0..7] to edi[0..7]
						add esi, edx		;// esi += edx
						add edi, edx		;// esi += edx
						dec ecx				;// decrement ecx
						jnz pred_comp__l3	;// loop while not zero
						emms				;// empty MMX state
					}
					return;
				}
				
				if(w == 16)
				{
					_asm
					{
						mov esi, s			;// esi = s
						mov edi, d			;// edi = d
						mov ecx, h			;// ecx = h
						mov edx, lx			;// edx = lx
pred_comp__l4:
						movq mm0, [esi]
						movq mm1, [esi+8]
						movq [edi], mm0		;// simply move from esi[0..7] to edi[0..7]
						movq [edi+8], mm1	;// simply move from esi[8..15] to edi[8..15]
						add esi, edx		;// esi += edx
						add edi, edx		;// esi += edx
						dec ecx				;// decrement ecx
						jnz pred_comp__l4	;// loop while not zero
						emms				;// empty MMX state
					}
					return;
				}
			}

			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = s[i];

				s+= lx;
				d+= lx;
			}
		}
	}
	else if (!xh && yh)
	if (addflag)
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov ecx, h					;// ecx = h
					mov edx, lx					;// edx = lx
pred_comp__l5:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+edx]			;// lower 4 bytes into mm1 = (esi + edx)[0..3]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+edx+4]		;// lower 4 bytes into mm3 = (esi + edx)[4..7]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
					punpcklbw mm1, PACKED_0		;// unpack the lower 2 bytes into mm1
					punpcklbw mm3, PACKED_0		;// unpack the lower 2 bytes into mm3
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					paddw mm0, mm1				;// mm0 += mm1
					paddw mm2, mm3				;// mm2 += mm3
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					movq [edi], mm0				;// store mm0 into edi[0..7]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l5			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov ecx, h					;// ecx = h
					mov edx, lx					;// edx = lx
pred_comp__l6:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+edx]			;// lower 4 bytes into mm1 = (esi + edx)[0..3]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+edx+4]		;// lower 4 bytes into mm3 = (esi + edx)[4..7]
					movd mm4, [esi+8]			;// lower 4 bytes into mm4 = esi[8..11]
					movd mm5, [esi+edx+8]		;// lower 4 bytes into mm5 = (esi + edx)[8..11]
					movd mm6, [esi+12]			;// lower 4 bytes into mm6 = esi[12..15]
					movd mm7, [esi+edx+12]		;// lower 4 bytes into mm7 = (esi + edx)[12..15]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					paddw mm4, PACKED_1			;// mm4 += mm5 + (1, 1, 1, 1)
					paddw mm6, PACKED_1			;// mm6 += mm7 + (1, 1, 1, 1)
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
					movd mm5, [edi+8]			;// lower 4 bytes into mm5 = edi[8..11]
					movd mm7, [edi+12]			;// lower 4 bytes into mm7 = edi[12..15]
					punpcklbw mm1, PACKED_0		;// unpack the lower 2 bytes into mm1
					punpcklbw mm3, PACKED_0		;// unpack the lower 2 bytes into mm3
					punpcklbw mm5, PACKED_0		;// unpack the lower 2 bytes into mm5
					punpcklbw mm7, PACKED_0		;// unpack the lower 2 bytes into mm7
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					paddw mm0, mm1				;// mm0 += mm1
					paddw mm2, mm3				;// mm2 += mm3
					paddw mm4, mm5				;// mm4 += mm5
					paddw mm6, mm7				;// mm6 += mm7
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					packuswb mm4, mm6			;// pack mm4 and mm6
					movq [edi], mm0				;// store mm0 into edi[0..7]
					movq [edi+8], mm4			;// store mm4 into edi[8..15]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l6			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (d[i] + ((unsigned int)(s[i]+s[i+lx]+1)>>1)+1)>>1;
				
			s+= lx;
			d+= lx;
		}
	}
	else
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov ecx, h					;// ecx = h
					mov edx, lx					;// edx = lx
pred_comp__l7:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+edx]			;// lower 4 bytes into mm1 = (esi + edx)[0..3]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+edx+4]		;// lower 4 bytes into mm3 = (esi + edx)[4..7]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					movq [edi], mm0				;// store mm0 into edi[0..7]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l7			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov ecx, h					;// ecx = h
					mov edx, lx					;// edx = lx
pred_comp__l8:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+edx]			;// lower 4 bytes into mm1 = (esi + edx)[0..3]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+edx+4]		;// lower 4 bytes into mm3 = (esi + edx)[4..7]
					movd mm4, [esi+8]			;// lower 4 bytes into mm4 = esi[8..11]
					movd mm5, [esi+edx+8]		;// lower 4 bytes into mm5 = (esi + edx)[8..11]
					movd mm6, [esi+12]			;// lower 4 bytes into mm6 = esi[12..15]
					movd mm7, [esi+edx+12]		;// lower 4 bytes into mm7 = (esi + edx)[12..15]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					paddw mm4, PACKED_1			;// mm4 += mm5 + (1, 1, 1, 1)
					paddw mm6, PACKED_1			;// mm6 += mm7 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					packuswb mm4, mm6			;// pack mm4 and mm6
					movq [edi], mm0				;// store mm0 into edi[0..7]
					movq [edi+8], mm4			;// store mm4 into edi[8..15]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l8			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (unsigned int)(s[i]+s[i+lx]+1)>>1;
				
			s+= lx;
			d+= lx;
		}
	}
	else if (xh && !yh)
	if (addflag)
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l9:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+5]			;// lower 4 bytes into mm3 = esi[5..8]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					paddw mm0, PACKED_1
					paddw mm2, PACKED_1
					paddw mm0, mm1				;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, mm3				;// mm2 += mm3 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					movq [edi], mm0				;// store mm0 into edi[0..7]
					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l9			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l10:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+5]			;// lower 4 bytes into mm3 = esi[5..8]
					movd mm4, [esi+8]			;// lower 4 bytes into mm4 = esi[8..11]
					movd mm5, [esi+9]			;// lower 4 bytes into mm5 = esi[9..12]
					movd mm6, [esi+12]			;// lower 4 bytes into mm6 = esi[12..15]
					movd mm7, [esi+13]			;// lower 4 bytes into mm7 = esi[13..16]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					paddw mm4, PACKED_1			;// mm4 += mm5 + (1, 1, 1, 1)
					paddw mm6, PACKED_1			;// mm6 += mm7 + (1, 1, 1, 1)
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm3, [edi+4]			;// lower 4 bytes into mm3 = edi[4..7]
					movd mm5, [edi+8]			;// lower 4 bytes into mm5 = edi[8..11]
					movd mm7, [edi+12]			;// lower 4 bytes into mm7 = edi[12..15]
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, PACKED_1
					paddw mm2, PACKED_1
					paddw mm4, PACKED_1
					paddw mm6, PACKED_1
					paddw mm0, mm1				;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, mm3				;// mm2 += mm3 + (1, 1, 1, 1)
					paddw mm4, mm5				;// mm4 += mm5 + (1, 1, 1, 1)
					paddw mm6, mm7				;// mm6 += mm7 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					packuswb mm4, mm6			;// pack mm4 and mm6
					movq [edi], mm0				;// store mm0 into edi[0..7]
					movq [edi+8], mm4			;// store mm4 into edi[8..15]
					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l10			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (d[i] + ((unsigned int)(s[i]+s[i+1]+1)>>1)+1)>>1;
				
			s+= lx;
			d+= lx;
		}
	}
	else
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l11:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+5]			;// lower 4 bytes into mm3 = esi[0..3]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					movq [edi], mm0				;// store mm0 into edi[0..7]
					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l11			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l12:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+4]			;// lower 4 bytes into mm2 = esi[4..7]
					movd mm3, [esi+5]			;// lower 4 bytes into mm3 = esi[0..3]
					movd mm4, [esi+8]			;// lower 4 bytes into mm4 = esi[8..11]
					movd mm5, [esi+9]			;// lower 4 bytes into mm5 = esi[9..12]
					movd mm6, [esi+12]			;// lower 4 bytes into mm6 = esi[12..15]
					movd mm7, [esi+13]			;// lower 4 bytes into mm7 = esi[13..16]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, PACKED_1			;// mm0 += mm1 + (1, 1, 1, 1)
					paddw mm2, PACKED_1			;// mm2 += mm3 + (1, 1, 1, 1)
					paddw mm4, PACKED_1			;// mm4 += mm5 + (1, 1, 1, 1)
					paddw mm6, PACKED_1			;// mm6 += mm7 + (1, 1, 1, 1)
					psrlw mm0, 1				;// mm0 >>= 1
					psrlw mm2, 1				;// mm2 >>= 1
					psrlw mm4, 1				;// mm4 >>= 1
					psrlw mm6, 1				;// mm6 >>= 1
					packuswb mm0, mm2			;// pack mm0 and mm2
					packuswb mm4, mm6			;// pack mm4 and mm6
					movq [edi], mm0				;// store mm0 into edi[0..7]
					movq [edi+8], mm4			;// store mm4 into edi[8..15]
					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l12			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (unsigned int)(s[i]+s[i+1]+1)>>1;

			s+= lx;
			d+= lx;
		}
	}
	else /* if (xh && yh) */
	if (addflag)
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l13:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+edx]			;// lower 4 bytes into mm2 = (esi + edx)[0..3]
					movd mm3, [esi+edx+1]		;// lower 4 bytes into mm3 = (esi + edx)[1..4]
					movd mm4, [esi+4]			;// lower 4 bytes into mm4 = esi[4..7]
					movd mm5, [esi+5]			;// lower 4 bytes into mm5 = esi[5..8]
					movd mm6, [esi+edx+4]		;// lower 4 bytes into mm6 = (esi + edx)[4..7]
					movd mm7, [esi+edx+5]		;// lower 4 bytes into mm7 = (esi + edx)[5..8]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm5, [edi+4]			;// lower 4 bytes into mm5 = edi[4..7]
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					psrlw mm0, 2
					psrlw mm4, 2
					paddw mm0, PACKED_1
					paddw mm4, PACKED_1
					paddw mm0, mm1
					paddw mm4, mm5
					psrlw mm0, 1				;// mm0 = (mm0 >> 2) + (1, 1, 1, 1) + mm1
					psrlw mm4, 1				;// mm4 = (mm4 >> 2) + (1, 1, 1, 1) + mm5
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi], mm0				;// store mm0 into edi[0..7]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l13			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l14:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+edx]			;// lower 4 bytes into mm2 = (esi + edx)[0..3]
					movd mm3, [esi+edx+1]		;// lower 4 bytes into mm3 = (esi + edx)[1..4]
					movd mm4, [esi+4]			;// lower 4 bytes into mm4 = esi[4..7]
					movd mm5, [esi+5]			;// lower 4 bytes into mm5 = esi[5..8]
					movd mm6, [esi+edx+4]		;// lower 4 bytes into mm6 = (esi + edx)[4..7]
					movd mm7, [esi+edx+5]		;// lower 4 bytes into mm7 = (esi + edx)[5..8]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					movd mm1, [edi]				;// lower 4 bytes into mm1 = edi[0..3]
					movd mm5, [edi+4]			;// lower 4 bytes into mm5 = edi[4..7]
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					psrlw mm0, 2
					psrlw mm4, 2
					paddw mm0, PACKED_1
					paddw mm4, PACKED_1
					paddw mm0, mm1
					paddw mm4, mm5
					psrlw mm0, 1				;// mm0 = (mm0 >> 2) + (1, 1, 1, 1) + mm1
					psrlw mm4, 1				;// mm4 = (mm4 >> 2) + (1, 1, 1, 1) + mm5
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi], mm0				;// store mm0 into edi[0..7]

					movd mm0, [esi+8]			;// lower 4 bytes into mm0 = esi[8..11]
					movd mm1, [esi+9]			;// lower 4 bytes into mm1 = esi[9..12]
					movd mm2, [esi+edx+8]		;// lower 4 bytes into mm2 = (esi + edx)[8..11]
					movd mm3, [esi+edx+9]		;// lower 4 bytes into mm3 = (esi + edx)[9..12]
					movd mm4, [esi+12]			;// lower 4 bytes into mm4 = esi[12..15]
					movd mm5, [esi+13]			;// lower 4 bytes into mm5 = esi[13..16]
					movd mm6, [esi+edx+12]		;// lower 4 bytes into mm6 = (esi + edx)[12..15]
					movd mm7, [esi+edx+13]		;// lower 4 bytes into mm7 = (esi + edx)[13..16]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					movd mm1, [edi+8]			;// lower 4 bytes into mm1 = edi[8..11]
					movd mm5, [edi+12]			;// lower 4 bytes into mm5 = edi[12..15]
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					psrlw mm0, 2
					psrlw mm4, 2
					paddw mm0, PACKED_1
					paddw mm4, PACKED_1
					paddw mm0, mm1
					paddw mm4, mm5
					psrlw mm0, 1				;// mm0 = (mm0 >> 2) + (1, 1, 1, 1) + mm1
					psrlw mm4, 1				;// mm4 = (mm4 >> 2) + (1, 1, 1, 1) + mm5
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi+8], mm0			;// store mm0 into edi[8..15]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l14			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (d[i] + ((unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2)+1)>>1;

			s+= lx;
			d+= lx;
		}
	}
	else
	{
		if(cpu_MMX)
		{
			if(w == 8)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l15:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+edx]			;// lower 4 bytes into mm2 = (esi + edx)[0..3]
					movd mm3, [esi+edx+1]		;// lower 4 bytes into mm3 = (esi + edx)[1..4]
					movd mm4, [esi+4]			;// lower 4 bytes into mm4 = esi[4..7]
					movd mm5, [esi+5]			;// lower 4 bytes into mm5 = esi[5..8]
					movd mm6, [esi+edx+4]		;// lower 4 bytes into mm6 = (esi + edx)[4..7]
					movd mm7, [esi+edx+5]		;// lower 4 bytes into mm7 = (esi + edx)[5..8]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					psrlw mm0, 2				;// mm0 >>= 2
					psrlw mm4, 2				;// mm4 >>= 2
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi], mm0				;// store mm0 into edi[0..7]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l15			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}

			if(w == 16)
			{
				_asm
				{
					mov esi, s					;// esi = s
					mov edi, d					;// edi = d
					mov edx, lx					;// edx = lx
					mov ecx, h					;// ecx = h
pred_comp__l16:
					movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
					movd mm1, [esi+1]			;// lower 4 bytes into mm1 = esi[1..4]
					movd mm2, [esi+edx]			;// lower 4 bytes into mm2 = (esi + edx)[0..3]
					movd mm3, [esi+edx+1]		;// lower 4 bytes into mm3 = (esi + edx)[1..4]
					movd mm4, [esi+4]			;// lower 4 bytes into mm4 = esi[4..7]
					movd mm5, [esi+5]			;// lower 4 bytes into mm5 = esi[5..8]
					movd mm6, [esi+edx+4]		;// lower 4 bytes into mm6 = (esi + edx)[4..7]
					movd mm7, [esi+edx+5]		;// lower 4 bytes into mm7 = (esi + edx)[5..8]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					psrlw mm0, 2				;// mm0 >>= 2
					psrlw mm4, 2				;// mm4 >>= 2
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi], mm0				;// store mm0 into edi[0..7]

					movd mm0, [esi+8]			;// lower 4 bytes into mm0 = esi[8..11]
					movd mm1, [esi+9]			;// lower 4 bytes into mm1 = esi[9..12]
					movd mm2, [esi+edx+8]		;// lower 4 bytes into mm2 = (esi + edx)[8..11]
					movd mm3, [esi+edx+9]		;// lower 4 bytes into mm3 = (esi + edx)[9..12]
					movd mm4, [esi+12]			;// lower 4 bytes into mm4 = esi[12..15]
					movd mm5, [esi+13]			;// lower 4 bytes into mm5 = esi[13..16]
					movd mm6, [esi+edx+12]		;// lower 4 bytes into mm6 = (esi + edx)[12..15]
					movd mm7, [esi+edx+13]		;// lower 4 bytes into mm7 = (esi + edx)[13..16]
					punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
					punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
					punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
					punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
					punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
					punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
					punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
					punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
					paddw mm0, mm1
					paddw mm2, mm3
					paddw mm4, mm5
					paddw mm6, mm7
					paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
					paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
					paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
					paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
					psrlw mm0, 2				;// mm0 >>= 2
					psrlw mm4, 2				;// mm0 >>= 2
					packuswb mm0, mm4			;// pack mm0 and mm4
					movq [edi+8], mm0			;// store mm0 into edi[8..15]

					add esi, edx				;// esi += edx
					add edi, edx				;// edi += edx
					dec ecx						;// decrement ecx
					jnz pred_comp__l16			;// loop while not zero
					emms						;// empty MMX state
				}
				return;
			}
		}

		for (j=0; j<h; j++)
		{
			for (i=0; i<w; i++)
				d[i] = (unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2;

			s+= lx;
			d+= lx;
		}
	}
}



static void calc_DMV(DMV,dmvector,mvx,mvy)
int DMV[][2];
int *dmvector;
int mvx, mvy;
{
  if (pict_struct==FRAME_PICTURE)
  {
    if (topfirst)
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
    }
    else
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
    }
  }
  else
  {
    /* vector for prediction from field of opposite 'parity' */
    DMV[0][0] = ((mvx+(mvx>0))>>1) + dmvector[0];
    DMV[0][1] = ((mvy+(mvy>0))>>1) + dmvector[1];

    /* correct for vertical field shift */
    if (pict_struct==TOP_FIELD)
      DMV[0][1]--;
    else
      DMV[0][1]++;
  }
}

static void clearblock(cur,i0,j0)
unsigned char *cur[];
int i0, j0;
{
  int i, j, w, h;
  unsigned char *p;

  p = cur[0] + ((pict_struct==BOTTOM_FIELD) ? width : 0) + i0 + width2*j0;

  for (j=0; j<16; j++)
  {
    for (i=0; i<16; i++)
      p[i] = 128;
    p+= width2;
  }

  w = h = 16;

  if (chroma_format!=CHROMA444)
  {
    i0>>=1; w>>=1;
  }

  if (chroma_format==CHROMA420)
  {
    j0>>=1; h>>=1;
  }

  p = cur[1] + ((pict_struct==BOTTOM_FIELD) ? chrom_width : 0) + i0
             + chrom_width2*j0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<w; i++)
      p[i] = 128;
    p+= chrom_width2;
  }

  p = cur[2] + ((pict_struct==BOTTOM_FIELD) ? chrom_width : 0) + i0
             + chrom_width2*j0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<w; i++)
      p[i] = 128;
    p+= chrom_width2;
  }
}
