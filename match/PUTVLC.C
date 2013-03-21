/* putvlc.c, �����䳤����ĳ���       */



#include <stdio.h>

#include "global.h"
#include "vlc.h"

/* ˽�к��� */
static void putDC(sVLCtable *tab, int val);

//* ������DCϵ�����пɱ䳤����*/
void putDClum(val)
int val;
{
  putDC(DClumtab,val);
}

/* ��ɫ��DCϵ�����пɱ䳤����*/
void putDCchrom(val)
int val;
{
  putDC(DCchromtab,val);
}

/* �ɱ䳤����ľ��庯��*/
static void putDC(tab,val)
sVLCtable *tab;
int val;
{
  int absval, size;

  absval = (val<0) ? -val : val; /* abs(val) */

  if (absval>255)
  {
      /* �������Ӧ�ñ�֤��Զ��Ҫ����*/
    sprintf(errortext,"DC value out of range (%d)\n",val);
    error(errortext);
  }

 /* ����DCϵ��DCT���Ĵ�С*/
  size = 0;

  while (absval)
  {
    absval >>= 1;
    size++;
  }

  /* Ϊdct_dc_size �����䳤���� */
  putbits(tab[size].code,tab[size].len);

  /* ���Ӷ������� */
  if (size!=0)
  {
    if (val>=0)
      absval = val;
    else
      absval = val + (1<<size) - 1; /* val + (2 ^ size) - 1 */
    putbits(absval,size);
  }
}

/* �Է�֡�ڿ��AC�ĵ�һ��ϵ�����б䳤����*/
void putACfirst(run,val)
int run,val;
{
  if (run==0 && (val==1 || val==-1)) 
    putbits(2|(val<0),2); /* ���� '1s' (s=sign), (Table B-14, line 2) */
  else
    putAC(run,val,0); /* ������ϵ������AC�ı䳤����һ�� */
}

/* ��DCT��������ֵ���б���*/
void putAC(run,signed_level,vlcformat)
int run,signed_level,vlcformat;
{
  int level, len;
  VLCtable *ptab;

  level = (signed_level<0) ? -signed_level : signed_level; 

  /* Ҫȷ���γ���Ч*/
  if (run<0 || run>63 || level==0 || level>255)
  {
    sprintf(errortext,"AC value out of range (run=%d, signed_level=%d)\n",
      run,signed_level);
    error(errortext);
  }

  len = 0;

  if (run<2 && level<41)
  {
    /* vlc�ĸ�ʽ���ñ�Table B-14 �� B-15 */
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

  if (len!=0) /* ��ʾ�Ѿ����ڱ䳤������*/
  {
    putbits(ptab->code,len);
    putbits(signed_level<0,1); 
  }
  else
  {
    /* �����(run, level)����ϣ�û�к��ʵ�VLC�������escape��ʽ */
    putbits(1l,6); /* Escape �ֶ�*/
    putbits(run,6); 
      /* ISO/IEC 11172-2 ���õ���8 �� 16 bit �ı���*/
	if (signed_level>127)
        putbits(0,8);
      if (signed_level<-127)
        putbits(128,8);
      putbits(signed_level,8);
  }
}

/* ��macroblock_address_increment���б䳤����*/
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

/* ��macroblock_type ���б䳤���� */
void putmbtype(pict_type,mb_type)
int pict_type,mb_type;
{
  putbits(mbtypetab[pict_type-1][mb_type].code,
          mbtypetab[pict_type-1][mb_type].len);
}

/* ��motion_code ���б䳤���� */
void putmotioncode(motion_code)
int motion_code;
{
  int abscode;

  abscode = (motion_code>=0) ? motion_code : -motion_code; 
  putbits(motionvectab[abscode].code,motionvectab[abscode].len);
  if (motion_code!=0)
    putbits(motion_code<0,1); 
}

/* ��dmvector[t] ���б䳤���� */
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

/* ��coded_block_pattern ���б䳤���룬û��ʵ��4:2:2��4:4:4
 *
 
 */
void putcbp(cbp)
int cbp;
{
  putbits(cbptable[cbp].code,cbptable[cbp].len);
}