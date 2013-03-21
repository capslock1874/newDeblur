/* readpic.c, 从avi文件中读取源图像数据*/


#include <windows.h>
#include <vfw.h>

#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "video.h"

static void conv444to422(unsigned char[], unsigned char[]);
static void conv422to420(unsigned char[], unsigned char[]);

unsigned int u, v, bpscanl, dib_offset, j, hblank, vblank, hcrop, vcrop, topblank;
int x, y;
float R, G, B;

// RGB->YUV 转换常数

const float RY = (77.0/256.0), GY = (150.0/256.0), BY = (29.0/256.0);
const float RU = (-44.0/256.0), GU = (-87.0/256.0), BU = (131.0/256.0);
const float RV = (131.0/256.0), GV = (-110.0/256.0), BV = (-21.0/256.0);

static short PACKED_Y[4] = { 29, 150, 77, 0 };
static short PACKED_U[4] = { 131, -87, -44, 0 };
static short PACKED_V[4] = { -21, -110, 131, 0 };
static short PACKED_RY[4] = { 77, 77, 77, 77 };
static short PACKED_GY[4] = { 150, 150, 150, 150 };
static short PACKED_BY[4] = { 29, 29, 29, 29 };
static short PACKED_RU[4] = { -44, -44, -44, -44 };
static short PACKED_GU[4] = { -87, -87, -87, -87 };
static short PACKED_BU[4] = { 131, 131, 131, 131 };
static short PACKED_RV[4] = { 131, 131, 131, 131 };
static short PACKED_GV[4] = { -110, -110, -110, -110 };
static short PACKED_BV[4] = { -21, -21, -21, -21 };
static short PACKED_128[4] = { 128, 128, 128, 128 };
static short PACKED_0[4] = { 0, 0, 0, 0 };
static unsigned char PACKED_128B[8] = { 128, 128, 128, 128, 128, 128, 128, 128 };
static long PACKED_128L[2] = { 128, 128 };
static short PACKED_0x07[4] = { 0x07, 0x07, 0x07, 0x07 };
static short PACKED_0xf8[4] = { 0xf8, 0xf8, 0xf8, 0xf8 };

// conv444to422 and conv522to420 constants
static short PACKED_228[4] = { 228, 228, 228, 228 };
static short PACKED_70[4] = { 70, 70, 70, 70 };
static short PACKED_MINUS37[4] = { -37, -37, -37, -37 };
static short PACKED_MINUS21[4] = { -21, -21, -21, -21 };
static short PACKED_11[4] = { 11, 11, 11, 11 };
static short PACKED_5[4] = { 5, 5, 5, 5 };
static long PACKED_256[2] = { 256, 256 };
static short PACKED_255[4] = { 255, 255, 255, 255 };


const int Tolerance  = 10;
const int Filtersize = 6;

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*
下面的程序是一个软滤波器，它用像素周围与之最接近的值进行替代
 
*/

void softfilter(unsigned char* dest, unsigned char* src, int width, int height)
{
  int refval, aktval, upperval, lowerval, numvalues, sum, rowindex;
  int x, y, fx, fy, fy1, fy2, fx1, fx2;

  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {
      refval    = src[x+y*width];
      upperval  = refval + Tolerance;
      lowerval  = refval - Tolerance;

      numvalues = 1;
      sum       = refval;
      
      fy1 = MAX(y-Filtersize,   0);
      fy2 = MIN(y+Filtersize+1, height);

      for (fy = fy1; fy<fy2; fy++)
      {
        rowindex = fy*width;
        fx1      = MAX(x-Filtersize,   0)     + rowindex;
        fx2      = MIN(x+Filtersize+1, width) + rowindex;

        for (fx = fx1; fx<fx2; fx++)
        {
          aktval = src[fx];
          if ((lowerval-aktval)*(upperval-aktval)<0)
          {
            numvalues ++;
            sum += aktval;
          }
        } //for fx
      } //for fy
      
      dest[x+y*width] = sum/numvalues;
    }
  }
}


void readframe(lCurFrame,frame)
unsigned char *frame[];
unsigned long	lCurFrame;
{
	int h, w;
	int lblank, rblank;
	unsigned char	*lpframeY, *lpframeU, *lpframeV;
	unsigned int pixsz;
	unsigned char *lpColor;
	unsigned int palletized;
    int file;

    for(file=0;file<numAviFiles;file++)
        if ((lCurFrame/frame_repl) < nextFileFrames[file])
            break;

        lpbi = AVIStreamGetFrame(pget[file], lCurFrame/frame_repl-((file==0)?0:nextFileFrames[file-1]));

    if(!lpbi && !fake_bad_frames)
	{
		fprintf(stderr, "\ncould not retrieve video frame %i!\n", lCurFrame);
		fprintf(stderr, "\npossibly corrupt avi file, try -e option.");
		exit(25);
	}
	else if(!lpbi && fake_bad_frames)
	{
        for(file=0;file<numAviFiles;file++)
            if ((last_good_video_frame) < nextFileFrames[file])
                break;
		lpbi = AVIStreamGetFrame(pget[file], last_good_video_frame);
		bad_frame_count++;
	}
	else if(lpbi)
	{
		last_good_video_frame = lCurFrame/frame_repl;
	}

	pixsz = lpbi->biBitCount;
	if((pixsz!=8)&&(pixsz!=16)&&(pixsz!=24)&&(pixsz!=32))
	{
		fprintf(stderr, "\ncan only handle 8 bit palettized, or 16, 24, or 32 bit RGB video!");
		exit(26);
	}

	if(pixsz==8)
		palletized=1;
	else
		palletized=0;


	if (lpbi->biCompression != BI_RGB) // does not support BI_BITFIELDS
	{
		fprintf(stderr, "\ncan not handle compressed DIBs from codec!");
		exit(27);
	}

	lpColor = (unsigned char *)lpbi + lpbi->biSize; 

	if(palletized)
		dib_offset = sizeof(BITMAPINFOHEADER) + lpbi->biClrUsed * 4;
	else
		dib_offset = sizeof(BITMAPINFOHEADER); // offset past bitmap header
	lpbitmp = (unsigned char *)lpbi + dib_offset; // pointer to actual bitmap

	switch(pixsz)
	{
	case 8:
		bpscanl = (unsigned int)(((lpbi->biWidth + 3)>>2)<<2);
		break;
	case 16:
		bpscanl = (unsigned int)(lpbi->biWidth * 2);
		break;
	case 24:
		bpscanl = ((((unsigned int)lpbi->biWidth * 3 + 3)>>2)<<2);
		break;
	case 32:
		bpscanl = (unsigned int)(lpbi->biWidth * 4);
		break;
	default:
		fprintf(stderr, "\ninternal error: illegal pixsz value\n");
		exit(28); 
	}
	if(width>lpbi->biWidth)
		hblank = width - lpbi->biWidth;
	else
		hblank=0;

	if(height>lpbi->biHeight)
		vblank = height - lpbi->biHeight;
	else
		vblank=0;

	if(width<lpbi->biWidth)
		hcrop = lpbi->biWidth - width;
	else
		hcrop=0;

	if(height<lpbi->biHeight)
		vcrop = lpbi->biHeight - height;
	else
		vcrop=0;

	if(vertical_size>lpbi->biHeight)
		topblank = (vertical_size - lpbi->biHeight)/2;
	else
		topblank = 0;

	// 将RGB DIB 转化成 YUV (4:2:0)
	lpframeY = (unsigned char *)frame[0];
	lpframeU = (unsigned char *)frame[1];
	lpframeV = (unsigned char *)frame[2];

	j = topblank;
	while(j > 0)
	{
		for(x = 0; x < width; x++)
		{
			*lpframeY++ = 0;
			Ubuffer[x + (j - 1)*width] = 128;
			Vbuffer[x + (j - 1)*width] = 128;
		}
		j--;
	}

	switch(pixsz)
	{
		case 8:
			lpcurscanl = (unsigned char *)(lpbitmp + ((lpbi->biHeight - 1 - (vcrop/2)) * bpscanl) + ((hcrop/2)));
			lpframeU = Ubuffer+(hblank/2)+topblank*width;
			lpframeV = Vbuffer+(hblank/2)+topblank*width;

			h = lpbi->biHeight - (int) vcrop;
			lblank = hblank/2;
			rblank = hblank - lblank;

			do
			{
			
				if(lblank > 0)
				{
					memset(lpframeY, 0, lblank);
					memset(lpframeU, 128, lblank);
					memset(lpframeV, 128, lblank);
					lpframeY += lblank;
					lpframeU += lblank;
					lpframeV += lblank;
				}

				w = lpbi->biWidth - (int) hcrop;
				lpcurpix = lpcurscanl;

				do
				{
					R = (float) lpColor[*lpcurpix * 4 + 2];
					G = (float) lpColor[*lpcurpix * 4 + 1];
					B = (float) lpColor[*lpcurpix * 4 + 0];
					lpcurpix++;

					*lpframeY++ = (int)(RY*R + GY*G + BY*B);
					*lpframeU++ = (int)(RU*R + GU*G + BU*B + 128);
					*lpframeV++ = (int)(RV*R + GV*G + BV*B + 128);
				} while(--w);

				if(rblank > 0)
				{
					memset(lpframeY, 0, rblank);
					memset(lpframeU, 128, rblank);
					memset(lpframeV, 128, rblank);
					lpframeY += rblank;
					lpframeU += rblank;
					lpframeV += rblank;
				}

				lpcurscanl -= bpscanl;
			} while(--h);
			break;

		case 16:
			lpcurscanl = (unsigned char *)(lpbitmp + ((lpbi->biHeight - 1 - (vcrop/2)) * bpscanl) + ((hcrop/2) * 2));
			lpframeU = Ubuffer+(hblank/2)+topblank*width;
			lpframeV = Vbuffer+(hblank/2)+topblank*width;

			h = lpbi->biHeight - (int) vcrop;
			lblank = hblank/2;
			rblank = hblank - lblank;

			do
			{
				if(lblank > 0)
				{
					memset(lpframeY, 0, lblank);
					memset(lpframeU, 128, lblank);
					memset(lpframeV, 128, lblank);
					lpframeY += lblank;
					lpframeU += lblank;
					lpframeV += lblank;
				}

				if(cpu_MMX)
				{
					w = (lpbi->biWidth-(int)hcrop) >> 2;

					_asm
					{
						mov esi, lpcurscanl		;// esi = lpcurscanl
						mov ecx, lpframeY		;// ecx = lpframeY
						mov edx, lpframeU		;// edx = lpframeU
						mov edi, lpframeV		;// edi = lpframeV
						movq mm6, PACKED_128	;// mm6 = (128, 128, 128, 128)
readframe__l1:
						movq mm0, [esi]			;// mm0 = 4 words into esi[0..7]
						movq mm3, mm0
						movq mm6, mm0
						psrlw mm0, 7
						psrlw mm3, 2
						psllw mm6, 3
						pand mm0, PACKED_0xf8
						pand mm3, PACKED_0xf8
						pand mm6, PACKED_0xf8
						por mm0, PACKED_0x07	;// red is in mm0
						por mm3, PACKED_0x07	;// green is in mm3
						por mm6, PACKED_0x07	;// blue is in mm6
						movq mm1, mm0
						movq mm2, mm0
						movq mm4, mm3
						movq mm5, mm3
						pmullw mm0, PACKED_RY	;// mm0 *= (RY, RY, RY, RY)
						pmullw mm1, PACKED_RU	;// mm1 *= (RU, RU, RU, RU)
						pmullw mm2, PACKED_RV	;// mm2 *= (RV, RV, RV, RV)
						pmullw mm3, PACKED_GY	;// mm3 *= (GY, GY, GY, GY)
						pmullw mm4, PACKED_GU	;// mm4 *= (GU, GU, GU, GU)
						pmullw mm5, PACKED_GV	;// mm5 *= (GV, GV, GV, GV)
						paddw mm0, mm3			;// mm0 += mm3
						paddw mm1, mm4			;// mm1 += mm4
						paddw mm2, mm5			;// mm2 += mm5
						movq mm7, mm6
						movq mm3, mm6
						pmullw mm6, PACKED_BY	;// mm6 *= (RY, RY, RY, RY)
						pmullw mm7, PACKED_BU	;// mm7 *= (RY, RY, RY, RY)
						pmullw mm3, PACKED_BV	;// mm3 *= (RY, RY, RY, RY)
						paddw mm0, mm6			;// mm0 += mm6
						paddw mm1, mm7			;// mm1 += mm7
						paddw mm2, mm3			;// mm2 += mm3
						paddusw mm0, PACKED_128	;// mm0 += (128, 128, 128, 128)
						paddusw mm1, PACKED_128	;// mm1 += (128, 128, 128, 128)
						paddusw mm2, PACKED_128	;// mm2 += (128, 128, 128, 128)
						psrlw mm0, 8			;// shift mm0 and round to lower
						psrlw mm1, 8			;// shift mm1 and round to lower
						psrlw mm2, 8			;// shift mm2 and round to lower
						packuswb mm0, mm0		;// pack 4 words into the lower 4 bytes of mm0
						packuswb mm1, mm1		;// pack 4 words into the lower 4 bytes of mm1
						packuswb mm2, mm2		;// pack 4 words into the lower 4 bytes of mm2
						paddb mm1, PACKED_128B	;// mm1 += (128, 128, 128, 128, 128, 128, 128, 128)
						paddb mm2, PACKED_128B	;// mm2 += (128, 128, 128, 128, 128, 128, 128, 128)
						movd [ecx], mm0			;// store mm0 into ecx[0..7]
						movd [edx], mm1			;// store mm1 into edx[0..7]
						movd [edi], mm2			;// store mm2 into edi[0..7]

						add esi, 8				;// esi += 8
						add ecx, 4				;// edx += 4
						add edx, 4				;// edx += 4
						add edi, 4				;// edi += 4;
						dec dword ptr w			;// decrement w
						jnz readframe__l1		;// loop while not zero
						mov lpframeY, ecx		;// update lpframeY
						mov lpframeU, edx		;// update lpframeU
						mov lpframeV, edi		;// update lpframeV
						emms					;// empty MMX state
					}
				}
				else
				{
					w = lpbi->biWidth - (int) hcrop;
					lpcurpixw = (unsigned short *) lpcurscanl;

					do
					{
						R = (float)(((*lpcurpixw >> 7) & 0xf8)|0x07);
						G = (float)(((*lpcurpixw >> 2) & 0xf8)|0x07);
						B = (float)(((*lpcurpixw << 3) & 0xf8)|0x07);
						lpcurpixw++;

						*lpframeY++ = (int)(RY*R + GY*G + BY*B);
						*lpframeU++ = (int)(RU*R + GU*G + BU*B + 128);
						*lpframeV++ = (int)(RV*R + GV*G + BV*B + 128);
					} while(--w);
				}

				// take care of any blank pixels on the right
				if(rblank > 0)
				{
					memset(lpframeY, 0, rblank);
					memset(lpframeU, 128, rblank);
					memset(lpframeV, 128, rblank);
					lpframeY += rblank;
					lpframeU += rblank;
					lpframeV += rblank;
				}

				lpcurscanl -= bpscanl;
			} while(--h);
			break;

		case 24:
			lpcurscanl = (unsigned char *)(lpbitmp + ((lpbi->biHeight - 1 - (vcrop/2)) * bpscanl) + ((hcrop/2) * 3));
			lpframeU = Ubuffer+(hblank/2)+topblank*width;
			lpframeV = Vbuffer+(hblank/2)+topblank*width;

			h = lpbi->biHeight - (int) vcrop;
			lblank = hblank/2;
			rblank = hblank - lblank;

			do
			{
				// take care of any blank pixels on the left
				if(lblank > 0)
				{
					memset(lpframeY, 0, lblank);
					memset(lpframeU, 128, lblank);
					memset(lpframeV, 128, lblank);
					lpframeY += lblank;
					lpframeU += lblank;
					lpframeV += lblank;
				}

				if(cpu_MMX)
				{

					w = (lpbi->biWidth-(int)hcrop) >> 1;

					// assume that width is a multiple of 2 !
					_asm
					{
						mov esi, lpcurscanl			;// esi = lpcurscanl
						mov ecx, lpframeY			;// ecx = lpframeY
						mov edx, lpframeU			;// edx = lpframeU
						mov edi, lpframeV			;// edi = lpframeV
						movq mm6, PACKED_128L		;// mm6 = (128, 128)
						pxor mm7, mm7				;// mm7 = 0
readframe__l2:
						movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
						movd mm3, [esi+3]			;// lower 4 bytes into mm3 = esi[4..7]
						punpcklbw mm0, mm7			;// unpack the lower 4 bytes into mm0
						punpcklbw mm3, mm7			;// unpack the lower 4 bytes into mm3
						movq mm1, mm0
						movq mm2, mm0
						movq mm4, mm3
						movq mm5, mm3
						pmaddwd mm0, PACKED_Y		;// multiply each word in mm0 by PACKED_Y and add them to make only two dwords
						pmaddwd mm1, PACKED_U		;// multiply each word in mm1 by PACKED_U and add them to make only two dwords
						pmaddwd mm2, PACKED_V		;// multiply each word in mm2 by PACKED_V and add them to make only two dwords
						pmaddwd mm3, PACKED_Y		;// multiply each word in mm3 by PACKED_Y and add them to make only two dwords
						pmaddwd mm4, PACKED_U		;// multiply each word in mm4 by PACKED_U and add them to make only two dwords
						pmaddwd mm5, PACKED_V		;// multiply each word in mm5 by PACKED_V and add them to make only two dwords
						paddd mm0, mm6				;// mm0 += (128, 128)
						paddd mm1, mm6				;// mm1 += (128, 128)
						paddd mm2, mm6				;// mm2 += (128, 128)
						paddd mm3, mm6				;// mm3 += (128, 128)
						paddd mm4, mm6				;// mm4 += (128, 128)
						paddd mm5, mm6				;// mm5 += (128, 128)
						psrad mm0, 8				;// shift mm0 and round to lower
						psrad mm1, 8				;// shift mm1 and round to lower
						psrad mm2, 8				;// shift mm2 and round to lower
						psrad mm3, 8				;// shift mm3 and round to lower
						psrad mm4, 8				;// shift mm4 and round to lower
						psrad mm5, 8				;// shift mm5 and round to lower
						movd eax, mm0				;// load lower dword of mm0 into eax
						psrlq mm0, 32				;// shift mm0 to get upper dword
						movd ebx, mm0				;// load lower dword of mm0 into ebx
						add eax, ebx				;// eax += ebx
						mov [ecx], al				;// store result into [ecx]
						movd eax, mm1				;// load lower dword of mm1 into eax
						psrlq mm1, 32				;// shift mm1 to get upper dword
						movd ebx, mm1				;// load lower dword of mm1 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edx], al				;// store result into [edx]
						movd eax, mm2				;// load lower dword of mm2 into eax
						psrlq mm2, 32				;// shift mm2 to get upper dword
						movd ebx, mm2				;// load lower dword of mm2 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edi], al				;// store result into [edi]
						movd eax, mm3				;// load lower dword of mm3 into eax
						psrlq mm3, 32				;// shift mm2 to get upper dword
						movd ebx, mm3				;// load lower dword of mm3 into ebx
						add eax, ebx				;// eax += ebx
						mov [ecx+1], al				;// store result into [ecx+1]
						movd eax, mm4				;// load lower dword of mm4 into eax
						psrlq mm4, 32				;// shift mm4 to get upper dword
						movd ebx, mm4				;// load lower dword of mm4 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edx+1], al				;// store result into [edx+1]
						movd eax, mm5				;// load lower dword of mm5 into eax
						psrlq mm5, 32				;// shift mm5 to get upper dword
						movd ebx, mm5				;// load lower dword of mm5 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edi+1], al				;// store result into [edi+1]
						add esi, 6					;// esi += 6
						add ecx, 2					;// ecx += 2
						add edx, 2					;// edx += 2
						add edi, 2					;// edi += 2
						dec dword ptr w				;// decrement w
						jnz readframe__l2			;// loop while not zero
						mov lpframeY, ecx			;//	update lpframeY
						mov lpframeU, edx			;// update lpframeU
						mov lpframeV, edi			;// update lpframeV
						emms						;// empty MMX state
					}
				}
				else
				{
					w = lpbi->biWidth - (int) hcrop;
					lpcurpix = lpcurscanl;

					do
					{
						B = *lpcurpix++;
						G = *lpcurpix++;
						R = *lpcurpix++;

						*lpframeY++ = (int)(RY*R + GY*G + BY*B);
						*lpframeU++ = (int)(RU*R + GU*G + BU*B + 128);
						*lpframeV++ = (int)(RV*R + GV*G + BV*B + 128);
					} while(--w);
				}

				// take care of any blank pixels on the right
				if(rblank > 0)
				{
					memset(lpframeY, 0, rblank);
					memset(lpframeU, 128, rblank);
					memset(lpframeV, 128, rblank);
					lpframeY += rblank;
					lpframeU += rblank;
					lpframeV += rblank;
				}

				lpcurscanl -= bpscanl;
			} while(--h);
			break;

		case 32:
			lpcurscanl = (unsigned char *)(lpbitmp + ((lpbi->biHeight - 1 - (vcrop/2)) * bpscanl) + ((hcrop/2) * 4));
			lpframeU = Ubuffer+(hblank/2)+topblank*width;
			lpframeV = Vbuffer+(hblank/2)+topblank*width;

			h = lpbi->biHeight - (int) vcrop;
			lblank = hblank/2;
			rblank = hblank - lblank;

			do
			{
				// take care of any blank pixels on the left
				if(lblank > 0)
				{
					memset(lpframeY, 0, lblank);
					memset(lpframeU, 128, lblank);
					memset(lpframeV, 128, lblank);
					lpframeY += lblank;
					lpframeU += lblank;
					lpframeV += lblank;
				}

				if(cpu_MMX)
				{

					w = (lpbi->biWidth-(int)hcrop) >> 1;

					// assume that width is a multiple of 2 !
					_asm
					{
						mov esi, lpcurscanl			;// esi = lpcurscanl
						mov ecx, lpframeY			;// ecx = lpframeY
						mov edx, lpframeU			;// edx = lpframeU
						mov edi, lpframeV			;// edi = lpframeV
						movq mm6, PACKED_128L		;// mm6 = (128, 128)
						pxor mm7, mm7				;// mm7 = 0
readframe__l3:
						movd mm0, [esi]				;// lower 4 bytes into mm0 = esi[0..3]
						movd mm3, [esi+3]			;// lower 4 bytes into mm3 = esi[4..7]
						punpcklbw mm0, mm7			;// unpack the lower 4 bytes into mm0
						punpcklbw mm3, mm7			;// unpack the lower 4 bytes into mm3
						movq mm1, mm0
						movq mm2, mm0
						movq mm4, mm3
						movq mm5, mm3
						pmaddwd mm0, PACKED_Y		;// multiply each word in mm0 by PACKED_Y and add them to make only two dwords
						pmaddwd mm1, PACKED_U		;// multiply each word in mm1 by PACKED_U and add them to make only two dwords
						pmaddwd mm2, PACKED_V		;// multiply each word in mm2 by PACKED_V and add them to make only two dwords
						pmaddwd mm3, PACKED_Y		;// multiply each word in mm3 by PACKED_Y and add them to make only two dwords
						pmaddwd mm4, PACKED_U		;// multiply each word in mm4 by PACKED_U and add them to make only two dwords
						pmaddwd mm5, PACKED_V		;// multiply each word in mm5 by PACKED_V and add them to make only two dwords
						paddd mm0, mm6				;// mm0 += (128, 128)
						paddd mm1, mm6				;// mm1 += (128, 128)
						paddd mm2, mm6				;// mm2 += (128, 128)
						paddd mm3, mm6				;// mm3 += (128, 128)
						paddd mm4, mm6				;// mm4 += (128, 128)
						paddd mm5, mm6				;// mm5 += (128, 128)
						psrad mm0, 8				;// shift mm0 and round to lower
						psrad mm1, 8				;// shift mm1 and round to lower
						psrad mm2, 8				;// shift mm2 and round to lower
						psrad mm3, 8				;// shift mm3 and round to lower
						psrad mm4, 8				;// shift mm4 and round to lower
						psrad mm5, 8				;// shift mm5 and round to lower
						movd eax, mm0				;// load lower dword of mm0 into eax
						psrlq mm0, 32				;// shift mm0 to get upper dword
						movd ebx, mm0				;// load lower dword of mm0 into ebx
						add eax, ebx				;// eax += ebx
						mov [ecx], al				;// store result into [ecx]
						movd eax, mm1				;// load lower dword of mm1 into eax
						psrlq mm1, 32				;// shift mm1 to get upper dword
						movd ebx, mm1				;// load lower dword of mm1 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edx], al				;// store result into [edx]
						movd eax, mm2				;// load lower dword of mm2 into eax
						psrlq mm2, 32				;// shift mm2 to get upper dword
						movd ebx, mm2				;// load lower dword of mm2 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edi], al				;// store result into [edi]
						movd eax, mm3				;// load lower dword of mm3 into eax
						psrlq mm3, 32				;// shift mm2 to get upper dword
						movd ebx, mm3				;// load lower dword of mm3 into ebx
						add eax, ebx				;// eax += ebx
						mov [ecx+1], al				;// store result into [ecx+1]
						movd eax, mm4				;// load lower dword of mm4 into eax
						psrlq mm4, 32				;// shift mm4 to get upper dword
						movd ebx, mm4				;// load lower dword of mm4 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edx+1], al				;// store result into [edx+1]
						movd eax, mm5				;// load lower dword of mm5 into eax
						psrlq mm5, 32				;// shift mm5 to get upper dword
						movd ebx, mm5				;// load lower dword of mm5 into ebx
						add eax, ebx
						add eax, 128				;// eax += ebx + 128
						mov [edi+1], al				;// store result into [edi+1]
						add esi, 8					;// esi += 8
						add ecx, 2					;// ecx += 2
						add edx, 2					;// edx += 2
						add edi, 2					;// edi += 2
						dec dword ptr w				;// decrement w
						jnz readframe__l3			;// loop while not zero
						mov lpframeY, ecx			;//	update lpframeY
						mov lpframeU, edx			;// update lpframeU
						mov lpframeV, edi			;// update lpframeV
						emms						;// empty MMX state
					}
				}
				else
				{
					w = lpbi->biWidth - (int) hcrop;
					lpcurpix = lpcurscanl;

					do
					{
						B = *lpcurpix++;
						G = *lpcurpix++;
						R = *lpcurpix++;
						lpcurpix++;

						*lpframeY++ = (int)(RY*R + GY*G + BY*B);
						*lpframeU++ = (int)(RU*R + GU*G + BU*B + 128);
						*lpframeV++ = (int)(RV*R + GV*G + BV*B + 128);
					} while(--w);
				}

				if(rblank > 0)
				{
					memset(lpframeY, 0, rblank);
					memset(lpframeU, 128, rblank);
					memset(lpframeV, 128, rblank);
					lpframeY += rblank;
					lpframeU += rblank;
					lpframeV += rblank;
				}

				lpcurscanl -= bpscanl;
			} while(--h);
			break;

		default:
			fprintf(stderr, "\ninternal error: illegal pixsz value\n");
			exit(29); 
	}

	j = vblank - topblank;
	while(j > 0)
	{
		for(x = 0; x < width; x++)
		{
			*lpframeY++ = 0;
			Ubuffer[x + (topblank+lpbi->biHeight + j - 1)*width] = 128;
			Vbuffer[x + (topblank+lpbi->biHeight + j - 1)*width] = 128;
		}
	j--;
	}



	conv444to422(Ubuffer, Ubuffer422);
	conv444to422(Vbuffer, Vbuffer422);

	conv422to420(Ubuffer422, frame[1]);
	conv422to420(Vbuffer422, frame[2]);

    if (use_image_noise_reduction)
    {
        lpframeY = (unsigned char *)frame[0];
        softfilter(TempFilterBuffer, lpframeY, width, height);
        memcpy(lpframeY, TempFilterBuffer, width*height);

        lpframeU = (unsigned char *)frame[1];
        softfilter(TempFilterBuffer, lpframeU, width/2, height/2);
        memcpy(lpframeU, TempFilterBuffer, width*height/4);

        lpframeV = (unsigned char *)frame[2];
        softfilter(TempFilterBuffer, lpframeV, width/2, height/2);
        memcpy(lpframeV, TempFilterBuffer, width*height/4);
    }
}



// 水平滤波和2:1子采样
static void conv444to422(src,dst)
unsigned char *src, *dst;
{
	int i, j, im5, im4, im3, im2, im1, ip1, ip2, ip3, ip4, ip5, ip6;
	int width_div8;
	int h;

	if(cpu_MMX && width > 16 && (width & 15) == 0)
	{
		h = height;
		width_div8 = width >> 3;

		do
		{
			// prologue

			*dst++ = clp[(int)(228*(src[0]+src[1])
							   +70*(src[0]+src[2])
							   -37*(src[0]+src[3])
							   -21*(src[0]+src[4])
							   +11*(src[0]+src[5])
							   + 5*(src[0]+src[6])+256)>>9];
			*dst++ = clp[(int)(228*(src[2]+src[3])
							   +70*(src[1]+src[4])
							   -37*(src[0]+src[5])
							   -21*(src[0]+src[6])
							   +11*(src[0]+src[7])
							   + 5*(src[0]+src[8])+256)>>9];
			*dst++ = clp[(int)(228*(src[4]+src[5])
							   +70*(src[3]+src[6])
							   -37*(src[2]+src[7])
							   -21*(src[1]+src[8])
							   +11*(src[0]+src[9])
							   + 5*(src[0]+src[10])+256)>>9];
			*dst++ = clp[(int)(228*(src[6]+src[7])
							   +70*(src[5]+src[8])
							   -37*(src[4]+src[9])
							   -21*(src[3]+src[10])
							   +11*(src[2]+src[11])
							   + 5*(src[1]+src[12])+256)>>9];

			src += 8;

			_asm
			{
				mov esi, src				;// esi = src
				mov edi, dst				;// edi = dst
				mov ecx, width_div8
				sub ecx, 2					;// ecx = (width >> 3) - 2
conv444to422__l1:
				movq mm7, PACKED_255		;// mm7 = (255, 255, 255, 255)
				movq mm5, [esi-1]			;// mm5 = 4 words into esi[-1..6]
				movq mm4, [esi]				;// mm4 = 4 words into esi[0..7]
				movq mm1, [esi+1]			;// mm1 = 4 words into esi[1..8]
				movq mm3, [esi+2]			;// mm3 = 4 words into esi[2..9]
				pand mm5, mm7				;// keep only LSB of mm5
				pand mm4, mm7				;// keep only LSB of mm4
				pand mm1, mm7				;// keep only LSB of mm1
				pand mm3, mm7				;// keep only LSB of mm3
				paddw mm4, mm1				;// mm4 += mm1
				paddw mm5, mm3				;// mm5 += mm3
				movq mm1, mm4
				movq mm3, mm5
				pmullw mm4, PACKED_228		;// mm4 *= 228
				pmullw mm5, PACKED_70		;// mm5 *= 70
				pmulhw mm1, PACKED_228		;// mm1 *= 228
				pmulhw mm3, PACKED_70		;// mm3 *= 70
				movq mm6, mm4
				movq mm7, mm5
				punpcklwd mm4, mm1			;// unpack the two low words into two dwords
				punpckhwd mm6, mm1			;// unpack the two upper words into two dwords
				punpcklwd mm5, mm3			;// unpack the two low words into two dwords
				punpckhwd mm7, mm3			;// unpack the two upper words into two dwords
				paddd mm4, mm5				;// mm4 += mm5
				paddd mm6, mm7				;// mm6 += mm7

				movq mm7, PACKED_255		;// mm7 = (255, 255, 255, 255)
				movq mm5, [esi-3]			;// mm5 = 4 words into esi[-3..4]
				movq mm0, [esi-2]			;// mm0 = 4 words into esi[-2..5]
				movq mm1, [esi+3]			;// mm1 = 4 words into esi[3..10]
				movq mm3, [esi+4]			;// mm3 = 4 words into esi[4..11]
				pand mm5, mm7				;// keep only LSB of mm5
				pand mm0, mm7				;// keep only LSB of mm0
				pand mm1, mm7				;// keep only LSB of mm1
				pand mm3, mm7				;// keep only LSB of mm3
				paddw mm0, mm1				;// mm0 += mm1
				paddw mm5, mm3				;// mm5 += mm3
				movq mm1, mm0
				movq mm3, mm5
				pmullw mm0, PACKED_MINUS37	;// mm4 *= -37
				pmullw mm5, PACKED_MINUS21	;// mm5 *= -21
				pmulhw mm1, PACKED_MINUS37	;// mm1 *= -37
				pmulhw mm3, PACKED_MINUS21	;// mm3 *= -21
				movq mm2, mm0
				movq mm7, mm5
				punpcklwd mm0, mm1			;// unpack the two low words into two dwords
				punpckhwd mm2, mm1			;// unpack the two upper words into two dwords
				punpcklwd mm5, mm3			;// unpack the two low words into two dwords
				punpckhwd mm7, mm3			;// unpack the two upper words into two dwords
				paddd mm0, mm5
				paddd mm2, mm7
				paddd mm0, mm4				;// mm0 += mm4 + mm5
				paddd mm2, mm6				;// mm2 += mm6 + mm7

				movq mm7, PACKED_255		;// mm7 = (255, 255, 255, 255)
				movq mm5, [esi-5]			;// mm5 = 4 words into esi[-5..2]
				movq mm4, [esi-4]			;// mm4 = 4 words into esi[-4..3]
				movq mm1, [esi+5]			;// mm1 = 4 words into esi[5..12]
				movq mm3, [esi+6]			;// mm3 = 4 words into esi[6..13]
				pand mm5, mm7				;// keep only LSB of mm5
				pand mm4, mm7				;// keep only LSB of mm4
				pand mm1, mm7				;// keep only LSB of mm1
				pand mm3, mm7				;// keep only LSB of mm3
				paddw mm4, mm1				;// mm4 += mm1
				paddw mm5, mm3				;// mm5 += mm3
				movq mm1, mm4
				movq mm3, mm5
				pmullw mm4, PACKED_11		;// mm4 *= 11
				pmullw mm5, PACKED_5		;// mm5 *= 5
				pmulhw mm1, PACKED_11		;// mm1 *= 11
				pmulhw mm3, PACKED_5		;// mm3 *= 5
				movq mm6, mm4
				movq mm7, mm5
				punpcklwd mm4, mm1			;// unpack the two low words into two dwords
				punpckhwd mm6, mm1			;// unpack the two upper words into two dwords
				punpcklwd mm5, mm3			;// unpack the two low words into two dwords
				punpckhwd mm7, mm3			;// unpack the two upper words into two dwords
				paddd mm4, mm5
				paddd mm6, mm7
				paddd mm0, mm4
				paddd mm2, mm6
				paddd mm0, PACKED_256		;// mm0 += mm4 + mm5 + (256, 256, 256, 256)
				paddd mm2, PACKED_256		;// mm2 += mm6 + mm7 + (256, 256, 256, 256)
				psrld mm0, 9				;// mm0 >>= 9
				psrld mm2, 9				;// mm0 >>= 9
				packssdw mm0, mm2			;// pack mm0 and mm2
				packuswb mm0, mm0			;// pack words into bytes
				movd [edi], mm0				;// store the lower 4 bytes of mm0 into edi[0..3]

				add esi, 8					;// esi += 8
				add edi, 4					;// edi += 4

				dec ecx						;// decrement ecx
				jnz conv444to422__l1		;// loop while not zero
				mov src, esi				;// update src
				mov dst, edi				;// update dst
				emms						;// empty MMX state
			}

			// epilogue

			*dst++ = clp[(int)(228*(src[0]+src[1])
							   +70*(src[-1]+src[2])
							   -37*(src[-2]+src[3])
							   -21*(src[-3]+src[4])
							   +11*(src[-4]+src[5])
							   + 5*(src[-5]+src[6])+256)>>9];
			*dst++ = v = clp[(int)(228*(src[2]+src[3])
							   +70*(src[1]+src[4])
							   -37*(src[0]+src[5])
							   -21*(src[-1]+src[6])
							   +11*(src[-2]+src[7])
							   + 5*(src[-3]+src[8])+256)>>9];
			*dst++ = clp[(int)(228*(src[4]+src[5])
							   +70*(src[3]+src[6])
							   -37*(src[2]+src[7])
							   -21*(src[1]+src[7])
							   +11*(src[0]+src[7])
							   + 5*(src[-1]+src[7])+256)>>9];
			*dst++ = clp[(int)(228*(src[6]+src[7])
							   +70*(src[5]+src[7])
							   -37*(src[4]+src[7])
							   -21*(src[3]+src[7])
							   +11*(src[2]+src[7])
							   + 5*(src[1]+src[7])+256)>>9];

			src += 8;

		} while(--h);
	}
	else
	{
		for (j=0; j<height; j++)
		{
			for (i=0; i<width; i+=2)
			{
				im5 = (i<5) ? 0 : i-5;
				im4 = (i<4) ? 0 : i-4;
				im3 = (i<3) ? 0 : i-3;
				im2 = (i<2) ? 0 : i-2;
				im1 = (i<1) ? 0 : i-1;
				ip1 = (i<width-1) ? i+1 : width-1;
				ip2 = (i<width-2) ? i+2 : width-1;
				ip3 = (i<width-3) ? i+3 : width-1;
				ip4 = (i<width-4) ? i+4 : width-1;
				ip5 = (i<width-5) ? i+5 : width-1;
				ip6 = (i<width-5) ? i+6 : width-1;

				dst[i>>1] = clp[(int)(228*(src[i]+src[ip1])
						    	 +70*(src[im1]+src[ip2])
							     -37*(src[im2]+src[ip3])
							     -21*(src[im3]+src[ip4])
							     +11*(src[im4]+src[ip5])
							     + 5*(src[im5]+src[ip6])+256)>>9];
			}
			src+= width;
			dst+= width>>1;
		}
	}
}

static void conv422to420(src,dst)
unsigned char *src, *dst;
{
	int w, i, j, k, jm5, jm4, jm3, jm2, jm1;
	int jp1, jp2, jp3, jp4, jp5, jp6;
	int height_div2 = height >> 1;
	unsigned char *s, *d;

	w = width>>1;

	if(cpu_MMX && width > 8 && (width & 3) == 0 && height > 16 && (height & 1) == 0)
	{
		i = w >> 2;

		do
		{
			// prologue
			s = src;
			d = dst;

			for(k = 0; k < 4; k++)
			{
				d[k] = clp[(int)(228*(s[k]+s[k+w])
							 +70*(s[k]+s[k+w*2])
							 -37*(s[k]+s[k+w*3])
							 -21*(s[k]+s[k+w*4])
							 +11*(s[k]+s[k+w*5])
							 + 5*(s[k]+s[k+w*6])+256)>>9];

				d[k+w] = clp[(int)(228*(s[k+w*2]+s[k+w*3])
							 +70*(s[k+w]+s[k+w*4])
							 -37*(s[k]+s[k+w*5])
							 -21*(s[k]+s[k+w*6])
							 +11*(s[k]+s[k+w*7])
							 + 5*(s[k]+s[k+w*8])+256)>>9];

				d[k+2*w] = clp[(int)(228*(s[k+w*4]+s[k+w*5])
							 +70*(s[k+w*3]+s[k+w*6])
							 -37*(s[k+w*2]+s[k+w*7])
							 -21*(s[k+w]+s[k+w*8])
							 +11*(s[k]+s[k+w*9])
							 + 5*(s[k]+s[k+w*10])+256)>>9];

				d[k+3*w] = clp[(int)(228*(s[k+w*6]+s[k+w*7])
							 +70*(s[k+w*5]+s[k+w*8])
							 -37*(s[k+w*4]+s[k+w*9])
							 -21*(s[k+w*3]+s[k+w*10])
							 +11*(s[k+w*2]+s[k+w*11])
							 + 5*(s[k+w]+s[k+w*12])+256)>>9];
			}

			d += 4*w;
			s += 8*w;

			j = height_div2 - 8;

			_asm
			{
				mov esi, s				;// esi = s
				mov edi, d				;// edi = d
	conv422to420__l1:
				mov eax, w				;// eax = w
				mov ebx, eax			
				neg ebx					;// ebx = -w
				pxor mm7, mm7			;// mm7 = 0
				movd mm2, [esi+ebx]		;// lower 4 bytes into mm2 = (esi + ebx)[0..3]
				movd mm0, [esi]			;// lower 4 bytes into mm0 = esi[0..3]
				movd mm1, [esi+eax]		;// lower 4 bytes into mm1 = (esi + eax)[0..3]
				movd mm3, [esi+2*eax]	;// lower 4 bytes into mm3 = (esi + 2 * eax)[0..3]
				punpcklbw mm0, mm7		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm0, mm1			;// mm0 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				movq mm1, mm0
				movq mm3, mm2
				pmullw mm0, PACKED_228	;// mm0 = LSW(mm0 * 228)
				pmulhw mm1, PACKED_228	;// mm1 = MSW(mm0 * 228)
				pmullw mm2, PACKED_70	;// mm2 = LSW(mm2 * 70)
				pmulhw mm3, PACKED_70	;// mm3 = MSW(mm2 * 70)
				movq mm4, mm0
				movq mm5, mm2
				punpcklwd mm0, mm1		;// unpack the lower 2 words into mm0 and mm1
				punpckhwd mm4, mm1		;// unpack the upper 2 words into mm4 and mm1
				punpcklwd mm2, mm3		;// unpack the lower 2 words into mm2 and mm3
				punpckhwd mm5, mm3		;// unpack the upper 2 words into mm5 and mm3
				paddd mm0, mm2			;// mm0 += mm2
				paddd mm4, mm5			;// mm4 += mm5

				lea ecx, [2*ebx+ebx]	;// ecx = -3*w
				lea edx, [2*eax+eax]	;// edx = 3*w

				movd mm2, [esi+ecx]		;// lower 4 bytes into mm2 = (esi + ecx)[0..3]
				movd mm6, [esi+2*ebx]	;// lower 4 bytes into mm6 = (esi + 2 * ebx)[0..3]
				movd mm1, [esi+edx]		;// lower 4 bytes into mm1 = (esi + edx)[0..3]
				movd mm3, [esi+4*eax]	;// lower 4 bytes into mm3 = (esi + 4 * eax)[0..3]
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm6, mm1			;// mm6 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				movq mm1, mm6
				movq mm3, mm2
				pmullw mm6, PACKED_MINUS37	;// mm6 = LSW(-37 * mm6)
				pmulhw mm1, PACKED_MINUS37	;// mm1 = MSW(-37 * mm6)
				pmullw mm2, PACKED_MINUS21	;// mm2 = LSW(-21 * mm2)
				pmulhw mm3, PACKED_MINUS21	;// mm3 = MSW(-21 * mm2)
				movq mm7, mm6
				movq mm5, mm2
				punpcklwd mm6, mm1		;// unpack the lower 2 words into mm6 and mm1
				punpckhwd mm7, mm1		;// unpack the upper 2 words into mm7 and mm1
				punpcklwd mm2, mm3		;// unpack the lower 2 words into mm2 and mm3
				punpckhwd mm5, mm3		;// unpack the upper 2 words into mm5 and mm3
				paddd mm6, mm2
				paddd mm7, mm5
				paddd mm0, mm6			;// mm0 += mm2 + mm6
				paddd mm4, mm7			;// mm4 += mm5 + mm7


				lea ecx, [4*ebx+ebx]	;// ecx = -5*w
				lea edx, [4*eax+eax]	;// edx = 5*w
				add eax, edx			;// eax = 6*w

				pxor mm7, mm7			;// mm7 = 0
				movd mm2, [esi+ecx]		;// lower 4 bytes into mm2 = (esi + ecx)[0..3]
				movd mm6, [esi+4*ebx]	;// lower 4 bytes into mm6 = (esi + 4 * ebx)[0..3]
				movd mm1, [esi+edx]		;// lower 4 bytes into mm1 = (esi + edx)[0..3]
				movd mm3, [esi+eax]		;// lower 4 bytes into mm3 = (esi + eax)[0..3]
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm6, mm1			;// mm6 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				movq mm1, mm6
				movq mm3, mm2
				pmullw mm6, PACKED_11	;// mm6 = LSW(mm6 * 11)
				pmulhw mm1, PACKED_11	;// mm1 = MSW(mm6 * 11)
				pmullw mm2, PACKED_5	;// mm2 = LSW(mm2 * 5)
				pmulhw mm3, PACKED_5	;// mm3 = MSW(mm2 * 5)
				movq mm7, mm6
				movq mm5, mm2
				punpcklwd mm6, mm1		;// unpack the lower 2 words into mm6 and mm1
				punpckhwd mm7, mm1		;// unpack the upper 2 words into mm7 and mm1
				punpcklwd mm2, mm3		;// unpack the lower 2 words into mm2 and mm3
				punpckhwd mm5, mm3		;// unpack the upper 2 words into mm5 and mm3
				paddd mm6, mm2
				paddd mm7, mm5
				paddd mm0, mm6
				paddd mm4, mm7
				paddd mm0, PACKED_256	;// mm0 += mm2 + mm6 + (256, 256, 256, 256)
				paddd mm4, PACKED_256	;// mm4 += mm5 + mm7 + (256, 256, 256, 256)
				psrld mm0, 9			;// mm0 >>= 9
				psrld mm4, 9			;// mm4 >>= 9
				packssdw mm0, mm4		;// pack mm0 and mm4
				packuswb mm0, mm0		;// pack mm0 to get result into the lower 4 bytes
				movd [edi], mm0			;// store result into edi[0..3]

				add esi, width			;// esi += width
				add edi, w				;// edi += w
				dec dword ptr j			;// decrement j
				jnz conv422to420__l1	;// loop while not zero
				mov s, esi				;// update s
				mov d, edi				;// update d
				emms					;// empty MMX state
			}

			// epilogue

			for(k = 0; k < 4; k++)
			{
				d[k] = clp[(int)(228*(s[k]+s[k+w])
								 +70*(s[k-w]+s[k+w*2])
								 -37*(s[k-w*2]+s[k+w*3])
								 -21*(s[k-w*3]+s[k+w*4])
								 +11*(s[k-w*4]+s[k+w*5])
								 + 5*(s[k-w*5]+s[k+w*6])+256)>>9];

				d[k+w] = clp[(int)(228*(s[k+w*2]+s[k+w*3])
								 +70*(s[k+w]+s[k+w*4])
								 -37*(s[k]+s[k+w*5])
								 -21*(s[k-w]+s[k+w*6])
								 +11*(s[k-w*2]+s[k+w*7])
								 + 5*(s[k-w*3]+s[k+w*8])+256)>>9];

				d[k+2*w] = clp[(int)(228*(s[k+w*4]+s[k+w*5])
								 +70*(s[k+w*3]+s[k+w*6])
								 -37*(s[k+w*2]+s[k+w*7])
								 -21*(s[k+w]+s[k+w*7])
								 +11*(s[k]+s[k+w*7])
								 + 5*(s[k-w]+s[k+w*7])+256)>>9];

				d[k+3*w] = clp[(int)(228*(s[k+w*6]+s[k+w*7])
								 +70*(s[k+w*5]+s[k+w*7])
								 -37*(s[k+w*4]+s[k+w*7])
								 -21*(s[k+w*3]+s[k+w*7])
								 +11*(s[k+w*2]+s[k+w*7])
								 + 5*(s[k+w]+s[k+w*7])+256)>>9];
			}

			src+=4;
			dst+=4;
		} while(--i);
	}
	else
	{
		for (i=0; i<w; i++)
		{
			for (j=0; j<height; j+=2)
			{
				jm5 = (j<5) ? 0 : j-5;
				jm4 = (j<4) ? 0 : j-4;
				jm3 = (j<3) ? 0 : j-3;
				jm2 = (j<2) ? 0 : j-2;
				jm1 = (j<1) ? 0 : j-1;
				jp1 = (j<height-1) ? j+1 : height-1;
				jp2 = (j<height-2) ? j+2 : height-1;
				jp3 = (j<height-3) ? j+3 : height-1;
				jp4 = (j<height-4) ? j+4 : height-1;
				jp5 = (j<height-5) ? j+5 : height-1;
				jp6 = (j<height-5) ? j+6 : height-1;

				// FIR filter with 0.5 sample interval phase shift
				v = clp[(int)(228*(src[w*j]+src[w*jp1])
								 +70*(src[w*jm1]+src[w*jp2])
								 -37*(src[w*jm2]+src[w*jp3])
								 -21*(src[w*jm3]+src[w*jp4])
								 +11*(src[w*jm4]+src[w*jp5])
								 + 5*(src[w*jm5]+src[w*jp6])+256)>>9];

				if(dst[w*(j>>1)] != v)
				{
					v = v;
				}

				dst[w*(j>>1)] = v;
			}
			src++;
			dst++;
		}
	}
}
