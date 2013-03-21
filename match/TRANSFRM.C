/* transfrm.c,  前向/后向变换*/


#include <stdio.h>
#include <math.h>
#include "global.h"

static short PACKED_0[4] = { 0, 0, 0, 0 };
static short PACKED_128[4] = { 128, 128, 128, 128 };
static unsigned char PACKED_128B[8] = { 128, 128, 128, 128, 128, 128, 128, 128 };

static void add_pred(unsigned char *pred, unsigned char *cur,
  int lx, short *blk);
static void sub_pred(unsigned char *pred, unsigned char *cur,
  int lx, short *blk);

void transform(pred,cur,mbi,blocks)
unsigned char *pred[], *cur[];
struct mbinfo *mbi;
short blocks[][64];
{
  int i, j, i1, j1, k, n, cc, offs, lx;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      for (n=0; n<block_count; n++)
      {
        cc = (n<4) ? 0 : (n&1)+1; /* color component index */
        if (cc==0)
        {
          /* luminance */
          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
          {
            /* field DCT */
            offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
            lx = width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
            lx = width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += width;
        }
        else
        {
          /* chrominance */

          /* scale coordinates */
          i1 = (chroma_format==CHROMA444) ? i : i>>1;
          j1 = (chroma_format!=CHROMA420) ? j : j>>1;

          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type
              && (chroma_format!=CHROMA420))
          {
            /* field DCT */
            offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
            lx = chrom_width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
            lx = chrom_width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += chrom_width;
        }

        sub_pred(pred[cc]+offs,cur[cc]+offs,lx,blocks[k*block_count+n]);
        fdct(blocks[k*block_count+n]);
      }

      k++;
    }
}

/* 反向变换预测误差和加预测*/
void itransform(pred,cur,mbi,blocks)
unsigned char *pred[],*cur[];
struct mbinfo *mbi;
short blocks[][64];
{
  int i, j, i1, j1, k, n, cc, offs, lx;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      for (n=0; n<block_count; n++)
      {
        cc = (n<4) ? 0 : (n&1)+1; /* color component index */

        if (cc==0)
        {
          /* luminance */
          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
          {
            /* field DCT */
            offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
            lx = width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
            lx = width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += width;
        }
        else
        {
          /* chrominance */

          /* scale coordinates */
          i1 = (chroma_format==CHROMA444) ? i : i>>1;
          j1 = (chroma_format!=CHROMA420) ? j : j>>1;

          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type
              && (chroma_format!=CHROMA420))
          {
            /* field DCT */
            offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
            lx = chrom_width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
            lx = chrom_width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += chrom_width;
        }

        idct(blocks[k*block_count+n]);
        add_pred(pred[cc]+offs,cur[cc]+offs,lx,blocks[k*block_count+n]);
      }

      k++;
    }
}

/* 加预测和预测误差*/
static void add_pred(pred,cur,lx,blk)
unsigned char *pred, *cur;
int lx;
short *blk;
{
	int i, j;

	if(cpu_MMX)
	{
		// I do loop unrolling on the inner loop and the outer loop !
		_asm
		{
			mov esi, blk				;// esi = blk
			mov ebx, pred				;// ebx = pred
			mov edi, cur				;// edi = cur
			mov edx, lx					;// edx = lx
			lea ecx, [2*edx]			;// ecx = 2 * lx
			movq mm0, [esi]				;// mm0 = 4 words into esi[0..7]
			movq mm1, [esi+8]			;// mm1 = 4 words into esi[8..15]
			movq mm2, [ebx]				;// mm2 = 4 words into ebx[0..7]
			movq mm3, [esi+16]			;// mm3 = 4 words into esi[16..23]
			movq mm4, [esi+24]			;// mm4 = 4 words into esi[24..31]
			movq mm5, [ebx+edx]			;// mm5 = 4 words into (ebx + edx)[0..7]
			movq mm6, mm2
			movq mm7, mm5
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm6, PACKED_0		;// unpack the upper 4 bytes into mm6
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			paddw mm0, mm2				;// mm0 += mm2
			paddw mm1, mm6				;// mm1 += mm6
			paddw mm3, mm5				;// mm3 += mm5
			paddw mm4, mm7				;// mm4 += mm7
			psubw mm0, PACKED_128		;// convert mm0 from unsigned to signed by substracting 128
			psubw mm1, PACKED_128		;// convert mm1 from unsigned to signed by substracting 128
			psubw mm3, PACKED_128		;// convert mm3 from unsigned to signed by substracting 128
			psubw mm4, PACKED_128		;// convert mm4 from unsigned to signed by substracting 128
			packsswb mm0, mm1			;// pack mm0 and mm1 to get only 8 signed bytes into mm0
			packsswb mm3, mm4			;// pack mm3 and mm4 to get only 8 signed bytes into mm3
			paddb mm0, PACKED_128B		;// reverse the effect of convertion to signed
			paddb mm3, PACKED_128B		;// reverse the effect of convertion to signed
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi], mm0				;// store mm0
			movq [edi+edx], mm3			;// store mm3
			movq mm0, [esi+32]			;// mm0 = 4 words into esi[32..39]
			movq mm1, [esi+40]			;// mm1 = 4 words into esi[40..47]
			movq mm2, [ebx]				;// mm2 = 4 words into ebx[0..7]
			movq mm3, [esi+48]			;// mm3 = 4 words into esi[48..55]
			movq mm4, [esi+56]			;// mm4 = 4 words into esi[56..63]
			movq mm5, [ebx+edx]			;// mm5 = 4 words into (ebx + edx)[0..7]
			add edi, ecx				;// edi += 2 * lx
			movq mm6, mm2
			movq mm7, mm5
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm6, PACKED_0		;// unpack the upper 4 bytes into mm6
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			paddw mm0, mm2				;// mm0 += mm2
			paddw mm1, mm6				;// mm1 += mm6
			paddw mm3, mm5				;// mm3 += mm5
			paddw mm4, mm7				;// mm4 += mm7
			psubw mm0, PACKED_128		;// convert mm0 from unsigned to signed by substracting 128
			psubw mm1, PACKED_128		;// convert mm1 from unsigned to signed by substracting 128
			psubw mm3, PACKED_128		;// convert mm3 from unsigned to signed by substracting 128
			psubw mm4, PACKED_128		;// convert mm4 from unsigned to signed by substracting 128
			packsswb mm0, mm1			;// pack mm0 and mm1 to get only 8 signed bytes into mm0
			packsswb mm3, mm4			;// pack mm3 and mm4 to get only 8 signed bytes into mm3
			paddb mm0, PACKED_128B		;// reverse the effect of convertion to signed
			paddb mm3, PACKED_128B		;// reverse the effect of convertion to signed
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi], mm0				;// store mm0
			movq [edi+edx], mm3			;// store mm3
			movq mm0, [esi+64]			;// mm0 = 4 words into esi[64..71]
			movq mm1, [esi+72]			;// mm1 = 4 words into esi[72..79]
			movq mm2, [ebx]				;// mm2 = 4 words into ebx[0..7]
			movq mm3, [esi+80]			;// mm3 = 4 words into esi[80..87]
			movq mm4, [esi+88]			;// mm4 = 4 words into esi[88..95]
			movq mm5, [ebx+edx]			;// mm5 = 4 words into (ebx + edx)[0..7]
			add edi, ecx				;// edi += 2 * lx
			movq mm6, mm2
			movq mm7, mm5
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm6, PACKED_0		;// unpack the upper 4 bytes into mm6
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			paddw mm0, mm2				;// mm0 += mm2
			paddw mm1, mm6				;// mm1 += mm6
			paddw mm3, mm5				;// mm3 += mm5
			paddw mm4, mm7				;// mm4 += mm7
			psubw mm0, PACKED_128		;// convert mm0 from unsigned to signed by substracting 128
			psubw mm1, PACKED_128		;// convert mm1 from unsigned to signed by substracting 128
			psubw mm3, PACKED_128		;// convert mm3 from unsigned to signed by substracting 128
			psubw mm4, PACKED_128		;// convert mm4 from unsigned to signed by substracting 128
			packsswb mm0, mm1			;// pack mm0 and mm1 to get only 8 signed bytes into mm0
			packsswb mm3, mm4			;// pack mm3 and mm4 to get only 8 signed bytes into mm3
			paddb mm0, PACKED_128B		;// reverse the effect of convertion to signed
			paddb mm3, PACKED_128B		;// reverse the effect of convertion to signed
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi], mm0				;// store mm0
			movq [edi+edx], mm3			;// store mm3
			movq mm0, [esi+96]			;// mm0 = 4 words into esi[96..103]
			movq mm1, [esi+104]			;// mm1 = 4 words into esi[104..111]
			movq mm2, [ebx]				;// mm2 = 4 words into ebx[0..7]
			movq mm3, [esi+112]			;// mm3 = 4 words into esi[112..119]
			movq mm4, [esi+120]			;// mm4 = 4 words into esi[120..127]
			movq mm5, [ebx+edx]			;// mm5 = 4 words into (ebx + edx)[0..7]
			add edi, ecx				;// edi += 2 * lx
			movq mm6, mm2
			movq mm7, mm5
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm6, PACKED_0		;// unpack the upper 4 bytes into mm6
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			paddw mm0, mm2				;// mm0 += mm2
			paddw mm1, mm6				;// mm1 += mm6
			paddw mm3, mm5				;// mm3 += mm5
			paddw mm4, mm7				;// mm4 += mm7
			psubw mm0, PACKED_128		;// convert mm0 from unsigned to signed by substracting 128
			psubw mm1, PACKED_128		;// convert mm1 from unsigned to signed by substracting 128
			psubw mm3, PACKED_128		;// convert mm3 from unsigned to signed by substracting 128
			psubw mm4, PACKED_128		;// convert mm4 from unsigned to signed by substracting 128
			packsswb mm0, mm1			;// pack mm0 and mm1 to get only 8 signed bytes into mm0
			packsswb mm3, mm4			;// pack mm3 and mm4 to get only 8 signed bytes into mm3
			paddb mm0, PACKED_128B		;// reverse the effect of convertion to signed
			paddb mm3, PACKED_128B		;// reverse the effect of convertion to signed
			movq [edi], mm0				;// store mm0
			movq [edi+edx], mm3			;// store mm3
			emms						;// empty MMX state
		}
	}
	else
	{
		for (j=0; j<8; j++)
		{
			for (i=0; i<8; i++)
				cur[i] = clp[blk[i] + pred[i]];

			blk+= 8;
			cur+= lx;
			pred+= lx;
		}
	}
}

/* 从块数据中减去预测信息*/
static void sub_pred(pred,cur,lx,blk)
unsigned char *pred, *cur;
int lx;
short *blk;
{
	int i, j;

	if(cpu_MMX)
	{
		_asm
		{
			mov esi, cur				;// esi = cur
			mov ebx, pred				;// ebx = pred
			mov edi, blk				;// edi = blk
			mov edx, lx					;// edx = lx
			lea ecx, [2*edx]			;// ecx = 2 * lx
			movq mm0, [esi]				;// mm0 = 8 bytes into esi[0..7]
			movq mm2, [ebx]				;// mm2 = 8 bytes into ebx[0..7]
			movq mm4, [esi+edx]			;// mm4 = 8 bytes into (esi + edx)[0..7]
			movq mm6, [ebx+edx]			;// mm6 = 8 bytes into (ebx + edx)[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			movq mm7, mm6
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpckhbw mm1, PACKED_0		;// unpack the upper 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm3, PACKED_0		;// unpack the upper 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, PACKED_0		;// unpack the upper 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			psubw mm0, mm2				;// mm0 -= mm2
			psubw mm1, mm3				;// mm1 -= mm3
			psubw mm4, mm6				;// mm4 -= mm6
			psubw mm5, mm7				;// mm5 -= mm7
			add esi, ecx				;// esi += 2 * lx
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi], mm0				;// store mm0 into edi[0..7]
			movq [edi+8], mm1			;// store mm1 into edi[8..15]
			movq [edi+16], mm4			;// store mm4 into edi[16..23]
			movq [edi+24], mm5			;// store mm5 into edi[24..31]
			movq mm0, [esi]				;// mm0 = 8 bytes into esi[0..7]
			movq mm2, [ebx]				;// mm2 = 8 bytes into ebx[0..7]
			movq mm4, [esi+edx]			;// mm4 = 8 bytes into (esi + edx)[0..7]
			movq mm6, [ebx+edx]			;// mm6 = 8 bytes into (ebx + edx)[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			movq mm7, mm6
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpckhbw mm1, PACKED_0		;// unpack the upper 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm3, PACKED_0		;// unpack the upper 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, PACKED_0		;// unpack the upper 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			psubw mm0, mm2				;// mm0 -= mm2
			psubw mm1, mm3				;// mm1 -= mm3
			psubw mm4, mm6				;// mm4 -= mm6
			psubw mm5, mm7				;// mm5 -= mm7
			add esi, ecx				;// esi += 2 * lx
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi+32], mm0			;// store mm0 into edi[32..39]
			movq [edi+40], mm1			;// store mm1 into edi[40..47]
			movq [edi+48], mm4			;// store mm4 into edi[48..55]
			movq [edi+56], mm5			;// store mm5 into edi[56..63]
			movq mm0, [esi]				;// mm0 = 8 bytes into esi[0..7]
			movq mm2, [ebx]				;// mm2 = 8 bytes into ebx[0..7]
			movq mm4, [esi+edx]			;// mm4 = 8 bytes into (esi + edx)[0..7]
			movq mm6, [ebx+edx]			;// mm6 = 8 bytes into (ebx + edx)[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			movq mm7, mm6
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpckhbw mm1, PACKED_0		;// unpack the upper 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm3, PACKED_0		;// unpack the upper 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, PACKED_0		;// unpack the upper 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			psubw mm0, mm2				;// mm0 -= mm2
			psubw mm1, mm3				;// mm1 -= mm3
			psubw mm4, mm6				;// mm4 -= mm6
			psubw mm5, mm7				;// mm5 -= mm7
			add esi, ecx				;// esi += 2 * lx
			add ebx, ecx				;// ebx += 2 * lx
			movq [edi+64], mm0			;// store mm0 into edi[64..71]
			movq [edi+72], mm1			;// store mm1 into edi[72..79]
			movq [edi+80], mm4			;// store mm4 into edi[80..87]
			movq [edi+88], mm5			;// store mm5 into edi[88..95]
			movq mm0, [esi]				;// mm0 = 8 bytes into esi[0..7]
			movq mm2, [ebx]				;// mm2 = 8 bytes into ebx[0..7]
			movq mm4, [esi+edx]			;// mm4 = 8 bytes into (esi + edx)[0..7]
			movq mm6, [ebx+edx]			;// mm6 = 8 bytes into (ebx + edx)[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			movq mm7, mm6
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpckhbw mm1, PACKED_0		;// unpack the upper 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpckhbw mm3, PACKED_0		;// unpack the upper 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, PACKED_0		;// unpack the upper 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpckhbw mm7, PACKED_0		;// unpack the upper 4 bytes into mm7
			psubw mm0, mm2				;// mm0 -= mm2
			psubw mm1, mm3				;// mm1 -= mm3
			psubw mm4, mm6				;// mm4 -= mm6
			psubw mm5, mm7				;// mm5 -= mm7
			movq [edi+96], mm0			;// store mm0 into edi[96..103]
			movq [edi+104], mm1			;// store mm1 into edi[104..111]
			movq [edi+112], mm4			;// store mm4 into edi[112..119]
			movq [edi+120], mm5			;// store mm5 into edi[120..127]
			emms						;// empty MMX state
		}
	}
	else
	{
		for (j=0; j<8; j++)
		{
			for (i=0; i<8; i++)
				blk[i] = cur[i] - pred[i];

			blk+= 8;
			cur+= lx;
			pred+= lx;
		}
	}
}

/*
 * 选择帧DCT或场DCT
 */
void dct_type_estimation(pred,cur,mbi)
unsigned char *pred,*cur;
struct mbinfo *mbi;
{
  short blk0[128], blk1[128];
  int i, j, i0, j0, k, offs, s0, s1, sq0, sq1, s01;
  double d, r;

  k = 0;

  for (j0=0; j0<height2; j0+=16)
    for (i0=0; i0<width; i0+=16)
    {
      if (frame_pred_dct || pict_struct!=FRAME_PICTURE)
        mbi[k].dct_type = 0;
      else
      {
        for (j=0; j<8; j++)
        {
          offs = width*((j<<1)+j0) + i0;
          for (i=0; i<16; i++)
          {
            blk0[16*j+i] = cur[offs] - pred[offs];
            blk1[16*j+i] = cur[offs+width] - pred[offs+width];
            offs++;
          }
        }
        /* correlate fields */
        s0=s1=sq0=sq1=s01=0;

        for (i=0; i<128; i++)
        {
          s0+= blk0[i];
          sq0+= blk0[i]*blk0[i];
          s1+= blk1[i];
          sq1+= blk1[i]*blk1[i];
          s01+= blk0[i]*blk1[i];
        }

        d = (sq0-(s0*s0)/128.0)*(sq1-(s1*s1)/128.0);

        if (d>0.0)
        {
          r = (s01-(s0*s1)/128.0)/sqrt(d);
          if (r>0.5)
            mbi[k].dct_type = 0; /* frame DCT */
          else
            mbi[k].dct_type = 1; /* field DCT */
        }
        else
          mbi[k].dct_type = 1; /* field DCT */
      }
      k++;
    }
}
