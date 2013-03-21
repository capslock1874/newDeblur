/* putseq.c, 基于序列级的程序*/



#include <windows.h>
#include <vfw.h>

#include <stdio.h>
#include <string.h>
#include "global.h"
#include "video.h"

int sec_remaining = 0;
int time_valid = 0;

void putseq()
{
  /* 该程序假定(N % M) == 0  N=15 M=3*/
  int i, j, k, f, f0, n, np, nb, sxf, syf, sxb, syb;
  int ipflag;


  char name[256];
  unsigned char *neworg[3], *newref[3];
  static char ipb[5] = {' ','I','P','B','D'};

  vbv_ovflow = vbv_unflow = 0;
  start_time = 0;

  rc_init_seq(); //初始化图像序列

  putseqhdr();//写图像序列头

  if (strlen(id_string) > 1)
    putuserdata(id_string);
/*判断ipb帧，并做相应的记录*/
  for (i=0; i<nframes; i++)
  {
    if (!quiet)
    {
	  fprintf(stderr, "                                                                               \r");
      fprintf(stderr,"frame %u",i);
      fflush(stderr);
    }

    f0 = N*((i+(M-1))/N) - (M-1);

    if (f0<0)
      f0=0;

    if (i==0 || (i-1)%M==0)
    {
      /* I or P frame */
      for (j=0; j<3; j++)
      {
        /* shuffle reference frames */
        neworg[j] = oldorgframe[j];
        newref[j] = oldrefframe[j];
        oldorgframe[j] = neworgframe[j];
        oldrefframe[j] = newrefframe[j];
        neworgframe[j] = neworg[j];
        newrefframe[j] = newref[j];
      }

      f = (i==0) ? 0 : i+M-1;
      if (f>=nframes)
        f = nframes - 1;

      if (i==f0) /* first displayed frame in GOP is I */
      {
        /* I frame */
        pict_type = I_TYPE;
        forw_hor_f_code = forw_vert_f_code = 15;
        back_hor_f_code = back_vert_f_code = 15;

        /* n: 当前GOP中的帧数
         *
         */
        n = (i==0) ? N-(M-1) : N;

        /* last GOP may contain less frames */
        if (n > nframes-f0)
          n = nframes-f0;

        if (i==0)
          np = (n + 2*(M-1))/M - 1; /* first GOP */
        else
          np = (n + (M-1))/M - 1;

        nb = n - np - 1;

        rc_init_GOP(np,nb);


        if(wr_seqh2gop) {
          if(!((i/N)%wr_seqh2gop)&&(i))
            putseqhdr();
          }
        putgophdr(f0,i==0); 
      }
      else
      {
        /* P 帧 */
        pict_type = P_TYPE;
        forw_hor_f_code = motion_data[0].forw_hor_f_code;
        forw_vert_f_code = motion_data[0].forw_vert_f_code;
        back_hor_f_code = back_vert_f_code = 15;
        sxf = motion_data[0].sxf;
        syf = motion_data[0].syf;
      }
    }
    else
    {
      /* B 帧 */
      for (j=0; j<3; j++)
      {
        neworg[j] = auxorgframe[j];
        newref[j] = auxframe[j];
      }

      f = i - 1;
      pict_type = B_TYPE;
      n = (i-2)%M + 1; 
      forw_hor_f_code = motion_data[n].forw_hor_f_code;
      forw_vert_f_code = motion_data[n].forw_vert_f_code;
      back_hor_f_code = motion_data[n].back_hor_f_code;
      back_vert_f_code = motion_data[n].back_vert_f_code;
      sxf = motion_data[n].sxf;
      syf = motion_data[n].syf;
      sxb = motion_data[n].sxb;
      syb = motion_data[n].syb;
    }

    temp_ref = f - f0;
    frame_pred_dct = frame_pred_dct_tab[pict_type-1];
    q_scale_type = qscale_tab[pict_type-1];
    intravlc = intravlc_tab[pict_type-1];
    altscan = altscan_tab[pict_type-1];

#ifdef DEBUG
    fprintf(statfile,"\nFrame %d (#%d in display order):\n",i,f);
    fprintf(statfile," picture_type=%c\n",ipb[pict_type]);
    fprintf(statfile," temporal_reference=%d\n",temp_ref);
    fprintf(statfile," frame_pred_frame_dct=%d\n",frame_pred_dct);
    fprintf(statfile," q_scale_type=%d\n",q_scale_type);
    fprintf(statfile," intra_vlc_format=%d\n",intravlc);
    fprintf(statfile," alternate_scan=%d\n",altscan);

    if (pict_type!=I_TYPE)
    {
      fprintf(statfile," forward search window: %d...%d / %d...%d\n",
        -sxf,sxf,-syf,syf);
      fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
        -(4<<forw_hor_f_code),(4<<forw_hor_f_code)-1,
        -(4<<forw_vert_f_code),(4<<forw_vert_f_code)-1);
    }

    if (pict_type==B_TYPE)
    {
      fprintf(statfile," backward search window: %d...%d / %d...%d\n",
        -sxb,sxb,-syb,syb);
      fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
        -(4<<back_hor_f_code),(4<<back_hor_f_code)-1,
        -(4<<back_vert_f_code),(4<<back_vert_f_code)-1);
    }
#else
	if(!quiet)
	{
		fprintf(stderr,", Type=%c",ipb[pict_type]);

		if(start_time == 0)
			time(&start_time);
		fprintf(stderr, ", %u%% complete ", i*100/(totalFrames*frame_repl));
	}
#endif

	/*读取avi的每一帧，包括8，16，24，32位，并转化为YUV格式*/
    readframe(f+frame0, neworg);

    if (fieldpic)
    {
      if (!quiet)
      {
        fprintf(stderr,"\nfirst field  (%s) ",topfirst ? "top" : "bot");
        fflush(stderr);
      }

      pict_struct = topfirst ? TOP_FIELD : BOTTOM_FIELD;

      motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        neworg[0],newref[0],
                        sxf,syf,sxb,syb,mbinfo,0,0);

      predict(oldrefframe,newrefframe,predframe,0,mbinfo);
      dct_type_estimation(predframe[0],neworg[0],mbinfo);
      transform(predframe,neworg,mbinfo,blocks);

      putpict(neworg[0]);

      for (k=0; k<mb_height2*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,newref,mbinfo,blocks);
#ifdef DEBUG
      calcSNR(neworg,newref);
      stats();
#endif

      if (!quiet)
      {
        fprintf(stderr,"second field (%s) ",topfirst ? "bot" : "top");
        fflush(stderr);
      }

      pict_struct = topfirst ? BOTTOM_FIELD : TOP_FIELD;

      ipflag = (pict_type==I_TYPE);
      if (ipflag)
      {
        /* 第一帧 = I, 第二帧 = P */
        pict_type = P_TYPE;
        forw_hor_f_code = motion_data[0].forw_hor_f_code;
        forw_vert_f_code = motion_data[0].forw_vert_f_code;
        back_hor_f_code = back_vert_f_code = 15;
        sxf = motion_data[0].sxf;
        syf = motion_data[0].syf;
      }

      motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        neworg[0],newref[0],
                        sxf,syf,sxb,syb,mbinfo,1,ipflag);

      predict(oldrefframe,newrefframe,predframe,1,mbinfo);
      dct_type_estimation(predframe[0],neworg[0],mbinfo);
      transform(predframe,neworg,mbinfo,blocks);

      putpict(neworg[0]);

      for (k=0; k<mb_height2*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,newref,mbinfo,blocks);
#ifdef DEBUG
      calcSNR(neworg,newref);
      stats();
#endif
    }
    else
    {
      pict_struct = FRAME_PICTURE;
      motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        neworg[0],newref[0],
                        sxf,syf,sxb,syb,mbinfo,0,0);

      predict(oldrefframe,newrefframe,predframe,0,mbinfo);
      dct_type_estimation(predframe[0],neworg[0],mbinfo);
      transform(predframe,neworg,mbinfo,blocks);

      putpict(neworg[0]);

      for (k=0; k<mb_height*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,newref,mbinfo,blocks);
#ifdef DEBUG
      calcSNR(neworg,newref);
      stats();
#endif
    }

    sprintf(name,tplref,f+frame0);
    writeframe(name,newref);

  }

  putseqend();
  flushbits();
}
