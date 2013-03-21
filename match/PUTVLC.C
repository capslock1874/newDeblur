/* putvlc.c, 产生变长编码的程序       */



#include <stdio.h>

#include "global.h"
#include "vlc.h"

/* 私有函数 */
static void putDC(sVLCtable *tab, int val);

//* 对亮度DC系数进行可变长编码*/
void putDClum(val)
int val;
{
  putDC(DClumtab,val);
}

/* 对色度DC系数进行可变长编码*/
void putDCchrom(val)
int val;
{
  putDC(DCchromtab,val);
}

/* 可变长编码的具体函数*/
static void putDC(tab,val)
sVLCtable *tab;
int val;
{
  int absval, size;

  absval = (val<0) ? -val : val; /* abs(val) */

  if (absval>255)
  {
      /* 这种情况应该保证永远不要发生*/
    sprintf(errortext,"DC value out of range (%d)\n",val);
    error(errortext);
  }

 /* 计算DC系数DCT表的大小*/
  size = 0;

  while (absval)
  {
    absval >>= 1;
    size++;
  }

  /* 为dct_dc_size 产生变长编码 */
  putbits(tab[size].code,tab[size].len);

  /* 附加定长编码 */
  if (size!=0)
  {
    if (val>=0)
      absval = val;
    else
      absval = val + (1<<size) - 1; /* val + (2 ^ size) - 1 */
    putbits(absval,size);
  }
}

/* 对非帧内块的AC的第一个系数进行变长编码*/
void putACfirst(run,val)
int run,val;
{
  if (run==0 && (val==1 || val==-1)) 
    putbits(2|(val<0),2); /* 产生 '1s' (s=sign), (Table B-14, line 2) */
  else
    putAC(run,val,0); /* 对其他系数则与AC的变长编码一样 */
}

/* 对DCT表的其他值进行编码*/
void putAC(run,signed_level,vlcformat)
int run,signed_level,vlcformat;
{
  int level, len;
  VLCtable *ptab;

  level = (signed_level<0) ? -signed_level : signed_level; 

  /* 要确保游程有效*/
  if (run<0 || run>63 || level==0 || level>255)
  {
    sprintf(errortext,"AC value out of range (run=%d, signed_level=%d)\n",
      run,signed_level);
    error(errortext);
  }

  len = 0;

  if (run<2 && level<41)
  {
    /* vlc的格式采用表Table B-14 或 B-15 */
    if (vlcformat)
      ptab = &dct_code_tab1a[run][level-1];
    else
      ptab = &dct_code_tab1[run][level-1];

    len = ptab->len;
  }
  else if (run<32 && level<6)
  {
    if (vlcformat)
      ptab = &dct_code_tab2a[run-2][level-1];
    else
      ptab = &dct_code_tab2[run-2][level-1];

    len = ptab->len;
  }

  if (len!=0) /* 表示已经存在变长编码了*/
  {
    putbits(ptab->code,len);
    putbits(signed_level<0,1); 
  }
  else
  {
    /* 对这个(run, level)的组合，没有合适的VLC，则采用escape方式 */
    putbits(1l,6); /* Escape 字段*/
    putbits(run,6); 
      /* ISO/IEC 11172-2 采用的是8 或 16 bit 的编码*/
	if (signed_level>127)
        putbits(0,8);
      if (signed_level<-127)
        putbits(128,8);
      putbits(signed_level,8);
  }
}

/* 对macroblock_address_increment进行变长编码*/
void putaddrinc(addrinc)
int addrinc;
{
  while (addrinc>33)
  {
    putbits(0x08,11); 
    addrinc-= 33;
  }

  putbits(addrinctab[addrinc-1].code,addrinctab[addrinc-1].len);
}

/* 对macroblock_type 进行变长编码 */
void putmbtype(pict_type,mb_type)
int pict_type,mb_type;
{
  putbits(mbtypetab[pict_type-1][mb_type].code,
          mbtypetab[pict_type-1][mb_type].len);
}

/* 对motion_code 进行变长编码 */
void putmotioncode(motion_code)
int motion_code;
{
  int abscode;

  abscode = (motion_code>=0) ? motion_code : -motion_code; 
  putbits(motionvectab[abscode].code,motionvectab[abscode].len);
  if (motion_code!=0)
    putbits(motion_code<0,1); 
}

/* 对dmvector[t] 进行变长编码 */
void putdmv(dmv)
int dmv;
{
  if (dmv==0)
    putbits(0,1);
  else if (dmv>0)
    putbits(2,2);
  else
    putbits(3,2);
}

/* 对coded_block_pattern 进行变长编码，没有实现4:2:2和4:4:4
 *
 
 */
void putcbp(cbp)
int cbp;
{
  putbits(cbptable[cbp].code,cbptable[cbp].len);
}
