/* putmpg.c, 
 *块和运动向量编码程序
 *   */



#include <stdio.h>
#include "global.h"

/* 为帧内编码块产生变长编码 */
void putintrablk(blk,cc)
short *blk;
int cc;
{
  int n, dct_diff, run, signed_level;

  /* DC 系数 */
  dct_diff = blk[0] - dc_dct_pred[cc]; /* 与前一块的差值 */
  dc_dct_pred[cc] = blk[0];

  if (cc==0)
    putDClum(dct_diff);
  else
    putDCchrom(dct_diff);

  /* AC 系数 */
  run = 0;
  for (n=1; n<64; n++)
  {
    /* 使用一个合适的熵编码扫描方式*/
    signed_level = blk[(altscan ? alternate_scan : zig_zag_scan)[n]];
    if (signed_level!=0)
    {
      putAC(run,signed_level,intravlc);
      run = 0;
    }
    else
      run++; /* 对0系数计数 */
  }

  /* 结束一个块，并加上标准的终结码字*/ 
  if (intravlc)
    putbits(6,4); /* 0110  (表 B-15)  */
  else
    putbits(2,2); /* 10 10 (表 B-14) */
}

/* 为非帧内编码块产生变长编码 */
void putnonintrablk(blk)
short *blk;
{
  int n, run, signed_level, first;

  run = 0;
  first = 1;

  for (n=0; n<64; n++)
  {
     /* 使用一个合适的熵编码扫描方式*/
    signed_level = blk[(altscan ? alternate_scan : zig_zag_scan)[n]];

    if (signed_level!=0)
    {
      if (first)
      {
        /* 非帧内块的第一个系数 */
        putACfirst(run,signed_level);
        first = 0;
      }
      else
        putAC(run,signed_level,0);

      run = 0;
    }
    else
      run++; /* 对0系数计数 */
  }

  /* 结束一个块，并加上标准的终结码字*/ 
  putbits(2,2);
}

/* 为运动向量产生变长编码 */
void putmv(dmv,f_code)
int dmv,f_code;
{
  int r_size, f, vmin, vmax, dv, temp, motion_code, motion_residual;

  r_size = f_code - 1; /* 固定长度编码的比特数 */
  f = 1<<r_size;
  vmin = -16*f; /* 下限 */
  vmax = 16*f - 1; /* 上限*/
  dv = 32*f;

  if (dmv>vmax)
    dmv-= dv;
  else if (dmv<vmin)
    dmv+= dv;

  /* 检测值是否正确 */
  if (dmv<vmin || dmv>vmax)
    if (!quiet)
      fprintf(stderr,"invalid motion vector\n");

  /* 将dmv分成dmvmotion_code和motion_residual */
  temp = ((dmv<0) ? -dmv : dmv) + f - 1;
  motion_code = temp>>r_size;
  if (dmv<0)
    motion_code = -motion_code;
  motion_residual = temp & (f-1);

  putmotioncode(motion_code); /* 变长编码 */

  if (r_size!=0 && motion_code!=0)
    putbits(motion_residual,r_size); /* 定长编码 */
}
