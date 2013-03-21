/* puthdr.c, 产生头信息*/



#include <stdio.h>
#include <math.h>
#include "global.h"

/* 函数声明 */
static int frametotc(int frame);

/* 产生序列头
 *
 */
void putseqhdr()
{
  int i;

  alignbits();
  putbits(SEQ_START_CODE,32); 
  putbits(horizontal_size,12); 
  putbits(vertical_size,12); 
  putbits(aspectratio,4); 
  putbits(frame_rate_code,4); 
  putbits((int)ceil(bit_rate/400.0),18); 
  putbits(1,1); 
  putbits(vbv_buffer_size,10); 
  putbits(constrparms,1); 

  putbits(load_iquant,1); 
  if (load_iquant)
    for (i=0; i<64; i++)  /*矩阵以之形顺序下载 */
      putbits(intra_q[zig_zag_scan[i]],8); 

  putbits(load_niquant,1); 
  if (load_niquant)
    for (i=0; i<64; i++)
      putbits(inter_q[zig_zag_scan[i]],8); 
}

/* 产生序列的扩展头*/
void putseqext()
{
  alignbits();
  putbits(EXT_START_CODE,32); 
  putbits(SEQ_ID,4); 
  putbits((profile<<4)|level,8); 
  putbits(prog_seq,1); 
  putbits(chroma_format,2); 
  putbits(horizontal_size>>12,2); 
  putbits(vertical_size>>12,2); 
  putbits(((int)ceil(bit_rate/400.0))>>18,12); 
  putbits(1,1); 
  putbits(vbv_buffer_size>>10,8); 
  putbits(0,1); 
  putbits(0,2); 
  putbits(0,5); 
}

/* 产生序列显示的扩展头信息
 *
 */
void putseqdispext()
{
  alignbits();
  putbits(EXT_START_CODE,32); 
  putbits(DISP_ID,4); 
  putbits(video_format,3); 
  putbits(1,1); 
  putbits(color_primaries,8); 
  putbits(transfer_characteristics,8); 
  putbits(matrix_coefficients,8); 
  putbits(display_horizontal_size,14); 
  putbits(1,1); 
  putbits(display_vertical_size,14); 
}

void putuserdata(userdata)
char *userdata;
{
  alignbits();
  putbits(USER_START_CODE,32); 
  while (*userdata)
    putbits(*userdata++,8);
}

/* 产生组图像的头信息
 *
 */
void putgophdr(frame,closed_gop)
int frame,closed_gop;
{
  int tc;

  alignbits();
  putbits(GOP_START_CODE,32); /* group_start_code */
  tc = frametotc(tc0+frame);
  putbits(tc,25); /* time_code */
  putbits(closed_gop,1); /* closed_gop */
  putbits(0,1); /* broken_link */
}

static int frametotc(frame)
int frame;
{
  int fps, pict, sec, minute, hour, tc;

  fps = (int)(frame_rate+0.5);
  pict = frame%fps;
  frame = (frame-pict)/fps;
  sec = frame%60;
  frame = (frame-sec)/60;
  minute = frame%60;
  frame = (frame-minute)/60;
  hour = frame%24;
  tc = (hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;

  return tc;
}

void putpicthdr()
{
  alignbits();
  putbits(PICTURE_START_CODE,32); 
  calc_vbv_delay();
  putbits(temp_ref,10); 
  putbits(pict_type,3); 
  putbits(vbv_delay,16); 

  if (pict_type==P_TYPE || pict_type==B_TYPE)
  {
    putbits(0,1); 
      putbits(forw_hor_f_code,3);
  }

  if (pict_type==B_TYPE)
  {
    putbits(0,1); 
      putbits(back_hor_f_code,3);
  }

  putbits(0,1);
}

/* 产生图像编码扩展
 *
 */
void putpictcodext()
{
  alignbits();
  putbits(EXT_START_CODE,32); 
  putbits(CODING_ID,4); 
  putbits(forw_hor_f_code,4); 
  putbits(forw_vert_f_code,4); 
  putbits(back_hor_f_code,4); 
  putbits(back_vert_f_code,4); 
  putbits(dc_prec,2); 
  putbits(pict_struct,2); 
  putbits((pict_struct==FRAME_PICTURE)?topfirst:0,1); 
  putbits(frame_pred_dct,1); 
  putbits(0,1); 
  putbits(q_scale_type,1); 
  putbits(intravlc,1); 
  putbits(altscan,1); 
  putbits(repeatfirst,1); 
  putbits(prog_frame,1); 
  putbits(prog_frame,1); 
  putbits(0,1); 
}

/* 产生序列结束字节*/
void putseqend()
{
  alignbits();
  putbits(SEQ_END_CODE,32);
}
