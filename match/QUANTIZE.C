/* quantize.c, 量化和逆量化                        */



#include <stdio.h>
#include "global.h"

// 下面的这些常量将用在与MMX相关的代码中
static short PACKED_0xFF00[4] = { 0xff00U, 0xff00U, 0xff00U, 0xff00U };
static unsigned short PACKED_32768[4] = { 32768, 32768, 32768, 32768 };
static unsigned short PACKED_30720[4] = { 30720, 30720, 30720, 30720 };
static unsigned short PACKED_61440[4] = { 61440, 61440, 61440, 61440 };
static unsigned short PACKED_2048[4] = { 2048, 2048, 2048, 2048 };


/* 该量化器在趋近于0时采用1/8的步长（DC系数除外）
 * 
 */
int quant_intra(src,dst,dc_prec,quant_mat,recip_quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned char *quant_mat;
unsigned short *recip_quant_mat;
int mquant;
{
	int i;
	int x, y, d;
	static short mquantm3p2div4[4];
	static unsigned short recip_mquant[4];
    /*下面的代码是针对MMX处理器或快速量化的*/
	if(cpu_MMX && fastQuantization)
	{
		mquantm3p2div4[0] = ((mquant*3)+2)>>2;
		mquantm3p2div4[1] = mquantm3p2div4[0];
		mquantm3p2div4[2] = mquantm3p2div4[0];
		mquantm3p2div4[3] = mquantm3p2div4[0];	
		recip_mquant[0] = (unsigned short)((16384.0 + (double) mquant/2.0)/mquant);
		recip_mquant[1] = recip_mquant[0];
		recip_mquant[2] = recip_mquant[0];
		recip_mquant[3] = recip_mquant[0];

		x = src[0];
		_asm
		{
			mov esi, src				;// esi = src
			mov edx, recip_quant_mat	;// edx = recip_quant_mat
			mov edi, dst				;// edi = dst
			mov ecx, 8					;// ecx = 8
quant_intra__l1:
			movq mm0, [esi]				;// mm0 = 4 words into esi[0..7]
			movq mm1, [esi+8]			;// mm1 = 4 words into esi[8..15]
			movq mm2, mm0
			movq mm3, mm1
			psraw mm2, 15				;// copy sign of mm0 into mm2
			psraw mm3, 15				;// copy sign of mm3 into mm3
			pxor mm0, mm2
			pxor mm1, mm3
			psubw mm0, mm2				;// mm0 = abs(mm0)
			psubw mm1, mm3				;// mm1 = abs(mm1)
			psllw mm0, 5				;// mm0 *= 32
			psllw mm1, 5				;// mm1 *= 32
			movq mm4, mm0
			movq mm6, [edx]				;// mm6 = 4 words into edx[0..7]
			movq mm5, mm1
			pmullw mm0, mm6				;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6				;// mm4 = MSW(mm0 * mm6)
			psrlw mm0, 14				;// shift LSW
			psllw mm4, 2				;// shift MSW
			movq mm6, [edx+8]			;// mm6 = 4 words into edx[8..11]
			por mm0, mm4				;// combine mm0 and mm4 to get result
			pmullw mm1, mm6				;// mm1 = LSW(mm1 * mm6)
			pmulhw mm5, mm6				;// mm5 = MSW(mm5 * mm6)
			psrlw mm1, 14				;// shift LSW
			psllw mm5, 2				;// shift MSW
			por mm1, mm5				;// combine mm1 and mm5 to get result
			paddw mm0, mquantm3p2div4	;// mm0 += (2 * mquant + 2) >> 2
			paddw mm1, mquantm3p2div4	;// mm1 += (2 * mquant + 2) >> 2
			movq mm4, mm0
			movq mm6, recip_mquant		;// mm6 = recip_mquant
			movq mm5, mm1
			pmullw mm0, mm6				;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6				;// mm4 = MSW(mm0 * mm6)
			psrlw mm0, 15				;// shift LSW
			psllw mm4, 1				;// shift MSW
			por mm0, mm4				;// combine mm0 and mm4 to get result
			pmullw mm1, mm6				;// mm1 = LSW(mm1 * mm6)
			pmulhw mm5, mm6				;// mm5 = MSW(mm5 * mm6)
			psrlw mm1, 15				;// shift LSW
			psllw mm5, 1				;// shift MSW
			movq mm6, PACKED_0xFF00		;// mm6 = (0xff00, 0xff00, 0xff00, 0xff00)
			por mm1, mm5				;// combine mm1 and mm5 to get result
			paddusw mm0, mm6
			paddusw mm1, mm6
			psubusw mm0, mm6			;// clip each word in mm0 to [0..255]
			psubusw mm1, mm6			;// clip each word in mm1 to [0..255]
			paddw mm0, mm2
			paddw mm1, mm3
			pxor mm0, mm2				;// restore original sign of mm0
			pxor mm1, mm3				;// restore original sign of mm1
			movq [edi], mm0				;// store mm0 into edi[0..7]
			movq [edi+8], mm1			;// store mm1 into edi[8..15]
			add esi, 16					;// esi += 16
			add edx, 16					;// edx += 16
			add edi, 16					;// edi += 16
			dec ecx						;// decrement ecx
			jnz quant_intra__l1			;// loop while not zero
			emms						;// empty MMX state
		}

		d = 8>>dc_prec; 
		dst[0] = (x>=0) ? (x+(d>>1))/d : -((-x+(d>>1))/d); // 将 (x/d) 四舍五入	
	}
	else
	{
		x = src[0];
		d = 8>>dc_prec; /* intra_dc_mult */
		dst[0] = (x>=0) ? (x+(d>>1))/d : -((-x+(d>>1))/d); /* 将 (x/d) 四舍五入 */

		for (i=1; i<64; i++)
		{
			x = src[i];
			d = quant_mat[i];
			y = (32*(x>=0 ? x : -x) + (d>>1))/d; /* 将(32*x/quant_mat) 四舍五入 */
			
			d = (3*mquant+2)>>2;
			y = (y+d)/(2*mquant); 

			
			if (y > 255)
			{
				y = 255;
			}

			dst[i] = (x>=0) ? y : -y;

			#if 0
			if (x<0)
			x = -x;
			d = mquant*quant_mat[i];
			y = (16*x + ((3*d)>>3)) / d;
			dst[i] = (src[i]<0) ? -y : y;
			#endif
		}
	}

	return 1;
}

int quant_non_intra(src,dst,quant_mat, recip_quant_mat,mquant)
short *src, *dst;
unsigned char *quant_mat;
unsigned short *recip_quant_mat;
int mquant;
{
	int i;
	int x, y, d;
	int nzflag;
	short recip_mquant[4];

	nzflag = 0;

	if(cpu_MMX && fastQuantization)
	{
		recip_mquant[0] = (unsigned short)((16384.0 + (double) mquant/2.0)/mquant);
		recip_mquant[1] = recip_mquant[0];
		recip_mquant[2] = recip_mquant[0];
		recip_mquant[3] = recip_mquant[0];

		_asm
		{
			mov esi, src				;// esi = src
			mov edx, recip_quant_mat	;// edx = recip_quant_mat
			mov edi, dst				;// edi = dst
			mov ecx, 8					;// ecx = 8
			pxor mm7, mm7				;// mm7 = nzflag
quant_non_intra__l1:
			movq mm0, [esi]				;// mm0 = 4 words into src[0..7]
			movq mm1, [esi+8]			;// mm1 = 4 words into src[8..15]
			movq mm2, mm0
			movq mm3, mm1
			psraw mm2, 15				;// copy sign of mm0 into mm2
			psraw mm3, 15				;// copy sign of mm3 into mm3
			pxor mm0, mm2
			pxor mm1, mm3
			psubw mm0, mm2				;// mm0 = abs(mm0)
			psubw mm1, mm3				;// mm1 = abs(mm1)
			psllw mm0, 5				;// mm0 *= 32
			psllw mm1, 5				;// mm1 *= 32
			movq mm4, mm0
			movq mm6, [edx]				;// mm6 = 4 words into edx[0..7]
			movq mm5, mm1
			pmullw mm0, mm6				;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6				;// mm4 = MSW(mm0 * mm6)
			psrlw mm0, 14				;// shift LSW
			psllw mm4, 2				;// shift MSW
			movq mm6, [edx+8]			;// mm6 = 4 words into edx[8..15]
			por mm0, mm4				;// combine mm0 and mm4 to get result
			pmullw mm1, mm6				;// mm1 = LSW(mm1 * mm6)
			pmulhw mm5, mm6				;// mm5 = MSW(mm1 * mm6)
			psrlw mm1, 14				;// shift LSW
			psllw mm5, 2				;// shift MSW
			por mm1, mm5				;// combine mm1 and mm5 to get result
			movq mm4, mm0
			movq mm6, recip_mquant		;// mm6 = recip_mquant
			movq mm5, mm1
			pmullw mm0, mm6				;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6				;// mm4 = MSW(mm0 * mm6)
			psrlw mm0, 15				;// shift LSW
			psllw mm4, 1				;// shift MSW
			por mm0, mm4				;// combine mm0 and mm4 to get result
			pmullw mm1, mm6				;// mm1 = LSW(mm1 * mm6)
			pmulhw mm5, mm6				;// mm5 = MSW(mm1 * mm6)
			psrlw mm1, 15				;// shift LSW
			psllw mm5, 1				;// shift MSW
			movq mm6, PACKED_0xFF00		;// mm6 = (0xff00, 0xff00, 0xff00, 0xff00)
			por mm1, mm5				;// combine mm1 and mm5 to get result
			paddusw mm0, mm6
			paddusw mm1, mm6
			psubusw mm0, mm6			;// clip each word in mm0 to [0..255]
			psubusw mm1, mm6			;// clip each word in mm1 to [0..255]
			paddw mm0, mm2
			paddw mm1, mm3
			pxor mm0, mm2				;// restore original sign of mm0
			pxor mm1, mm3				;// restore original sign of mm1
			por mm7, mm0				;// update nzflag
			movq [edi], mm0				;// store mm0 into dst[0..7]
			por mm7, mm1				;// update nzflag
			movq [edi+8], mm1			;// store mm1 into dst[8..15]
			add esi, 16					;// esi += 16
			add edx, 16					;// edx += 16
			add edi, 16					;// edi += 16
			dec ecx						;// decrement ecx
			jnz quant_non_intra__l1		;// loop while not zero
			movd eax, mm7				;// load the lower dword of mm7 into eax
			psrlq mm7, 32				;// shift mm7 to get upper dword
			movd ebx, mm7				;// load the lower dword of mm7 into ebx
			or eax, ebx					;// update nzflag
			setnz byte ptr nzflag		;// generate nzflag
			emms						;// empty MMX state
		}

	}
	else
	{
		for (i=0; i<64; i++)
		{
			x = src[i];
			d = quant_mat[i];
			y = (32*(x>=0 ? x : -x) + (d>>1))/d; 
			y /= (2*mquant);

			
			if (y > 255)
			{
				y = 255;
			}

			if ((dst[i] = (x>=0 ? y : -y)) != 0)
				nzflag=1;
		}
	}

	return nzflag;
}

/* MPEG-1 逆量化，针对I帧*/
extern void iquant_intra(src,dst,dc_prec,quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned char *quant_mat;
int mquant;
{
	int x, i, val;
	unsigned short PACKED_MQUANT[4];

	if(cpu_MMX && fastQuantization)
	{
		PACKED_MQUANT[0] = mquant;
		PACKED_MQUANT[1] = mquant;
		PACKED_MQUANT[2] = mquant;
		PACKED_MQUANT[3] = mquant;

		x = src[0];
		_asm
		{
			mov esi, src			;// esi = src
			mov edx, quant_mat		;// edx = quant_mat
			mov edi, dst			;// edi = dst
			mov ecx, 8				;// ecx = 8
			pxor mm7, mm7			;// mm7 = 0
iquant_intra__l1:
			movq mm0, [esi]			;// mm0 = 4 words into esi[0..7]
			movq mm2, [esi+8]		;// mm2 = 4 words into esi[8..15]
			movq mm4, [edx]			;// mm4 = 4 words into edx[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			psraw mm1, 15			;// copy sign of mm0 into mm1
			psraw mm3, 15			;// copy sign of mm2 into mm3
			pxor mm0, mm1
			pxor mm2, mm3
			psubw mm0, mm1			;// mm0 = abs(mm0)
			psubw mm2, mm3			;// mm2 = abs(mm2)
			punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, mm7		;// unpack the lower 4 bytes into mm5
			pmullw mm0, mm4			;// mm0 *= mm4
			pmullw mm2, mm5			;// mm2 *= mm5
			movq mm4, mm0
			movq mm6, PACKED_MQUANT	;// mm6 = (mquant, mquant, mquant, mquant)
			movq mm5, mm2
			pmullw mm0, mm6			;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6			;// mm4 = MSW(mm4 * mm6)
			pmullw mm2, mm6			;// mm2 = LSW(mm2 * mm6)
			pmulhw mm5, mm6			;// mm5 = MSW(mm5 * mm6)
			psrlw mm0, 4			;// shift LSW
			psllw mm4, 12			;// shift MSW
			psrlw mm2, 4			;// shift LSW
			psllw mm5, 12			;// shift MSW
			por mm0, mm4			;// combine mm0 and mm4 to get result
			por mm2, mm5			;// combine mm2 and mm5 to get result
			paddw mm0, mm1
			paddw mm2, mm3
			pxor mm0, mm1			;// restore original sign of mm0
			pxor mm2, mm3			;// restore original sign of mm2
			paddw mm0, PACKED_32768
			paddw mm2, PACKED_32768
			paddusw mm0, PACKED_30720
			paddusw mm2, PACKED_30720
			psubusw mm0, PACKED_61440
			psubusw mm2, PACKED_61440
			psubw mm0, PACKED_2048	;// clip each words into mm0 to [-2048..2047]
			psubw mm2, PACKED_2048	;// clip each words into mm2 to [-2048..2047]
			movq [edi], mm0			;// store mm0 into edi[0..7]
			movq [edi+8], mm2		;// store mm2 into edi[8..15]
			add esi, 16				;// esi += 16
			add edx, 8				;// edx += 8
			add edi, 16				;// edi += 16
			dec ecx					;// decrement ecx
			jnz iquant_intra__l1	;// loop while not zero
			emms					;// empty MMX state
		}
		dst[0] = x << (3-dc_prec);
	}
	else
	{
		dst[0] = src[0] << (3-dc_prec);
		for (i=1; i<64; i++)
		{
			val = (int)(src[i]*quant_mat[i]*mquant)/16;

			/* 如果不匹配则进行如下操作 */
			if ((val&1)==0 && val!=0)
				val+= (val>0) ? -1 : 1;

			/* 色饱和 */
			dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
		}
	}
}

/*逆量化针对非I帧图像*/
extern void iquant_non_intra(src,dst,quant_mat,mquant)
short *src, *dst;
unsigned char *quant_mat;
int mquant;
{
	int i, val;
	unsigned short PACKED_MQUANT[4];

	if(cpu_MMX && fastQuantization)
	{
		PACKED_MQUANT[0] = mquant;
		PACKED_MQUANT[1] = mquant;
		PACKED_MQUANT[2] = mquant;
		PACKED_MQUANT[3] = mquant;

		_asm
		{
			mov esi, src			;// esi = src
			mov edx, quant_mat		;// edx = quant_mat
			mov edi, dst			;// edi = dst
			mov ecx, 8				;// ecx = 8
			pxor mm7, mm7			;// mm7 = 0
iquant_non_intra__l1:
			movq mm0, [esi]			;// mm0 = 4 words into esi[0..7]
			movq mm2, [esi+8]		;// mm2 = 4 words into esi[8..15]
			movq mm4, [edx]			;// mm4 = 4 words into edx[0..7]
			movq mm1, mm0
			movq mm3, mm2
			movq mm5, mm4
			psraw mm1, 15			;// copy sign of mm0 into mm1
			psraw mm3, 15			;// copy sign of mm2 into mm3
			pxor mm0, mm1
			pxor mm2, mm3
			psubw mm0, mm1			;// mm0 = abs(mm0)
			psubw mm2, mm3			;// mm2 = abs(mm2)
			psllw mm0, 1			;// mm0 *= 2
			psllw mm2, 1			;// mm2 *= 2
			punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
			punpckhbw mm5, mm7		;// unpack the lower 4 bytes into mm5
			pmullw mm0, mm4			;// mm0 *= mm4
			pmullw mm2, mm5			;// mm2 *= mm5
			movq mm4, mm0
			movq mm6, PACKED_MQUANT	;// mm6 = (mquant, mquant, mquant, mquant)
			movq mm5, mm2
			pmullw mm0, mm6			;// mm0 = LSW(mm0 * mm6)
			pmulhw mm4, mm6			;// mm4 = MSW(mm4 * mm6)
			pmullw mm2, mm6			;// mm2 = LSW(mm2 * mm6)
			pmulhw mm5, mm6			;// mm5 = MSW(mm5 * mm6)
			psrlw mm0, 5			;// shift LSW
			psllw mm4, 11			;// shift MSW
			psrlw mm2, 5			;// shift LSW
			psllw mm5, 11			;// shift MSW
			por mm0, mm4			;// combine mm0 and mm4 to get result
			por mm2, mm5			;// combine mm2 and mm5 to get result
			paddw mm0, mm1
			paddw mm2, mm3
			pxor mm0, mm1			;// restore original sign of mm0
			pxor mm2, mm3			;// restore original sign of mm2
			paddw mm0, PACKED_32768
			paddw mm2, PACKED_32768
			paddusw mm0, PACKED_30720
			paddusw mm2, PACKED_30720
			psubusw mm0, PACKED_61440
			psubusw mm2, PACKED_61440
			psubw mm0, PACKED_2048	;// clip each words into mm0 to [-2048..2047]
			psubw mm2, PACKED_2048	;// clip each words into mm2 to [-2048..2047]
			movq [edi], mm0			;// store mm0 into edi[0..7]
			movq [edi+8], mm2		;// store mm2 into edi[8..15]
			add esi, 16				;// esi += 16
			add edx, 8				;// edx += 8
			add edi, 16				;// edi += 16
			dec ecx					;// decrement ecx
			jnz iquant_non_intra__l1;// loop while not zero
			emms					;// empty MMX state
		}
	}
	else
	{
		for (i=0; i<64; i++)
		{
			val = src[i];
			if (val!=0)
			{
				val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i]*mquant)/32;

				/* 如果不匹配则进行如下操作 */
				if ((val&1)==0 && val!=0)
				val+= (val>0) ? -1 : 1;
			}

			/* 色饱和 */
			dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
		}
	}
}
