/* fdctref.c, 前向离散余弦变换，双精度      */


extern unsigned int doublePrecision;
#include <math.h>

#ifndef PI
# ifdef M_PI
#  define PI M_PI
# else
#  define PI 3.14159265358979323846
# endif
#endif

/* 声明全局函数 */
void init_fdct(void);
void fdct(short *block);

/*声明私有变量*/
static double c[8][8];     /*采用浮点运算时的转换系数*/
static short   c2[8][8];   /*采用整数运算时的转换系数*/


/* 关掉由长整型数据转换成双精度数据时产生的警告信息*/
#pragma warning( disable : 4244 )

void init_fdct()
{
	int i, j;
	double s;
	double sTmp;

	for (i=0; i<8; i++)
	{
		s = (i==0) ? sqrt(0.125) : 0.5;

		for (j=0; j<8; j++)
		{
			c[i][j] = s * cos((PI/8.0)*i*(j+0.5));
			/* 保存整型数据并左移14位*/
			sTmp = c[i][j] * 16384;
			c2[i][j] = floor(sTmp+0.499999);
		}
	}
}

/*该函数用来完成DCT变换*/


void fdct(block)
short *block;
{
	int        i, j, k;
	short*     pBlock;
	short*     pC;
	long       s2;
	short      tmp2[64];
	short*     pTmp2;
	double     s;
	double     tmp[64];


	for (i=0; i<8; i++)
		for (j=0; j<8; j++)
		{
		/*
		* 如果采用的是MMX处理器，则可以用整数运算替代浮点运算来加速处理过程
		* 
		*/
		if (!doublePrecision)
		{
			s2 = 0;
			pC = (short*)c2 + (j*8);
			pBlock = block + (8*i);

			__asm {
			mov         eax, pBlock       ;
			movq        mm0, [eax]        ; //将块0-3放入地址mm0
			movq        mm1, [eax+8]      ; //将块4-7放入地址mm1
			mov         eax, pC           ;
			movq        mm3, [eax]        ;
			movq        mm4, [eax+8]      ;
			pmaddwd     mm0, mm3          ;
			pmaddwd     mm1, mm4          ;

			movd        eax, mm0          ; 
			psrlq       mm0, 0x20         ;
			movd        ebx, mm0          ; 
			add         eax, ebx

			movd        ebx, mm1          ; 
			add         eax, ebx
			psrlq       mm1, 0x20         ;
			movd        ebx, mm1          ; 
			add         eax, ebx

			sar         eax, 14           ;  // 带符号右移14 bits
			jnc         NO_ROUNDING1      ; // 四舍五入到最近的整数
			inc         eax
NO_ROUNDING1:
            mov         s2, eax
			emms
			}
			tmp2[8*j+i] = s2;
		}
		else
		/* 下面采用的是浮点运算的方法*/
		{
			s = 0.0;
			for (k=0; k<8; k++)
				s += c[j][k] * block[8*i+k];
			tmp[8*i+j] = s;
		}
    }

	for (j=0; j<8; j++)
		for (i=0; i<8; i++)
		{
			if (!doublePrecision)
			{
				s2 = 0;
				pC = (short*)c2 + (i*8);
				pTmp2 = tmp2 + (j*8);

				__asm
				{
				mov         eax, pTmp2        ;
				movq        mm0, [eax]        ; 
				movq        mm1, [eax+8]      ; 
				mov         eax, pC           ;
				movq        mm3, [eax]        ;
				movq        mm4, [eax+8]      ;
				pmaddwd     mm0, mm3          ;
				pmaddwd     mm1, mm4          ;

				movd        eax, mm0          ; 
				psrlq       mm0, 0x20         ;
				movd        ebx, mm0          ; 
				add         eax, ebx

				movd        ebx, mm1          ; 
				add         eax, ebx
				psrlq       mm1, 0x20         ;
				movd        ebx, mm1          ; 
				add         eax, ebx
				sar         eax, 14           ; 
				jnc         NO_ROUNDING2      ; 
				inc         eax
NO_ROUNDING2:
				mov         s2, eax
				emms
			}
			block[8*i+j] = s2;
		}
		else
		{
			s = 0.0;
			for (k=0; k<8; k++)
				s += c[i][k] * tmp[8*k+j];
			block[8*i+j] = (short)floor(s+0.499999);
		}
	}
}

#pragma warning( default : 4244 )
