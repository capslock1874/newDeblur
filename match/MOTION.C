/* motion.c, 运动预测程序模块                               */



#include <stdio.h>
#include "global.h"

/* 私有函数*/

static void frame_ME(unsigned char *oldorg, unsigned char *neworg,
  unsigned char *oldref, unsigned char *newref, unsigned char *cur,
  int i, int j, int sxf, int syf, int sxb, int syb, struct mbinfo *mbi);

static void field_ME(unsigned char *oldorg, unsigned char *neworg,
  unsigned char *oldref, unsigned char *newref, unsigned char *cur,
  unsigned char *curref, int i, int j, int sxf, int syf, int sxb, int syb,
  struct mbinfo *mbi, int secondfield, int ipflag);

static void frame_estimate(unsigned char *org,
  unsigned char *ref, unsigned char *mb,
  int i, int j,
  int sx, int sy, int *iminp, int *jminp, int *imintp, int *jmintp,
  int *iminbp, int *jminbp, int *dframep, int *dfieldp,
  int *tselp, int *bselp, int imins[2][2], int jmins[2][2]);

static void field_estimate(unsigned char *toporg,
  unsigned char *topref, unsigned char *botorg, unsigned char *botref,
  unsigned char *mb, int i, int j, int sx, int sy, int ipflag,
  int *iminp, int *jminp, int *imin8up, int *jmin8up, int *imin8lp,
  int *jmin8lp, int *dfieldp, int *d8p, int *selp, int *sel8up, int *sel8lp,
  int *iminsp, int *jminsp, int *dsp);

static void dpframe_estimate(unsigned char *ref,
  unsigned char *mb, int i, int j, int iminf[2][2], int jminf[2][2],
  int *iminp, int *jminp, int *imindmvp, int *jmindmvp,
  int *dmcp, int *vmcp);

static void dpfield_estimate(unsigned char *topref,
  unsigned char *botref, unsigned char *mb,
  int i, int j, int imins, int jmins, int *imindmvp, int *jmindmvp,
  int *dmcp, int *vmcp);

static int fullsearch(unsigned char *org, unsigned char *ref,
  unsigned char *blk,
  int lx, int i0, int j0, int sx, int sy, int h, int xmax, int ymax,
  int *iminp, int *jminp);

static int dist1(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h, int distlim);

static int dist2(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h);

static int bdist1(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int bdist2(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int variance(unsigned char *p, int lx);

static int calcDist16(unsigned char *p1In, unsigned char *p2In);

/* 私有变量 */

static unsigned char tmpResult[16];

// 下面的常数将用在MMX部分
static short PACKED_0[4] = { 0, 0, 0, 0 };
static short PACKED_1[4] = { 1, 1, 1, 1 };
static short PACKED_2[4] = { 2, 2, 2, 2 };


/*
 * 前向和交织帧的运动估计
 *
 *oldorg：前向预测的原始参考图（供P图像和B图像使用）
 *neworg：后向预测的原始参考图（仅供B图像使用）
 *oldref：前向预测的重建图像（P图像和B图像）
 *newref：后向预测的重建图像（B图像）
 *cur：当前原始图像帧（由预测图像产生的）
 *curref：当前的重建图像帧（用来根据第一个场预测第二个场）
 *sxf，syf：前向搜索窗口
 *sxb，syb：后向搜索窗口
 *mbi：指向宏块信息结构体的指针
 *输出结果mbi->
 *mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *MV[][][]: 运动向量 (场格式)
 *mv_field_sel: 顶部/底部场
 *motion_type: MC_FIELD, MC_16X8
 */
void motion_estimation(oldorg,neworg,oldref,newref,cur,curref,
  sxf,syf,sxb,syb,mbi,secondfield,ipflag)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur,*curref;
int sxf,syf,sxb,syb;
struct mbinfo *mbi;
int secondfield,ipflag;
{
  int i, j;

  for (j=0; j<height2; j+=16)
  {
    for (i=0; i<width; i+=16)
    {
      if (pict_struct==FRAME_PICTURE)
        frame_ME(oldorg,neworg,oldref,newref,cur,i,j,sxf,syf,sxb,syb,mbi);
      else
        field_ME(oldorg,neworg,oldref,newref,cur,curref,i,j,sxf,syf,sxb,syb,
          mbi,secondfield,ipflag);
      mbi++;
    }

    if (!quiet)
    {
      putc('.',stderr);
      fflush(stderr);
    }
  }
  if (!quiet)
    putc('\r',stderr);
}

static void frame_ME(oldorg,neworg,oldref,newref,cur,imin2,j,sxf,syf,sxb,syb,mbi)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur;
int imin2,j,sxf,syf,sxb,syb;
struct mbinfo *mbi;
{
  int imin,jmin,iminf,jminf,iminr,jminr;
  int imint,jmint,iminb,jminb;
  int imintf,jmintf,iminbf,jminbf;
  int imintr,jmintr,iminbr,jminbr;
  int var,v0;
  int dmc,dmcf,dmcr,dmci,vmc,vmcf,vmcr,vmci;
  int dmcfield,dmcfieldf,dmcfieldr,dmcfieldi;
  int tsel,bsel,tself,bself,tselr,bselr;
  unsigned char *mb;
  int imins[2][2],jmins[2][2];
  int imindp,jmindp,imindmv,jmindmv,dmc_dp,vmc_dp;

  mb = cur + imin2 + width*j;

  var = variance(mb,width);

  if (pict_type==I_TYPE)
    mbi->mb_type = MB_INTRA;
  else if (pict_type==P_TYPE)
  {
    if (frame_pred_dct)
    {
      dmc = fullsearch(oldorg,oldref,mb,
                       width,imin2,j,sxf,syf,16,width,height,&imin,&jmin);
      vmc = dist2(oldref+(imin>>1)+width*(jmin>>1),mb,
                  width,imin&1,jmin&1,16);
      mbi->motion_type = MC_FRAME;
    }
    else
    {
      frame_estimate(oldorg,oldref,mb,imin2,j,sxf,syf,
        &imin,&jmin,&imint,&jmint,&iminb,&jminb,
        &dmc,&dmcfield,&tsel,&bsel,imins,jmins);

      if (M==1)
        dpframe_estimate(oldref,mb,imin2,j>>1,imins,jmins,
          &imindp,&jmindp,&imindmv,&jmindmv,&dmc_dp,&vmc_dp);

      if (M==1 && dmc_dp<dmc && dmc_dp<dmcfield)
      {
        mbi->motion_type = MC_DMV;
        dmc = dmc_dp;
        vmc = vmc_dp;
      }
      else if (dmc<=dmcfield)
      {
        mbi->motion_type = MC_FRAME;
        vmc = dist2(oldref+(imin>>1)+width*(jmin>>1),mb,
                    width,imin&1,jmin&1,16);
      }
      else
      {
        mbi->motion_type = MC_FIELD;
        dmc = dmcfield;
        vmc = dist2(oldref+(tsel?width:0)+(imint>>1)+(width<<1)*(jmint>>1),
                    mb,width<<1,imint&1,jmint&1,8);
        vmc+= dist2(oldref+(bsel?width:0)+(iminb>>1)+(width<<1)*(jminb>>1),
                    mb+width,width<<1,iminb&1,jminb&1,8);
      }
    }

 
    if (vmc>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      
      v0 = dist2(oldref+imin2+width*j,mb,width,0,0,16);
      if (4*v0>5*vmc && v0>=9*256)
      {
        /* use MC */
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FRAME)
        {
          mbi->MV[0][0][0] = imin - (imin2<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
        }
        else if (mbi->motion_type==MC_DMV)
        {
          
          /* same parity vector */
          mbi->MV[0][0][0] = imindp - (imin2<<1);
          mbi->MV[0][0][1] = (jmindp<<1) - (j<<1);

          /* opposite parity vector */
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
         
          mbi->MV[0][0][0] = imint - (imin2<<1);
          mbi->MV[0][0][1] = (jmint<<1) - (j<<1);
          mbi->MV[1][0][0] = iminb - (imin2<<1);
          mbi->MV[1][0][1] = (jminb<<1) - (j<<1);
          mbi->mv_field_sel[0][0] = tsel;
          mbi->mv_field_sel[1][0] = bsel;
        }
      }
      else
      {
        /* No-MC */
        var = v0;
        mbi->mb_type = 0;
        mbi->motion_type = MC_FRAME;
        mbi->MV[0][0][0] = 0;
        mbi->MV[0][0][1] = 0;
      }
    }
  }
  else /* B帧图像*/
  {
    if (frame_pred_dct)
    {
      /* 前向 */
      dmcf = fullsearch(oldorg,oldref,mb,
                        width,imin2,j,sxf,syf,16,width,height,&iminf,&jminf);
      vmcf = dist2(oldref+(iminf>>1)+width*(jminf>>1),mb,
                   width,iminf&1,jminf&1,16);

      /* 后向 */
      dmcr = fullsearch(neworg,newref,mb,
                        width,imin2,j,sxb,syb,16,width,height,&iminr,&jminr);
      vmcr = dist2(newref+(iminr>>1)+width*(jminr>>1),mb,
                   width,iminr&1,jminr&1,16);

      /* 双向帧 */
      vmci = bdist2(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

      
      if (vmcf<=vmcr && vmcf<=vmci)
      {
        vmc = vmcf;
        mbi->mb_type = MB_FORWARD;
      }
      else if (vmcr<=vmci)
      {
        vmc = vmcr;
        mbi->mb_type = MB_BACKWARD;
      }
      else
      {
        vmc = vmci;
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      }

      mbi->motion_type = MC_FRAME;
    }
    else
    {
      /* 前向预测 */
      frame_estimate(oldorg,oldref,mb,imin2,j,sxf,syf,
        &iminf,&jminf,&imintf,&jmintf,&iminbf,&jminbf,
        &dmcf,&dmcfieldf,&tself,&bself,imins,jmins);

      /* 后向预测 */
      frame_estimate(neworg,newref,mb,imin2,j,sxb,syb,
        &iminr,&jminr,&imintr,&jmintr,&iminbr,&jminbr,
        &dmcr,&dmcfieldr,&tselr,&bselr,imins,jmins);

      /* 计算交织距离 */
      /* 帧 */
      dmci = bdist1(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

      /* 上半部分场 */
      dmcfieldi = bdist1(
                    oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                    newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);

      /* 下半部分场 */
      dmcfieldi+= bdist1(
                    oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                    newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);

      
      if (dmci<dmcfieldi && dmci<dmcf && dmci<dmcfieldf
          && dmci<dmcr && dmci<dmcfieldr)
      {
        /* 交织帧 */
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = bdist2(oldref+(iminf>>1)+width*(jminf>>1),
                     newref+(iminr>>1)+width*(jminr>>1),
                     mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);
      }
      else if (dmcfieldi<dmcf && dmcfieldi<dmcfieldf
               && dmcfieldi<dmcr && dmcfieldi<dmcfieldr)
      {
        /* 交织场*/
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = bdist2(oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                     newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                     mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);
        vmc+= bdist2(oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                     newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                     mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);
      }
      else if (dmcf<dmcfieldf && dmcf<dmcr && dmcf<dmcfieldr)
      {
        /* 前向帧 */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FRAME;
        vmc = dist2(oldref+(iminf>>1)+width*(jminf>>1),mb,
                    width,iminf&1,jminf&1,16);
      }
      else if (dmcfieldf<dmcr && dmcfieldf<dmcfieldr)
      {
        /* 前向场 */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FIELD;
        vmc = dist2(oldref+(tself?width:0)+(imintf>>1)+(width<<1)*(jmintf>>1),
                    mb,width<<1,imintf&1,jmintf&1,8);
        vmc+= dist2(oldref+(bself?width:0)+(iminbf>>1)+(width<<1)*(jminbf>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,8);
      }
      else if (dmcr<dmcfieldr)
      {
        /* 后向帧 */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = dist2(newref+(iminr>>1)+width*(jminr>>1),mb,
                    width,iminr&1,jminr&1,16);
      }
      else
      {
        /* 后向场 */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = dist2(newref+(tselr?width:0)+(imintr>>1)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintr&1,jmintr&1,8);
        vmc+= dist2(newref+(bselr?width:0)+(iminbr>>1)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbr&1,jminbr&1,8);
      }
    }

    /* 选择帧内编码或者非帧内编码
     *
     * 选择是基于帧内块方差和预测误差的方差
     *
     
     */
    if (vmc>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      var = vmc;
      if (mbi->motion_type==MC_FRAME)
      {
        /* 前向*/
        mbi->MV[0][0][0] = iminf - (imin2<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        /* 后向*/
        mbi->MV[0][1][0] = iminr - (imin2<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
      }
      else
      {
        
        /* 前向 */
        mbi->MV[0][0][0] = imintf - (imin2<<1);
        mbi->MV[0][0][1] = (jmintf<<1) - (j<<1);
        mbi->MV[1][0][0] = iminbf - (imin2<<1);
        mbi->MV[1][0][1] = (jminbf<<1) - (j<<1);
        mbi->mv_field_sel[0][0] = tself;
        mbi->mv_field_sel[1][0] = bself;
        /* 后向*/
        mbi->MV[0][1][0] = imintr - (imin2<<1);
        mbi->MV[0][1][1] = (jmintr<<1) - (j<<1);
        mbi->MV[1][1][0] = iminbr - (imin2<<1);
        mbi->MV[1][1][1] = (jminbr<<1) - (j<<1);
        mbi->mv_field_sel[0][1] = tselr;
        mbi->mv_field_sel[1][1] = bselr;
      }
    }
  }

  mbi->var = var;
}

/*
 * 场图像的运动估计
 *
 *oldorg：前向预测的原始参考图（供P图像和B图像使用）
 *neworg：后向预测的原始参考图（仅供B图像使用）
 *oldref：前向预测的重建图像（P图像和B图像）
 *newref：后向预测的重建图像（B图像）
 *cur：当前原始图像帧（由预测图像产生的）
 *curref：当前的重建图像帧（用来根据第一个场预测第二个场）
 *sxf，syf：前向搜索窗口
 *sxb，syb：后向搜索窗口
 *mbi：指向宏块信息结构体的指针
 *secondfield：指示一帧图像的第二个场
 *ipflag：指示当前P图像帧是由I图像帧所预测产生的

 *输出结果mbi->
 *mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *MV[][][]: 运动向量 (场格式)
 *mv_field_sel: 顶部/底部场
 *motion_type: MC_FIELD, MC_16X8

 */
static void field_ME(oldorg,neworg,oldref,newref,cur,curref,imin2,j,
  sxf,syf,sxb,syb,mbi,secondfield,ipflag)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur,*curref;
int imin2,j,sxf,syf,sxb,syb;
struct mbinfo *mbi;
int secondfield,ipflag;
{
  int w2;
  unsigned char *mb, *toporg, *topref, *botorg, *botref;
  int var,vmc,v0,dmc,dmcfieldi,dmc8i;
  int imin,jmin,imin8u,jmin8u,imin8l,jmin8l,dmcfield,dmc8,sel,sel8u,sel8l;
  int iminf,jminf,imin8uf,jmin8uf,imin8lf,jmin8lf,dmcfieldf,dmc8f,self,sel8uf,sel8lf;
  int iminr,jminr,imin8ur,jmin8ur,imin8lr,jmin8lr,dmcfieldr,dmc8r,selr,sel8ur,sel8lr;
  int imins,jmins,ds,imindmv,jmindmv,vmc_dp,dmc_dp;

  w2 = width<<1;

  mb = cur + imin2 + w2*j;
  if (pict_struct==BOTTOM_FIELD)
    mb += width;

  var = variance(mb,w2);

  if (pict_type==I_TYPE)
    mbi->mb_type = MB_INTRA;
  else if (pict_type==P_TYPE)
  {
    toporg = oldorg;
    topref = oldref;
    botorg = oldorg + width;
    botref = oldref + width;

    if (secondfield)
    {
      
      if (pict_struct==TOP_FIELD)
      {
        
        botorg = cur + width;
        botref = curref + width;
      }
      else
      {
        
        toporg = cur;
        topref = curref;
      }
    }

    field_estimate(toporg,topref,botorg,botref,mb,imin2,j,sxf,syf,ipflag,
                   &imin,&jmin,&imin8u,&jmin8u,&imin8l,&jmin8l,
                   &dmcfield,&dmc8,&sel,&sel8u,&sel8l,&imins,&jmins,&ds);

    if (M==1 && !ipflag)  
      dpfield_estimate(topref,botref,mb,imin2,j,imins,jmins,&imindmv,&jmindmv,
        &dmc_dp,&vmc_dp);

    
    if (M==1 && !ipflag && dmc_dp<dmc8 && dmc_dp<dmcfield)
    {
      
      mbi->motion_type = MC_DMV;
      dmc = dmc_dp;     
      vmc = vmc_dp;     

    }
    else if (dmc8<dmcfield)
    {
       /* 16x8 预测 */
      mbi->motion_type = MC_16X8;
      /* 上半部分宏块*/
      vmc = dist2((sel8u?botref:topref) + (imin8u>>1) + w2*(jmin8u>>1),
                  mb,w2,imin8u&1,jmin8u&1,8);
      /* lower half block */
      vmc+= dist2((sel8l?botref:topref) + (imin8l>>1) + w2*(jmin8l>>1),
                  mb+8*w2,w2,imin8l&1,jmin8l&1,8);
    }
    else
    {
      /* 下半部分宏块 */
      mbi->motion_type = MC_FIELD;
      vmc = dist2((sel?botref:topref) + (imin>>1) + w2*(jmin>>1),
                  mb,w2,imin&1,jmin&1,16);
    }

    /* 选择帧内编码还是非帧内编码*/
    if (vmc>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      
      if (!ipflag)
        v0 = dist2(((pict_struct==BOTTOM_FIELD)?botref:topref) + imin2 + w2*j,
                   mb,w2,0,0,16);
      if (ipflag || (4*v0>5*vmc && v0>=9*256))
      {
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FIELD)
        {
          mbi->MV[0][0][0] = imin - (imin2<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
          mbi->mv_field_sel[0][0] = sel;
        }
        else if (mbi->motion_type==MC_DMV)
        {
          
          mbi->MV[0][0][0] = imins - (imin2<<1);
          mbi->MV[0][0][1] = jmins - (j<<1);

         
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
          mbi->MV[0][0][0] = imin8u - (imin2<<1);
          mbi->MV[0][0][1] = jmin8u - (j<<1);
          mbi->MV[1][0][0] = imin8l - (imin2<<1);
          mbi->MV[1][0][1] = jmin8l - ((j+8)<<1);
          mbi->mv_field_sel[0][0] = sel8u;
          mbi->mv_field_sel[1][0] = sel8l;
        }
      }
      else
      {
        
        var = v0;
        mbi->mb_type = 0;
        mbi->motion_type = MC_FIELD;
        mbi->MV[0][0][0] = 0;
        mbi->MV[0][0][1] = 0;
        mbi->mv_field_sel[0][0] = (pict_struct==BOTTOM_FIELD);
      }
    }
  }
  else /* 如果为B帧图像*/
  {
    /* 前向预测*/
    field_estimate(oldorg,oldref,oldorg+width,oldref+width,mb,
                   imin2,j,sxf,syf,0,
                   &iminf,&jminf,&imin8uf,&jmin8uf,&imin8lf,&jmin8lf,
                   &dmcfieldf,&dmc8f,&self,&sel8uf,&sel8lf,&imins,&jmins,&ds);

     /*后向预测*/
    field_estimate(neworg,newref,neworg+width,newref+width,mb,
                   imin2,j,sxb,syb,0,
                   &iminr,&jminr,&imin8ur,&jmin8ur,&imin8lr,&jmin8lr,
                   &dmcfieldr,&dmc8r,&selr,&sel8ur,&sel8lr,&imins,&jmins,&ds);

     /* 计算双向预测的距离*/
    /* 场 */
    dmcfieldi = bdist1(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                       newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                       mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);

    /* 16x8 上半块*/
    dmc8i = bdist1(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

    /* 16x8 下半块 */
    dmc8i+= bdist1(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);

   /*选择距离最小的预测类型*/
    if (dmcfieldi<dmc8i && dmcfieldi<dmcfieldf && dmcfieldi<dmc8f
        && dmcfieldi<dmcfieldr && dmcfieldi<dmc8r)
    {
      /* 内插*/
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = bdist2(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                   newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                   mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);
    }
    else if (dmc8i<dmcfieldf && dmc8i<dmc8f
             && dmc8i<dmcfieldr && dmc8i<dmc8r)
    {
      /* 16x8, 内插*/
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_16X8;

      /* 上半块 */
      vmc = bdist2(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

      /* 下半块 */
      vmc+= bdist2(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);
    }
    else if (dmcfieldf<dmc8f && dmcfieldf<dmcfieldr && dmcfieldf<dmc8r)
    {
      
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_FIELD;
      vmc = dist2(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                  mb,w2,iminf&1,jminf&1,16);
    }
    else if (dmc8f<dmcfieldr && dmc8f<dmc8r)
    {
       /* 16x8, 前向*/
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_16X8;

      
      vmc = dist2(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                  mb,w2,imin8uf&1,jmin8uf&1,8);

     
      vmc+= dist2(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                  mb+8*w2,w2,imin8lf&1,jmin8lf&1,8);
    }
    else if (dmcfieldr<dmc8r)
    {
      
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = dist2(newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                  mb,w2,iminr&1,jminr&1,16);
    }
    else
    {
      
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_16X8;

     
      vmc = dist2(newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                  mb,w2,imin8ur&1,jmin8ur&1,8);

      vmc+= dist2(newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                  mb+8*w2,w2,imin8lr&1,jmin8lr&1,8);
    }

   
    if (vmc>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      var = vmc;
      if (mbi->motion_type==MC_FIELD)
      {
        /* 前向 */
        mbi->MV[0][0][0] = iminf - (imin2<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        mbi->mv_field_sel[0][0] = self;
        /* 后向 */
        mbi->MV[0][1][0] = iminr - (imin2<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
        mbi->mv_field_sel[0][1] = selr;
      }
      else /* MC_16X8 */
      {
        /* 前向 */
        mbi->MV[0][0][0] = imin8uf - (imin2<<1);
        mbi->MV[0][0][1] = jmin8uf - (j<<1);
        mbi->mv_field_sel[0][0] = sel8uf;
        mbi->MV[1][0][0] = imin8lf - (imin2<<1);
        mbi->MV[1][0][1] = jmin8lf - ((j+8)<<1);
        mbi->mv_field_sel[1][0] = sel8lf;
        /* 后向 */
        mbi->MV[0][1][0] = imin8ur - (imin2<<1);
        mbi->MV[0][1][1] = jmin8ur - (j<<1);
        mbi->mv_field_sel[0][1] = sel8ur;
        mbi->MV[1][1][0] = imin8lr - (imin2<<1);
        mbi->MV[1][1][1] = jmin8lr - ((j+8)<<1);
        mbi->mv_field_sel[1][1] = sel8lr;
      }
    }
  }

  mbi->var = var;
}

/*
 * 帧图像运动估计
 *
 
 */
static void frame_estimate(org,ref,mb,imin2,j,sx,sy,
  iminp,jminp,imintp,jmintp,iminbp,jminbp,dframep,dfieldp,tselp,bselp,
  imins,jmins)
unsigned char *org,*ref,*mb;
int imin2,j,sx,sy;
int *iminp,*jminp;
int *imintp,*jmintp,*iminbp,*jminbp;
int *dframep,*dfieldp;
int *tselp,*bselp;
int imins[2][2],jmins[2][2];
{
  int dt,db,dmint,dminb;
  int imint,iminb,jmint,jminb;

  /* frame prediction */
  *dframep = fullsearch(org,ref,mb,width,imin2,j,sx,sy,16,width,height,
                        iminp,jminp);

  /* predict top field from top field */
  dt = fullsearch(org,ref,mb,width<<1,imin2,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict top field from bottom field */
  db = fullsearch(org+width,ref+width,mb,width<<1,imin2,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);

  imins[0][0] = imint;
  jmins[0][0] = jmint;
  imins[1][0] = iminb;
  jmins[1][0] = jminb;

  /* select prediction for top field */
  if (dt<=db)
  {
    dmint=dt; *imintp=imint; *jmintp=jmint; *tselp=0;
  }
  else
  {
    dmint=db; *imintp=iminb; *jmintp=jminb; *tselp=1;
  }

  /* predict bottom field from top field */
  dt = fullsearch(org,ref,mb+width,width<<1,imin2,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict bottom field from bottom field */
  db = fullsearch(org+width,ref+width,mb+width,width<<1,imin2,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);

  imins[0][1] = imint;
  jmins[0][1] = jmint;
  imins[1][1] = iminb;
  jmins[1][1] = jminb;

  /* select prediction for bottom field */
  if (db<=dt)
  {
    dminb=db; *iminbp=iminb; *jminbp=jminb; *bselp=1;
  }
  else
  {
    dminb=dt; *iminbp=imint; *jminbp=jmint; *bselp=0;
  }

  *dfieldp=dmint+dminb;
}

/*
 * 场图片运动预测程序
 *
 * toporg：原始上半部分场地址
 *topref：重建的上半部分场地址
 *botorg：原始下半部分参考场地址
 *botref：重建的下半部分参考场的地址
 *mb：待匹配的宏块
 *i，j：宏块的位置（即搜索窗口的中心）
 *iminp，jminp，selp，dfieldp：最佳预测场的位置和距离
 *imin8up，jmin8up，sel8up：上半部分宏块的最佳预测的位置
 *imin8lp，jmin8lp，sel8lp：下半部分宏块的最佳预测的位置
 *d8p：最佳16×8的预测块的距离
 *iminsp，jminsp，dsp：最佳同奇偶性预测场的位置和距离
 */

static void field_estimate(toporg,topref,botorg,botref,mb,imin2,j,sx,sy,ipflag,
  iminp,jminp,imin8up,jmin8up,imin8lp,jmin8lp,dfieldp,d8p,selp,sel8up,sel8lp,
  iminsp,jminsp,dsp)
unsigned char *toporg, *topref, *botorg, *botref, *mb;
int imin2,j,sx,sy;
int ipflag;
int *iminp, *jminp;
int *imin8up, *jmin8up, *imin8lp, *jmin8lp;
int *dfieldp,*d8p;
int *selp, *sel8up, *sel8lp;
int *iminsp, *jminsp, *dsp;
{
  int dt, db, imint, jmint, iminb, jminb, notop, nobot;

  /* 如果ipflag为1则仅从对偶场预测 */
  notop = ipflag && (pict_struct==TOP_FIELD);
  nobot = ipflag && (pict_struct==BOTTOM_FIELD);

   /*场预测*/

  /* 从上半部分场预测当前场 */
  if (notop)
    dt = 65536; 
  else
    dt = fullsearch(toporg,topref,mb,width<<1,
                    imin2,j,sx,sy>>1,16,width,height>>1,
                    &imint,&jmint);

   /* 从下半部分场预测当前场*/
  if (nobot)
    db = 65536; 
  else
    db = fullsearch(botorg,botref,mb,width<<1,
                    imin2,j,sx,sy>>1,16,width,height>>1,
                    &iminb,&jminb);

  if (pict_struct==TOP_FIELD)
  {
    *iminsp = imint; *jminsp = jmint; *dsp = dt;
  }
  else
  {
    *iminsp = iminb; *jminsp = jminb; *dsp = db;
  }

    /* 选择预测场*/
  if (dt<=db)
  {
    *dfieldp = dt; *iminp = imint; *jminp = jmint; *selp = 0;
  }
  else
  {
    *dfieldp = db; *iminp = iminb; *jminp = jminb; *selp = 1;
  }


  /* 16x8 运动补偿 */

  if (notop)
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb,width<<1,
                    imin2,j,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);

  if (nobot)
    db = 65536;
  else
    db = fullsearch(botorg,botref,mb,width<<1,
                    imin2,j,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

  if (dt<=db)
  {
    *d8p = dt; *imin8up = imint; *jmin8up = jmint; *sel8up = 0;
  }
  else
  {
    *d8p = db; *imin8up = iminb; *jmin8up = jminb; *sel8up = 1;
  }

  if (notop)
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb+(width<<4),width<<1,
                    imin2,j+8,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);

  if (nobot)
    db = 65536;
  else
    db = fullsearch(botorg,botref,mb+(width<<4),width<<1,
                    imin2,j+8,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

    /* 为上半部分场选择预测场*/
  if (dt<=db)
  {
    *d8p += dt; *imin8lp = imint; *jmin8lp = jmint; *sel8lp = 0;
  }
  else
  {
    *d8p += db; *imin8lp = iminb; *jmin8lp = jminb; *sel8lp = 1;
  }
}

static void dpframe_estimate(ref,mb,imin2,j,iminf,jminf,
  iminp,jminp,imindmvp, jmindmvp, dmcp, vmcp)
unsigned char *ref, *mb;
int imin2,j;
int iminf[2][2], jminf[2][2];
int *iminp, *jminp;
int *imindmvp, *jmindmvp;
int *dmcp,*vmcp;
{
  int pref,ppred,delta_x,delta_y;
  int is,js,it,jt,ib,jb,it0,jt0,ib0,jb0;
  int imins,jmins,imint,jmint,iminb,jminb,imindmv,jmindmv;
  int vmc,local_dist;

  
  vmc = 1 << 30;

  for (pref=0; pref<2; pref++)
  {
    for (ppred=0; ppred<2; ppred++)
    {
      /* 将笛卡儿绝对坐标系转化成相对运动向量的值
       */
      is = iminf[pref][ppred] - (imin2<<1);
      js = jminf[pref][ppred] - (j<<1);

      if (pref!=ppred)
      {
        /* 垂直场偏调整*/
        if (ppred==0)
          js++;
        else
          js--;

        is<<=1;
        js<<=1;
        if (topfirst == ppred)
        {
          /* 第二场，比例因子为 1/3 */
          is = (is>=0) ? (is+1)/3 : -((-is+1)/3);
          js = (js>=0) ? (js+1)/3 : -((-js+1)/3);
        }
        else
          continue;
      }

      if (topfirst)
      {
        
        it0 = ((is+(is>0))>>1);
        jt0 = ((js+(js>0))>>1) - 1;

        
        ib0 = ((3*is+(is>0))>>1);
        jb0 = ((3*js+(js>0))>>1) + 1;
      }
      else
      {
        /* 用来根据底部场图象预测上部的运动向量 */
        it0 = ((3*is+(is>0))>>1);
        jt0 = ((3*js+(js>0))>>1) - 1;

         /* 用来根据顶部场图象预测下部的运动向量 */
        ib0 = ((is+(is>0))>>1);
        jb0 = ((js+(js>0))>>1) + 1;
      }

      
      is += imin2<<1;
      js += j<<1;
      it0 += imin2<<1;
      jt0 += j<<1;
      ib0 += imin2<<1;
      jb0 += j<<1;

      if (is >= 0 && is <= (width-16)<<1 &&
          js >= 0 && js <= (height-16))
      {
        for (delta_y=-1; delta_y<=1; delta_y++)
        {
          for (delta_x=-1; delta_x<=1; delta_x++)
          {
            
            it = it0 + delta_x;
            jt = jt0 + delta_y;
            ib = ib0 + delta_x;
            jb = jb0 + delta_y;

            if (it >= 0 && it <= (width-16)<<1 &&
                jt >= 0 && jt <= (height-16) &&
                ib >= 0 && ib <= (width-16)<<1 &&
                jb >= 0 && jb <= (height-16))
            {
              /* 计算预测错误 */
              local_dist = bdist2(
                ref + (is>>1) + (width<<1)*(js>>1),
                ref + width + (it>>1) + (width<<1)*(jt>>1),
                mb,             /* 当前宏块位置 */
                width<<1,       
                is&1, js&1, it&1, jt&1, 
                8);             /* 块高 */
              local_dist += bdist2(
                ref + width + (is>>1) + (width<<1)*(js>>1),
                ref + (ib>>1) + (width<<1)*(jb>>1),
                mb + width,     
                width<<1,       
                is&1, js&1, ib&1, jb&1, 
                8);             

              
              if (local_dist < vmc)
              {
                imins = is;
                jmins = js;
                imint = it;
                jmint = jt;
                iminb = ib;
                jminb = jb;
                imindmv = delta_x;
                jmindmv = delta_y;
                vmc = local_dist;
              }
            }
          }  /* 结束x循环 */
        } /* 结束y循环 */
      }
    }
  }

  local_dist = bdist1(
    ref + (imins>>1) + (width<<1)*(jmins>>1),
    ref + width + (imint>>1) + (width<<1)*(jmint>>1),
    mb,
    width<<1,
    imins&1, jmins&1, imint&1, jmint&1,
    8);
  local_dist += bdist1(
    ref + width + (imins>>1) + (width<<1)*(jmins>>1),
    ref + (iminb>>1) + (width<<1)*(jminb>>1),
    mb + width,
    width<<1,
    imins&1, jmins&1, iminb&1, jminb&1,
    8);

  *dmcp = local_dist;
  *iminp = imins;
  *jminp = jmins;
  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc;
}

static void dpfield_estimate(topref,botref,mb,imin2,j,imins,jmins,
  imindmvp, jmindmvp, dmcp, vmcp)
unsigned char *topref, *botref, *mb;
int imin2,j;
int imins, jmins;
int *imindmvp, *jmindmvp;
int *dmcp,*vmcp;
{
  unsigned char *sameref, *oppref;
  int io0,jo0,io,jo,delta_x,delta_y,mvxs,mvys,mvxo0,mvyo0;
  int imino,jmino,imindmv,jmindmv,vmc_dp,local_dist;



  
  if (pict_struct==TOP_FIELD)
  {
    sameref = topref;    
    oppref = botref;
  }
  else 
  {
    sameref = botref;
    oppref = topref;
  }

 
  mvxs = imins - (imin2<<1);
  mvys = jmins - (j<<1);

 
  mvxo0 = (mvxs+(mvxs>0)) >> 1;  /* mvxs // 2 */
  mvyo0 = (mvys+(mvys>0)) >> 1;  /* mvys // 2 */

 
  if (pict_struct==TOP_FIELD)
    mvyo0--;
  else
    mvyo0++;

  
  io0 = mvxo0 + (imin2<<1);
  jo0 = mvyo0 + (j<<1);

  
  vmc_dp = 1 << 30;

  for (delta_y = -1; delta_y <= 1; delta_y++)
  {
    for (delta_x = -1; delta_x <=1; delta_x++)
    {
      
      io = io0 + delta_x;
      jo = jo0 + delta_y;

      if (io >= 0 && io <= (width-16)<<1 &&
          jo >= 0 && jo <= (height2-16)<<1)
      {
        
        local_dist = bdist2(
          sameref + (imins>>1) + width2*(jmins>>1),
          oppref  + (io>>1)    + width2*(jo>>1),
          mb,             /* current mb location */
          width2,         /* adjacent line distance */
          imins&1, jmins&1, io&1, jo&1, /* half-pel flags */
          16);            /* block height */

        /* update delta with least distortion vector */
        if (local_dist < vmc_dp)
        {
          imino = io;
          jmino = jo;
          imindmv = delta_x;
          jmindmv = delta_y;
          vmc_dp = local_dist;
        }
      }
    }  /* end delta x loop */
  } /* end delta y loop */

  /* Compute L1 error for decision purposes */
  *dmcp = bdist1(
    sameref + (imins>>1) + width2*(jmins>>1),
    oppref  + (imino>>1) + width2*(jmino>>1),
    mb,             
    width2,         
    imins&1, jmins&1, imino&1, jmino&1, 
    16);            

  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc_dp;
}

/*
blk：块的左上角像素坐标 
h：块的高度 
lx：在参考块中，垂直相邻的像素之间的距离（以字节为单位）
org：源参考图像的左上角像素的坐标
ref：重建参考图像的左上角像素的坐标
i0，j0：搜索窗口的中心点
sx，sy：搜索窗口的半长和半宽
xmax，ymax：搜索区域的右边界和下边界
iminp，jminp：指向存储结果的指针
*/

/*
static int fullsearch(org,ref,blk,lx,i0,j0,sx,sy,h,xmax,ymax,iminp,jminp) //梯度下降法搜索
unsigned char *org,*ref,*blk;
int lx,i0,j0,sx,sy,h,xmax,ymax;
int *iminp,*jminp;
{
	int i,j,imin,jmin,ilow,ihigh,jlow,jhigh,icenter,jcenter;
	int d,dmin;
	int count;
    int isquare[8],jsquare[8];
    
    
	ilow = i0 - sx;
	ihigh = i0 + sx;
	
	if (ilow<0)
		ilow = 0;
	
	if (ihigh>xmax-16)
		ihigh = xmax-16;
	
	jlow = j0 - sy;
	jhigh = j0 + sy;
	
	if (jlow<0)
		jlow = 0;
	
	if (jhigh>ymax-h)
    jhigh = ymax-h;
	
//初始化
	imin = i0;
	jmin = j0;
	dmin = dist1(org+imin+lx*jmin,blk,lx,0,0,h,65536);
//step1
	icenter = i0;
	jcenter = j0;
	isquare[0]=icenter-1;jsquare[0]=jcenter-1;
	isquare[1]=icenter-1;jsquare[1]=jcenter;
	isquare[2]=icenter-1;jsquare[2]=jcenter+1;
	isquare[3]=icenter  ;jsquare[3]=jcenter+1;
	isquare[4]=icenter+1;jsquare[4]=jcenter+1;
	isquare[5]=icenter+1;jsquare[5]=jcenter;
	isquare[6]=icenter+1;jsquare[6]=jcenter-1;
    isquare[7]=icenter  ;isquare[7]=jcenter-1;

for(count=0;count<8;count++)
{
	if(isquare[count]>=ilow&&jsquare[count]>=jlow&&isquare[count]<=ihigh&&jsquare[count]<=jhigh)
	{	d = dist1(org + isquare[count] + lx * jsquare[count], blk, lx, 0, 0, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = isquare[count];
		jmin = jsquare[count];
	}
	

	
	}
		else continue;
}

//step2
while((icenter!=imin)&&(jcenter!=jmin))
{
	icenter = imin;
	jcenter = jmin;
   
	isquare[0]=icenter-1;jsquare[0]=jcenter-1;
	isquare[1]=icenter-1;jsquare[1]=jcenter;
	isquare[2]=icenter-1;jsquare[2]=jcenter+1;
	isquare[3]=icenter  ;jsquare[3]=jcenter+1;
	isquare[4]=icenter+1;jsquare[4]=jcenter+1;
	isquare[5]=icenter+1;jsquare[5]=jcenter;
	isquare[6]=icenter+1;jsquare[6]=jcenter-1;
    isquare[7]=icenter  ;isquare[7]=jcenter-1;


	for(count=0;count<8;count++)
	{
		if(isquare[count]>=ilow&&jsquare[count]>=jlow&&isquare[count]<=ihigh&&jsquare[count]<=jhigh)
		{	d = dist1(org + isquare[count] + lx * jsquare[count], blk, lx, 0, 0, h, dmin);
		
		if (d < dmin)
		{
			dmin = d;
			imin = isquare[count];
			jmin = jsquare[count];
		}
		}
		else continue;
		
		
	}
	


}





*/




/*

 static int fullsearch(org,ref,blk,lx,i0,j0,sx,sy,h,xmax,ymax,iminp,jminp)//全搜索
 unsigned char *org,*ref,*blk;
 int lx,i0,j0,sx,sy,h,xmax,ymax;
 int *iminp,*jminp;
{
  int i,j,imin,jmin,ilow,ihigh,jlow,jhigh;
  int d,dmin;
  int k,l,sxy;

  ilow = i0 - sx;
  ihigh = i0 + sx;

  if (ilow<0)
    ilow = 0;

  if (ihigh>xmax-16)
    ihigh = xmax-16;

  jlow = j0 - sy;
  jhigh = j0 + sy;

  if (jlow<0)
    jlow = 0;

  if (jhigh>ymax-h)
    jhigh = ymax-h;
	
  // 全搜索，螺旋向外 

  imin = i0;
  jmin = j0;
  dmin = dist1(org+imin+lx*jmin,blk,lx,0,0,h,65536);

  sxy = (sx>sy) ? sx : sy;

  for (l=1; l<=sxy; l++)
  {
    i = i0 - l;
    j = j0 - l;
    for (k=0; k<8*l; k++)
    {
      if (i>=ilow && i<=ihigh && j>=jlow && j<=jhigh)
      {
        d = dist1(org+i+lx*j,blk,lx,0,0,h,dmin);

        if (d<dmin)
        {
          dmin = d;
          imin = i;
          jmin = j;
        }
      }

      if      (k<2*l) i++;
      else if (k<4*l) j++;
      else if (k<6*l) i--;
      else            j--;
    }
  }
*/
/*
static int fullsearch(org,ref,blk,lx,i0,j0,sx,sy,h,xmax,ymax,iminp,jminp)//菱形搜索
unsigned char *org,*ref,*blk;
int lx,i0,j0,sx,sy,h,xmax,ymax;
int *iminp,*jminp;
{
	int i,j,imin,jmin,ilow,ihigh,jlow,jhigh,icenter,jcenter;
	int d,dmin;
	int count;
    int ilx[8],jlx[8];
    int ismalllx[4],jsmalllx[4];
    
	ilow = i0 - sx;
	ihigh = i0 + sx;
	
	if (ilow<0)
		ilow = 0;
	
	if (ihigh>xmax-16)
		ihigh = xmax-16;
	
	jlow = j0 - sy;
	jhigh = j0 + sy;
	
	if (jlow<0)
		jlow = 0;
	
	if (jhigh>ymax-h)
    jhigh = ymax-h;
	
//初始化
	imin = i0;
	jmin = j0;
	dmin = dist1(org+imin+lx*jmin,blk,lx,0,0,h,65536);
//step1
	icenter = i0;
	jcenter = j0;

ilx[0]=icenter-2;jlx[0]=jcenter;
ilx[1]=icenter-1;jlx[1]=jcenter+1;
ilx[2]=icenter  ;jlx[2]=jcenter+2;
ilx[3]=icenter+1;jlx[3]=jcenter+1;
ilx[4]=icenter+2;jlx[4]=jcenter;
ilx[5]=icenter+1;jlx[5]=jcenter-1;
ilx[6]=icenter  ;jlx[6]=jcenter-2;
ilx[7]=icenter-1;jlx[7]=jcenter-1;


for(count=0;count<8;count++)
{
	if(ilx[count]>=ilow&&jlx[count]>=jlow&&ilx[count]<=ihigh&&jlx[count]<=jhigh)
	{	d = dist1(org + ilx[count] + lx * jlx[count], blk, lx, 0, 0, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = ilx[count];
		jmin = jlx[count];
	}
	

	
	}
		else continue;
}

//step2
while((icenter!=imin)&&(jcenter!=jmin))
{
	icenter = imin;
	jcenter = jmin;
    ilx[0]=icenter-2;jlx[0]=jcenter;
	ilx[1]=icenter-1;jlx[1]=jcenter+1;
	ilx[2]=icenter  ;jlx[2]=jcenter+2;
	ilx[3]=icenter+1;jlx[3]=jcenter+1;
	ilx[4]=icenter+2;jlx[4]=jcenter;
	ilx[5]=icenter+1;jlx[5]=jcenter-1;
	ilx[6]=icenter  ;jlx[6]=jcenter-2;
    ilx[7]=icenter-1;jlx[7]=jcenter-1;




	for(count=0;count<8;count++)
	{
		if(ilx[count]>=ilow&&jlx[count]>=jlow&&ilx[count]<=ihigh&&jlx[count]<=jhigh)
		{	d = dist1(org + ilx[count] + lx * jlx[count], blk, lx, 0, 0, h, dmin);
		
		if (d < dmin)
		{
			dmin = d;
			imin = ilx[count];
			jmin = jlx[count];
		}
		}
		else continue;
		
		
	}
	


}

//step3
ismalllx[0]=icenter-1;jsmalllx[0]=jcenter;
ismalllx[1]=icenter  ;jsmalllx[1]=jcenter+1;
ismalllx[0]=icenter+1;jsmalllx[0]=jcenter;
ismalllx[0]=icenter  ;jsmalllx[0]=jcenter-1;


for(count=0;count<4;count++)
{
	if(ismalllx[count]>=ilow&&jsmalllx[count]>=jlow&&ismalllx[count]<=ihigh&&jsmalllx[count]<=jhigh)
	{	d = dist1(org + ismalllx[count] + lx * jsmalllx[count], blk, lx, 0, 0, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = ismalllx[count];
		jmin = jsmalllx[count];
	}
	}
	else continue;
	
	
}*/


static int fullsearch(org,ref,blk,lx,i0,j0,sx,sy,h,xmax,ymax,iminp,jminp)//四步法搜索
unsigned char *org,*ref,*blk;
int lx,i0,j0,sx,sy,h,xmax,ymax;
int *iminp,*jminp;
{
	int i,j,imin,jmin,ilow,ihigh,jlow,jhigh,icenter,jcenter;
	int d,dmin;
	int count;
    int ibigsquare[8],jbigsquare[8];
    int ismallsquare[8],jsmallsquare[8];
    
	ilow = i0 - sx;
	ihigh = i0 + sx;
	
	if (ilow<0)
		ilow = 0;
	
	if (ihigh>xmax-16)
		ihigh = xmax-16;
	
	jlow = j0 - sy;
	jhigh = j0 + sy;
	
	if (jlow<0)
		jlow = 0;
	
	if (jhigh>ymax-h)
    jhigh = ymax-h;
	
//初始化
	imin = i0;
	jmin = j0;
	dmin = dist1(org+imin+lx*jmin,blk,lx,0,0,h,65536);
//step1
	icenter = i0;
	jcenter = j0;

ibigsquare[0]=icenter-2;jbigsquare[0]=jcenter-2;
ibigsquare[1]=icenter-2;jbigsquare[1]=jcenter;
ibigsquare[2]=icenter-2;jbigsquare[2]=jcenter+2;
ibigsquare[3]=icenter  ;jbigsquare[3]=jcenter+2;
ibigsquare[4]=icenter+2;jbigsquare[4]=jcenter+2;
ibigsquare[5]=icenter+2;jbigsquare[5]=jcenter;
ibigsquare[6]=icenter+2;jbigsquare[6]=jcenter-2;
ibigsquare[7]=icenter  ;jbigsquare[7]=jcenter-2;

for(count=0;count<8;count++)
{
	if(ibigsquare[count]>=ilow&&jbigsquare[count]>=jlow&&ibigsquare[count]<=ihigh&&jbigsquare[count]<=jhigh)
	{	d = dist1(org + ibigsquare[count] + lx * jbigsquare[count], blk, lx, 0, 0, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = ibigsquare[count];
		jmin = jbigsquare[count];
	}
	

	
	}
		else continue;
}

//step2
while((icenter!=imin)&&(jcenter!=jmin))
{
	icenter = imin;
	jcenter = jmin;
	ibigsquare[0]=icenter-2;jbigsquare[0]=jcenter-2;
	ibigsquare[1]=icenter-2;jbigsquare[1]=jcenter;
	ibigsquare[2]=icenter-2;jbigsquare[2]=jcenter+2;
	ibigsquare[3]=icenter  ;jbigsquare[3]=jcenter+2;
	ibigsquare[4]=icenter+2;jbigsquare[4]=jcenter+2;
	ibigsquare[5]=icenter+2;jbigsquare[5]=jcenter;
	ibigsquare[6]=icenter+2;jbigsquare[6]=jcenter-2;
    ibigsquare[7]=icenter  ;jbigsquare[7]=jcenter-2;

	for(count=0;count<8;count++)
	{
		if(ibigsquare[count]>=ilow&&jbigsquare[count]>=jlow&&ibigsquare[count]<=ihigh&&jbigsquare[count]<=jhigh)
		{	d = dist1(org + ibigsquare[count] + lx * jbigsquare[count], blk, lx, 0, 0, h, dmin);
		
		if (d < dmin)
		{
			dmin = d;
			imin = ibigsquare[count];
			jmin = jbigsquare[count];
		}
		}
		else continue;
		
		
	}
	


}

//step3
ismallsquare[0]=icenter-1;jsmallsquare[0]=jcenter-1;
ismallsquare[1]=icenter-1;jsmallsquare[1]=jcenter;
ismallsquare[2]=icenter-1;jsmallsquare[2]=jcenter+1;
ismallsquare[3]=icenter  ;jsmallsquare[3]=jcenter+1;
ismallsquare[4]=icenter+1;jsmallsquare[4]=jcenter+1;
ismallsquare[5]=icenter+1;jsmallsquare[5]=jcenter;
ismallsquare[6]=icenter+1;jsmallsquare[6]=jcenter-1;
ismallsquare[7]=icenter  ;ismallsquare[7]=jcenter-1;
for(count=0;count<8;count++)
{
	if(ismallsquare[count]>=ilow&&jsmallsquare[count]>=jlow&&ismallsquare[count]<=ihigh&&jsmallsquare[count]<=jhigh)
	{	d = dist1(org + ismallsquare[count] + lx * jsmallsquare[count], blk, lx, 0, 0, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = ismallsquare[count];
		jmin = jsmallsquare[count];
	}
	}
	else continue;
	
	
}


//六边形搜索
/*
static int fullsearch(org, ref, blk, lx, i0, j0, sx, sy, h, xmax, ymax, iminp, jminp)
unsigned char *org, *ref, *blk;
int lx, i0, j0, sx, sy, h, xmax, ymax;
int *iminp,*jminp;
{
	int i, j, i_center, j_center, imin, jmin, ilow, ihigh, jlow, jhigh;
	int i_largehex[6], j_largehex[6], i_smallhex[4], j_smallhex[4];
	int d, dmin;
	int cnt;
	
	ilow = i0 - sx;
	ihigh = i0 + sx;
	
	if (ilow < 0)
		ilow = 0;
	
	if (ihigh > xmax - 16)
		ihigh = xmax - 16;
	
	jlow = j0 - sy;
	jhigh = j0 + sy;
	
	if (jlow < 0)
		jlow = 0;
	
	if (jhigh > ymax - h)
		jhigh = ymax - h;
	
// 初始化 imin, jmin ,dmin	
	imin = i0;
	jmin = j0;
	dmin = dist1(org + imin + lx * jmin, blk, lx, 0, 0, h, 65536);

//step1
	i_center = i0;
	j_center = j0;
	i_largehex[0] = i_center - 2; j_largehex[0] = j_center;
	i_largehex[1] = i_center - 1; j_largehex[1] = j_center - 2;
	i_largehex[2] = i_center + 1; j_largehex[2] = j_center - 2;
	i_largehex[3] = i_center + 2; j_largehex[3] = j_center;
	i_largehex[4] = i_center + 1; j_largehex[4] = j_center + 2;
	i_largehex[5] = i_center - 1; j_largehex[5] = j_center + 2;

	for (cnt = 0; cnt < 6; cnt++)
	{
		if (i_largehex[cnt] >= ilow && i_largehex[cnt] <= ihigh && j_largehex[cnt] >= jlow && j_largehex[cnt] <= jhigh)
		{			
			d = dist1(org + i_largehex[cnt] + lx * j_largehex[cnt], blk, lx, 0, 0, h, dmin);
			
			if (d < dmin)
			{
				dmin = d;
				imin = i_largehex[cnt];
				jmin = j_largehex[cnt];
			}
		}
		else continue;
	}

//step2
	while ((i_center != imin) && (j_center != jmin))
	{
		i_center = imin;
		j_center = jmin;
		i_largehex[0] = i_center - 2; j_largehex[0] = j_center;
		i_largehex[1] = i_center - 1; j_largehex[1] = j_center - 2;
		i_largehex[2] = i_center + 1; j_largehex[2] = j_center - 2;
		i_largehex[3] = i_center + 2; j_largehex[3] = j_center;
		i_largehex[4] = i_center + 1; j_largehex[4] = j_center + 2;
		i_largehex[5] = i_center - 1; j_largehex[5] = j_center + 2;

		for (cnt = 0; cnt < 6; cnt++)
		{			
			if (i_largehex[cnt] >= ilow && i_largehex[cnt] <= ihigh && j_largehex[cnt] >= jlow && j_largehex[cnt] <= jhigh)
			{			
				d = dist1(org + i_largehex[cnt] + lx * j_largehex[cnt], blk, lx, 0, 0, h, dmin);
				
				if (d < dmin)
				{
					dmin = d;
					imin = i_largehex[cnt];
					jmin = j_largehex[cnt];
				}
			}
			else continue;
		}
		
	}

//step3
	i_smallhex[0] = i_center - 1;	j_smallhex[0] = j_center;
	i_smallhex[1] = i_center;		j_smallhex[1] = j_center - 1;
	i_smallhex[2] = i_center + 1;	j_smallhex[2] = j_center;
	i_smallhex[3] = i_center;		j_smallhex[3] = j_center + 1;

	for (cnt = 0; cnt < 4; cnt++)
	{
		if (i_smallhex[cnt] >= ilow && i_smallhex[cnt] <= ihigh && j_smallhex[cnt] >= jlow && j_smallhex[cnt] <= jhigh)
		{		
			d = dist1(org + i_smallhex[cnt] + lx * j_smallhex[cnt], blk, lx, 0, 0, h, dmin);
			
			if (d < dmin)
			{
				dmin = d;
				imin = i_smallhex[cnt];
				jmin = j_smallhex[cnt];
			}
		}
		else continue;
	}
*/
	/* half pel */

dmin = 65536;
imin <<= 1;
jmin <<= 1;
ilow = imin - (imin > 0);
ihigh = imin + (imin < ((xmax - 16) << 1));
jlow = jmin - (jmin > 0);
jhigh = jmin + (jmin < ((ymax - h) << 1));

for (j = jlow; j <= jhigh; j++)
for (i = ilow; i <= ihigh; i++)
{
	d = dist1(ref + (i >> 1) + lx * (j >> 1), blk, lx, i & 1, j & 1, h, dmin);
	
	if (d < dmin)
	{
		dmin = d;
		imin = i;
		jmin = j;
	}
}

*iminp = imin;
*jminp = jmin;

return dmin;
}


#pragma warning( disable : 4799 )

/*
 * 求两个块之间绝对差值
 * blk1,blk2：为两个块左上角像素坐标
 *lx：垂直相邻的像素之间的距离（以字节为单位）
 *hx，hy：水平和垂直插值的标志
 *h：块的高度（通常为8或16）
 *distlim：极值，如果结果超过该值则放弃之。
 */
static int dist1(blk1,blk2,lx,hx,hy,h,distlim)
unsigned char *blk1,*blk2;
int lx,hx,hy,h;
int distlim;
{
	unsigned char *p1,*p1a,*p2;
	int i,j;
	int s,v;

	if(fastMotionCompensationLevel)
	{
		lx <<= fastMotionCompensationLevel;
		h >>= fastMotionCompensationLevel;
	}

	if (!hx && !hy)
	{
		//针对MMX处理器
		if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1		;// esi = blk1
				mov edi, blk2		;// edi = blk2
				mov ecx, h			;// ecx = h
				mov edx, lx			;// edx = lx
				mov eax, 0			;// eax = s
dist1__l1:
				movq mm0, [esi]		;// load esi[0..7] into mm0
				movq mm1, [edi]		;// load edi[0..7] into mm1
				movq mm2, [esi+8]	;// load esi[8..15] into mm2
				movq mm3, [edi+8]	;// load edi[8..15] into mm3
				movq mm4, mm0
				movq mm5, mm1
				movq mm6, mm2
				movq mm7, mm3
				psubusb mm0, mm1
				psubusb mm2, mm3
				psubusb mm5, mm4
				psubusb mm7, mm6
				pxor mm1, mm1		;// mm1 = 0
				por mm0, mm5		;// mm0 = abs(esi[0..7] - edi[0..7])
				por mm2, mm7		;// mm2 = abs(esi[8..15] - edi[8..15])
				movq mm4, mm0
				movq mm6, mm2
				punpcklbw mm0, mm1	;// unpack the lower 4 bytes into mm0
				punpcklbw mm2, mm1	;// unpack the lower 4 bytes into mm2
				punpckhbw mm4, mm1	;// unpack the upper 4 bytes into mm4
				punpckhbw mm6, mm1	;// unpack the upper 4 bytes into mm6
				paddw mm0, mm2
				paddw mm4, mm6
				paddw mm0, mm4		;// mm0 += mm2 + mm4 + mm6
				movq mm5, mm0
				punpcklwd mm0, mm1	;// unpack the lower 2 words into mm0
				punpckhwd mm5, mm1	;// unpack the upper 2 words into mm5
				paddd mm0, mm5		;// mm0 += mm5
				movd ebx, mm0		;// load lower dword of mm0 into ebx
				add eax, ebx		;// eax += ebx
				psrlq mm0, 32		;// shift mm0 to get upper dword
				movd ebx, mm0		;// load lower dword of mm0 into ebx
				add eax, ebx		;// eax += ebx

				cmp eax, distlim	;// compare eax with distlim
				jge dist1__l2		;// terminate if eax >= distlim
				add esi, edx		;// esi += edx
				add edi, edx		;// edi += edx
				
				dec ecx				;// decrement ecx
				jnz dist1__l1		;// loop while not zero
dist1__l2:
				mov s, eax			;// s = eax
				emms				;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;

			for (j=0; j<h; j++)
			{
				if ((v = p1[0]  - p2[0])<0)  v = -v; s+= v;
				if ((v = p1[1]  - p2[1])<0)  v = -v; s+= v;
				if ((v = p1[2]  - p2[2])<0)  v = -v; s+= v;
				if ((v = p1[3]  - p2[3])<0)  v = -v; s+= v;
				if ((v = p1[4]  - p2[4])<0)  v = -v; s+= v;
				if ((v = p1[5]  - p2[5])<0)  v = -v; s+= v;
				if ((v = p1[6]  - p2[6])<0)  v = -v; s+= v;
				if ((v = p1[7]  - p2[7])<0)  v = -v; s+= v;
				if ((v = p1[8]  - p2[8])<0)  v = -v; s+= v;
				if ((v = p1[9]  - p2[9])<0)  v = -v; s+= v;
				if ((v = p1[10] - p2[10])<0) v = -v; s+= v;
				if ((v = p1[11] - p2[11])<0) v = -v; s+= v;
				if ((v = p1[12] - p2[12])<0) v = -v; s+= v;
				if ((v = p1[13] - p2[13])<0) v = -v; s+= v;
				if ((v = p1[14] - p2[14])<0) v = -v; s+= v;
				if ((v = p1[15] - p2[15])<0) v = -v; s+= v;

				if (s >= distlim)
					break;

				p1+= lx;
				p2+= lx;
			}
		}
	}
	else if (hx && !hy)
	{
		//针对支持3DNow的CPU
		if (cpu_3DNow)
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;

			for (j=0; j<h; j++)
			{
				

				#define femmsInst __asm _emit 0x0F __asm _emit 0x0E
				#define pavgusbmm0mm2 __asm _emit 0x0F __asm _emit 0x0F __asm _emit 0xC2 __asm _emit 0xBF
				#define pavgusbmm1mm3 __asm _emit 0x0F __asm _emit 0x0F __asm _emit 0xCB __asm _emit 0xBF

				__asm
				{
					
					femmsInst
					mov              eax, p1
					movq             mm0,[eax]
					movq             mm1,[eax+8]
					movq             mm2,[eax+1]
					movq             mm3,[eax+9]
					pavgusbmm0mm2
					movq             tmpResult,mm0
					pavgusbmm1mm3
					movq             tmpResult+8,mm1
					femmsInst
				}
				s += calcDist16(tmpResult,p2);

				p1+= lx;
				p2+= lx;
			}
		}
		else if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1			;// esi = blk1
				mov edi, blk2			;// edi = blk2
				mov ecx, h				;// ecx = h
				mov edx, lx				;// edx = lx
				mov eax, 0				;// eax = s
				pxor mm7, mm7			;// mm7 = 0
dist1__l3:
				movq mm5, PACKED_1		;// mm5 = (1, 1, 1, 1)
				movd mm0, [esi]			;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+1]		;// lower 4 bytes in mm1 = esi[1..4]
				movd mm2, [esi+4]		;// lower 4 bytes in mm2 = esi[4..7]
				movd mm3, [esi+5]		;// lower 4 bytes in mm3 = esi[5..8]
				punpcklbw mm0, mm7		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm0, mm1			;// mm0 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				paddw mm0, mm5			;// mm0 += (1,1,1,1)
				paddw mm2, mm5			;// mm2 += (1,1,1,1)
				psrlw mm0, 1			;// mm0 >>= 1
				psrlw mm2, 1			;// mm2 >>= 1

				movd mm4, [edi]			;// lower 4 bytes in mm4 = edi[0..3]
				movd mm6, [edi+4]		;// lower 4 bytes in mm6 = edi[0..3]
				punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				movq mm1, mm0
				movq mm3, mm2
				psubusw mm0, mm4
				psubusw mm2, mm6
				psubusw mm4, mm1
				psubusw mm6, mm3
				por mm0, mm4			;// mm0 = abs((esi[0..3] + esi[1..4]) >> 1) - edi[0..3])
				por mm2, mm6			;// mm2 = abs((esi[4..7] + esi[5..8]) >> 1) - edi[4..7])

				movq mm4, mm0
				movq mm6, mm2
				punpcklwd mm0, mm7		;// unpack the lower 4 word into mm0
				punpckhwd mm4, mm7		;// unpack the lower 4 word into mm4
				punpcklwd mm2, mm7		;// unpack the lower 4 word into mm2
				punpckhwd mm6, mm7		;// unpack the lower 4 word into mm6
				paddd mm0, mm2
				paddd mm4, mm6
				paddd mm0, mm4			;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx
				psrlq mm0, 32			;// shift mm0 to get upper dword
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx

				movd mm0, [esi+8]		;// lower 4 bytes in mm0 = esi[8..11]
				movd mm1, [esi+9]		;// lower 4 bytes in mm1 = esi[9..12]
				movd mm2, [esi+12]		;// lower 4 bytes in mm2 = esi[12..15]
				movd mm3, [esi+13]		;// lower 4 bytes in mm3 = esi[13..16]
				punpcklbw mm0, mm7		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm0, mm1			;// mm0 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				paddw mm0, mm5			;// mm0 += (1,1,1,1)
				paddw mm2, mm5			;// mm2 += (1,1,1,1)
				psrlw mm0, 1			;// mm0 >>= 1
				psrlw mm2, 1			;// mm2 >>= 1

				movd mm4, [edi+8]		;// lower 4 bytes in mm4 = edi[8..11]
				movd mm6, [edi+12]		;// lower 4 bytes in mm6 = edi[12..15]
				punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				movq mm1, mm0
				movq mm3, mm2
				psubusw mm0, mm4
				psubusw mm2, mm6
				psubusw mm4, mm1
				psubusw mm6, mm3
				por mm0, mm4			;// mm0 = abs((esi[8..11] + esi[9..12]) >> 1) - edi[8..11])
				por mm2, mm6			;// mm2 = abs((esi[12..15] + esi[13..16]) >> 1) - edi[12..15])

				movq mm4, mm0
				movq mm6, mm2
				punpcklwd mm0, mm7		;// unpack the lower 4 word into mm0
				punpckhwd mm4, mm7		;// unpack the lower 4 word into mm4
				punpcklwd mm2, mm7		;// unpack the lower 4 word into mm2
				punpckhwd mm6, mm7		;// unpack the lower 4 word into mm6
				paddd mm0, mm2
				paddd mm4, mm6
				paddd mm0, mm4			;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx
				psrlq mm0, 32			;// shift mm0 to get upper dword
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx

				add esi, edx			;// esi += edx
				add edi, edx			;// edi += edx
				dec ecx					;// decrement ecx
				jnz dist1__l3			;// loop while not zero
				mov s, eax				;// s = eax
				emms					;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;

			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1[i+1]+1)>>1) - p2[i];

					if (v>=0)
						s+= v;
					else
						s-= v;
				}
				p1+= lx;
				p2+= lx;
			}
		}
	}
	else if (!hx && hy)
	{
		if(cpu_3DNow)
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			p1a = p1 + lx;
			for (j=0; j<h; j++)
			{
				__asm
				{
				// femms
				femmsInst
				mov              eax, p1
				movq             mm0,[eax]
				movq             mm1,[eax+8]
				mov              eax,p1a
				movq             mm2,[eax]
				movq             mm3,[eax+8]
				pavgusbmm0mm2
				movq             tmpResult,mm0
				pavgusbmm1mm3
				movq             tmpResult+8,mm1
				femmsInst
				}
				s += calcDist16(tmpResult,p2);
				p1 = p1a;
				p1a+= lx;
				p2+= lx;
			}
		}
		else if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1			;// esi = blk1
				mov edi, blk2			;// edi = blk2
				mov edx, lx				;// edx = lx
				mov ecx, h				;// ecx = h
				mov eax, 0				;// eax = s
				pxor mm7, mm7			;// mm7 = 0
dist1__l4:
				movq mm5, PACKED_1		;// mm5 = (1, 1, 1, 1)
				movd mm0, [esi]			;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+edx]		;// lower 4 bytes in mm1 = (esi + edx)[0..3]
				movd mm2, [esi+4]		;// lower 4 bytes in mm2 = esi[4..7]
				movd mm3, [esi+edx+4]	;// lower 4 bytes in mm3 = (esi + edx)[4..7]
				punpcklbw mm0, mm7		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm0, mm1			;// mm0 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				paddw mm0, mm5			;// mm0 += (1, 1, 1, 1)
				paddw mm2, mm5			;// mm2 += (1, 1, 1, 1)
				psrlw mm0, 1			;// mm0 >>= 1
				psrlw mm2, 1			;// mm0 >>= 1

				movd mm4, [edi]			;// lower 4 bytes in mm4 = edi[0..3]
				movd mm6, [edi+4]		;// lower 4 bytes in mm6 = edi[4..7]
				punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				movq mm1, mm0
				movq mm3, mm2
				psubusw mm0, mm4
				psubusw mm2, mm6
				psubusw mm4, mm1
				psubusw mm6, mm3
				por mm0, mm4			;// mm0 = abs((esi[0..3] + (esi + edx)[0..3]) >> 1) - edi[0..3])
				por mm2, mm6			;// mm2 = abs((esi[4..7] + (esi + edx)[4..7]) >> 1) - edi[4..7])

				movq mm4, mm0
				movq mm6, mm2
				punpcklwd mm0, mm7		;// unpack lower 4 words into mm0
				punpckhwd mm4, mm7		;// unpack upper 4 words into mm4
				punpcklwd mm2, mm7		;// unpack lower 4 words into mm2
				punpckhwd mm6, mm7		;// unpack upper 4 words into mm6
				paddd mm0, mm2			
				paddd mm4, mm6			
				paddd mm0, mm4			;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx
				psrlq mm0, 32			;// shift mm0 to get upper dword
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx

				movd mm0, [esi+8]		;// lower 4 bytes in mm0 = esi[8..11]
				movd mm1, [esi+edx+8]	;// lower 4 bytes in mm1 = (esi + edx)[8..11]
				movd mm2, [esi+12]		;// lower 4 bytes in mm2 = esi[12..15]
				movd mm3, [esi+edx+12]	;// lower 4 bytes in mm3 = (esi + edx)[12..15]
				punpcklbw mm0, mm7		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, mm7		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, mm7		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, mm7		;// unpack the lower 4 bytes into mm3
				paddw mm0, mm1			;// mm0 += mm1
				paddw mm2, mm3			;// mm2 += mm3
				paddw mm0, mm5			;// mm0 += (1, 1, 1, 1)
				paddw mm2, mm5			;// mm2 += (1, 1, 1, 1)
				psrlw mm0, 1			;// mm0 >>= 1
				psrlw mm2, 1			;// mm0 >>= 1

				movd mm4, [edi+8]		;// lower 4 bytes in mm4 = edi[8..11]
				movd mm6, [edi+12]		;// lower 4 bytes in mm6 = edi[12..15]
				punpcklbw mm4, mm7		;// unpack the lower 4 bytes into mm4
				punpcklbw mm6, mm7		;// unpack the lower 4 bytes into mm6
				movq mm1, mm0
				movq mm3, mm2
				psubusw mm0, mm4
				psubusw mm2, mm6
				psubusw mm4, mm1
				psubusw mm6, mm3
				por mm0, mm4			;// mm0 = abs((esi[8..11] + (esi + edx)[8..11]) >> 1) - edi[8..11])
				por mm2, mm6			;// mm2 = abs((esi[12..15] + (esi + edx)[12..15]) >> 1) - edi[12..15])

				movq mm4, mm0
				movq mm6, mm2
				punpcklwd mm0, mm7		;// unpack the lower 2 words into mm0
				punpckhwd mm4, mm7		;// unpack the upper 2 words into mm4
				punpcklwd mm2, mm7		;// unpack the lower 2 words into mm2
				punpckhwd mm6, mm7		;// unpack the upper 2 words into mm6
				paddd mm0, mm2
				paddd mm4, mm6
				paddd mm0, mm4			;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx
				psrlq mm0, 32			;// shift mm0 to get upper dword
				movd ebx, mm0			;// load lower dword of mm0 into ebx
				add eax, ebx			;// eax += ebx

				add esi, edx			;// esi += edx
				add edi, edx			;// edi += edx

				dec ecx					;// decrement ecx
				jnz dist1__l4			;// loop while not zero
				mov s, eax				;// s = eax
				emms					;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			p1a = p1 + lx;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1a[i]+1)>>1) - p2[i];

					if (v>=0)
						s+= v;
					else
						s-= v;
				}
				p1 = p1a;
				p1a+= lx;
				p2+= lx;
			}
		}
	}
	else 
	{
		if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1				;// esi = blk1
				mov edi, blk2				;// edi = blk2
				mov ecx, h					;// ecx = h
				mov edx, lx					;// edx = lx
				mov eax, 0					;// eax = s
dist1__l5:
				movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+1]			;// lower 4 bytes in mm1 = esi[1..4]
				movd mm2, [esi+edx]			;// lower 4 bytes in mm2 = (esi + edx)[0..3]
				movd mm3, [esi+edx+1]		;// lower 4 bytes in mm3 = (esi + edx)[1..4]
				movd mm4, [esi+4]			;// lower 4 bytes in mm4 = esi[4..7]
				movd mm5, [esi+5]			;// lower 4 bytes in mm5 = esi[5..8]
				movd mm6, [esi+edx+4]		;// lower 4 bytes in mm6 = (esi + edx)[4..7]
				movd mm7, [esi+edx+5]		;// lower 4 bytes in mm7 = (esi + edx)[5..8]
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

				movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
				movd mm5, [edi+4]			;// lower 4 bytes in mm5 = edi[4..7]
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				movq mm3, mm0
				movq mm7, mm4
				psubusw mm0, mm1
				psubusw mm4, mm5
				psubusw mm1, mm3
				psubusw mm5, mm7
				por mm0, mm1				;// mm0 = abs((esi[0..3] + esi[1..4] + (esi + edx)[0..3] + (esi + edx)[1..4]) >> 2) - edi[0..3])
				por mm4, mm5				;// mm0 = abs((esi[4..7] + esi[5..8] + (esi + edx)[4..7] + (esi + edx)[5..8]) >> 2) - edi[4..7])

				movq mm2, mm0
				movq mm6, mm4
				punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
				punpckhwd mm2, PACKED_0		;// unpack the upper 2 words into mm2
				punpcklwd mm4, PACKED_0		;// unpack the lower 2 words into mm4
				punpckhwd mm6, PACKED_0		;// unpack the upper 2 words into mm6
				paddd mm0, mm2
				paddd mm4, mm6
				paddd mm0, mm4				;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				movd mm0, [esi+8]			;// lower 4 bytes in mm0 = esi[8..11]
				movd mm1, [esi+9]			;// lower 4 bytes in mm1 = esi[9..12]
				movd mm2, [esi+edx+8]		;// lower 4 bytes in mm2 = (esi + edx)[8..11]
				movd mm3, [esi+edx+9]		;// lower 4 bytes in mm3 = (esi + edx)[9..12]
				movd mm4, [esi+12]			;// lower 4 bytes in mm4 = esi[12..15]
				movd mm5, [esi+13]			;// lower 4 bytes in mm5 = esi[13..16]
				movd mm6, [esi+edx+12]		;// lower 4 bytes in mm6 = (esi + edx)[12..15]
				movd mm7, [esi+edx+13]		;// lower 4 bytes in mm7 = (esi + edx)[13..16]
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

				movd mm1, [edi+8]			;// lower 4 bytes in mm1 = edi[8..11]
				movd mm5, [edi+12]			;// lower 4 bytes in mm5 = edi[12..15]
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				movq mm3, mm0
				movq mm7, mm4
				psubusw mm0, mm1
				psubusw mm4, mm5
				psubusw mm1, mm3
				psubusw mm5, mm7
				por mm0, mm1				;// mm0 = abs((esi[8..11] + esi[9..12] + (esi + edx)[8..11] + (esi + edx)[9..12]) >> 2) - edi[8..11])
				por mm4, mm5				;// mm0 = abs((esi[12..15] + esi[13..16] + (esi + edx)[12..15] + (esi + edx)[13..16]) >> 2) - edi[12..15])

				movq mm2, mm0
				movq mm6, mm4
				punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
				punpckhwd mm2, PACKED_0		;// unpack the upper 2 words into mm2
				punpcklwd mm4, PACKED_0		;// unpack the lower 2 words into mm4
				punpckhwd mm6, PACKED_0		;// unpack the upper 2 words into mm6
				paddd mm0, mm2
				paddd mm4, mm6
				paddd mm0, mm4				;// mm0 += mm2 + mm4 + mm6
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				add esi, edx				;// esi += edx
				add edi, edx				;// edi += edx
				dec ecx						;// decrement ecx
				jnz dist1__l5				;// loop while not zero
				mov s, eax					;// s = eax
				emms						;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			p1a = p1 + lx;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1[i+1]+p1a[i]+p1a[i+1]+2)>>2) - p2[i];

					if (v>=0)
						s+= v;
					else
						s-= v;
				}
				p1 = p1a;
				p1a+= lx;
				p2+= lx;
			}
		}
	}

	return s;
}


/*
 * 两个块之间均方误差
 
 */
static int dist2(blk1,blk2,lx,hx,hy,h)
unsigned char *blk1,*blk2;
int lx,hx,hy,h;
{
	unsigned char *p1,*p1a,*p2;
	int i,j;
	int s,v;

	if(fastMotionCompensationLevel)
	{
		lx <<= fastMotionCompensationLevel;
		h >>= fastMotionCompensationLevel;
	}

	if (!hx && !hy)
	{
		if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1				;// esi = blk1
				mov edi, blk2				;// edi = blk2
				mov ecx, h					;// ecx = h
				mov edx, lx					;// edx = lx
				mov eax, 0					;// eax = s
dist2__l1:
				movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
				movd mm2, [esi+4]			;// lower 4 bytes in mm2 = esi[4..7]
				movd mm3, [edi+4]			;// lower 4 bytes in mm3 = edi[4..7]
				movd mm4, [esi+8]			;// lower 4 bytes in mm4 = esi[8..11]
				movd mm5, [edi+8]			;// lower 4 bytes in mm5 = edi[8..11]
				movd mm6, [esi+12]			;// lower 4 bytes in mm6 = esi[12..15]
				movd mm7, [edi+12]			;// lower 4 bytes in mm7 = edi[12..15]
				punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
				punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
				punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
				psubw mm0, mm1				;// mm0 -= mm1
				psubw mm2, mm3				;// mm2 -= mm3
				psubw mm4, mm5				;// mm4 -= mm5
				psubw mm6, mm7				;// mm6 -= mm7
				pmullw mm0, mm0				;// mm0 = sqr(mm0)
				pmullw mm2, mm2				;// mm2 = sqr(mm2)
				pmullw mm4, mm4				;// mm4 = sqr(mm4)
				pmullw mm6, mm6				;// mm6 = sqr(mm6)
				paddw mm0, mm2
				paddw mm4, mm6
				paddw mm0, mm4				;// mm0 += mm2 + mm4 + mm6
				movq mm1, mm0
				punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
				punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
				paddd mm0, mm1				;// mm0 += mm1
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				add esi, edx				;// esi += edx
				add edi, edx				;// edi += edx
				dec ecx						;// decrement ecx
				jnz dist2__l1				;// loop while not zero
				mov s, eax					;// s = eax
				emms						;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = p1[i] - p2[i];
					s+= v*v;
				}
				p1+= lx;
				p2+= lx;
			}
		}
	}
	else if (hx && !hy)
	{
		if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1				;// esi = blk1
				mov edi, blk2				;// edi = blk2
				mov edx, lx					;// edx = lx
				mov eax, 0					;// eax = s
				mov ecx, h					;// ecx = h
dist2__l2:
				movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+1]			;// lower 4 bytes in mm1 = esi[1..4]
				movd mm2, [esi+4]			;// lower 4 bytes in mm2 = esi[4..7]
				movd mm3, [esi+5]			;// lower 4 bytes in mm3 = esi[5..8]
				movd mm4, [esi+8]			;// lower 4 bytes in mm4 = esi[8..11]
				movd mm5, [esi+9]			;// lower 4 bytes in mm5 = esi[9..12]
				movd mm6, [esi+12]			;// lower 4 bytes in mm6 = esi[12..15]
				movd mm7, [esi+13]			;// lower 4 bytes in mm7 = esi[13..16]
				punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
				punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
				punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
				paddw mm0, mm1				;// mm0 += mm1
				paddw mm2, mm3				;// mm2 += mm3
				paddw mm4, mm5				;// mm4 += mm5
				paddw mm6, mm7				;// mm6 += mm7
				paddw mm0, PACKED_1			;// mm0 += (1, 1, 1, 1)
				paddw mm2, PACKED_1			;// mm2 += (1, 1, 1, 1)
				paddw mm4, PACKED_1			;// mm4 += (1, 1, 1, 1)
				paddw mm6, PACKED_1			;// mm6 += (1, 1, 1, 1)
				psrlw mm0, 1				;// mm0 >>= 1
				psrlw mm2, 1				;// mm2 >>= 1
				psrlw mm4, 1				;// mm4 >>= 1
				psrlw mm6, 1				;// mm6 >>= 1
				movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
				movd mm3, [edi+4]			;// lower 4 bytes in mm3 = edi[4..7]
				movd mm5, [edi+8]			;// lower 4 bytes in mm5 = edi[8..11]
				movd mm7, [edi+12]			;// lower 4 bytes in mm7 = edi[12..15]
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
				psubw mm0, mm1				;// mm0 -= mm1
				psubw mm2, mm3				;// mm2 -= mm3
				psubw mm4, mm5				;// mm4 -= mm5
				psubw mm6, mm7				;// mm6 -= mm7
				pmullw mm0, mm0				;// mm0 = sqr(mm0)
				pmullw mm2, mm2				;// mm2 = sqr(mm2)
				pmullw mm4, mm4				;// mm4 = sqr(mm4)
				pmullw mm6, mm6				;// mm6 = sqr(mm6)
				paddw mm0, mm2
				paddw mm4, mm6
				paddw mm0, mm4				;// mm0 += mm2 + mm4 + mm6
				movq mm1, mm0
				punpcklwd mm0, PACKED_0		;// unpack the 4 lower words into mm0
				punpckhwd mm1, PACKED_0		;// unpack the 4 lower words into mm1
				paddd mm0, mm1				;// mm0 += mm1
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				add esi, edx				;// esi += edx
				add edi, edx				;// edi += edx
				dec ecx						;// decrement ecx
				jnz dist2__l2				;// loop while not zero
				mov s, eax					;// s = eax
				emms						;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1[i+1]+1)>>1) - p2[i];
					s+= v*v;
				}
				p1+= lx;
				p2+= lx;
			}
		}
	}
	else if (!hx && hy)
	{
		if(cpu_MMX)
		{
			// I do loop unrolling on the inner loop !
			_asm
			{
				mov esi, blk1				;// esi = blk1
				mov edi, blk2				;// edi = blk2
				mov ecx, h					;// ecx = h
				mov edx, lx					;// edx = lx
				mov eax, 0					;// eax = s
dist2__l3:
				movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+edx]			;// lower 4 bytes in mm1 = (esi + edx)[0..3]
				movd mm2, [esi+4]			;// lower 4 bytes in mm2 = esi[4..7]
				movd mm3, [esi+edx+4]		;// lower 4 bytes in mm3 = (esi + edx)[4..7]
				movd mm4, [esi+8]			;// lower 4 bytes in mm4 = esi[8..11]
				movd mm5, [esi+edx+8]		;// lower 4 bytes in mm5 = (esi + edx)[8..11]
				movd mm6, [esi+12]			;// lower 4 bytes in mm6 = esi[12..15]
				movd mm7, [esi+edx+12]		;// lower 4 bytes in mm7 = (esi + edx)[12..15]
				punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
				punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
				punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
				punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
				paddw mm0, mm1				;// mm0 += mm1
				paddw mm2, mm3				;// mm2 += mm3
				paddw mm4, mm5				;// mm4 += mm5
				paddw mm6, mm7				;// mm6 += mm7
				paddw mm0, PACKED_1			;// mm0 += (1, 1, 1, 1)
				paddw mm2, PACKED_1			;// mm2 += (1, 1, 1, 1)
				paddw mm4, PACKED_1			;// mm4 += (1, 1, 1, 1)
				paddw mm6, PACKED_1			;// mm6 += (1, 1, 1, 1)
				psrlw mm0, 1				;// mm0 >>= 1
				psrlw mm2, 1				;// mm2 >>= 1
				psrlw mm4, 1				;// mm4 >>= 1
				psrlw mm6, 1				;// mm6 >>= 1
				movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
				movd mm3, [edi+4]			;// lower 4 bytes in mm3 = edi[4..7]
				movd mm5, [edi+8]			;// lower 4 bytes in mm5 = edi[8..11]
				movd mm7, [edi+12]			;// lower 4 bytes in mm7 = edi[12..15]
				punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
				punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
				punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
				punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
				psubw mm0, mm1				;// mm0 -= mm1
				psubw mm2, mm3				;// mm2 -= mm3
				psubw mm4, mm5				;// mm4 -= mm5
				psubw mm6, mm7				;// mm6 -= mm7
				pmullw mm0, mm0				;// mm0 = sqr(mm0)
				pmullw mm2, mm2				;// mm2 = sqr(mm2)
				pmullw mm4, mm4				;// mm4 = sqr(mm4)
				pmullw mm6, mm6				;// mm6 = sqr(mm6)
				paddw mm0, mm2
				paddw mm4, mm6
				paddw mm0, mm4				;// mm0 += mm2 + mm4 + mm6
				movq mm1, mm0
				punpcklwd mm0, PACKED_0		;// unpack the 4 lower words into mm0
				punpckhwd mm1, PACKED_0		;// unpack the 4 lower words into mm1
				paddd mm0, mm1				;// mm0 += mm1
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				add esi, edx				;// esi += edx
				add edi, edx				;// edi += edx
				dec ecx						;// decrement ecx
				jnz dist2__l3				;// loop while not zero
				mov s, eax					;// s = eax
				emms						;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			p1a = p1 + lx;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1a[i]+1)>>1) - p2[i];
					s+= v*v;
				}
				p1 = p1a;
				p1a+= lx;
				p2+= lx;
			}
		}
	}
	else 
	{
		if(cpu_MMX)
		{
			_asm
			{
				mov esi, blk1				;// esi = blk1
				mov edi, blk2				;// edi = blk2
				mov ecx, h					;// ecx = h
				mov edx, lx					;// edx = lx
				mov eax, 0					;// eax = s
dist2__l4:
				movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
				movd mm1, [esi+1]			;// lower 4 bytes in mm1 = esi[1..4]
				movd mm2, [esi+edx]			;// lower 4 bytes in mm2 = (esi + edx)[0..3]
				movd mm3, [esi+edx+1]		;// lower 4 bytes in mm3 = (esi + edx)[1..4]
				movd mm4, [esi+4]			;// lower 4 bytes in mm4 = esi[4..7]
				movd mm5, [esi+5]			;// lower 4 bytes in mm5 = esi[5..8]
				movd mm6, [esi+edx+4]		;// lower 4 bytes in mm6 = (esi + edx)[4..7]
				movd mm7, [esi+edx+5]		;// lower 4 bytes in mm7 = (esi + edx)[5..8]
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
				movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
				movd mm5, [edi+4]			;// lower 4 bytes in mm5 = edi[4..7]
				punpcklbw mm1, PACKED_0		;// unpack the 4 lower 4 bytes into mm1
				punpcklbw mm5, PACKED_0		;// unpack the 4 lower 4 bytes into mm5
				psubw mm0, mm1				;// mm0 -= mm1
				psubw mm4, mm5				;// mm4 -= mm5
				pmullw mm0, mm0				;// mm0 = sqr(mm0)
				pmullw mm4, mm4				;// mm4 = sqr(mm4)
				paddw mm0, mm4				;// mm0 += mm4
				movq mm1, mm0
				punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
				punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
				paddd mm0, mm1				;// mm0 += mm1
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				movd mm0, [esi+8]			;// lower 4 bytes in mm0 = esi[8..11]
				movd mm1, [esi+9]			;// lower 4 bytes in mm1 = esi[9..12]
				movd mm2, [esi+edx+8]		;// lower 4 bytes in mm2 = (esi + edx)[8..11]
				movd mm3, [esi+edx+9]		;// lower 4 bytes in mm3 = (esi + edx)[9..12]
				movd mm4, [esi+12]			;// lower 4 bytes in mm4 = esi[12..15]
				movd mm5, [esi+13]			;// lower 4 bytes in mm5 = esi[13..16]
				movd mm6, [esi+edx+12]		;// lower 4 bytes in mm6 = (esi + edx)[12..15]
				movd mm7, [esi+edx+13]		;// lower 4 bytes in mm7 = (esi + edx)[13..16]
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
				movd mm1, [edi+8]			;// lower 4 bytes in mm1 = edi[8..11]
				movd mm5, [edi+12]			;// lower 4 bytes in mm5 = edi[12..15]
				punpcklbw mm1, PACKED_0		;// unpack the 4 lower 4 bytes into mm1
				punpcklbw mm5, PACKED_0		;// unpack the 4 lower 4 bytes into mm5
				psubw mm0, mm1				;// mm0 -= mm1
				psubw mm4, mm5				;// mm4 -= mm5
				pmullw mm0, mm0				;// mm0 = sqr(mm0)
				pmullw mm4, mm4				;// mm4 = sqr(mm4)
				paddw mm0, mm4				;// mm0 += mm4
				movq mm1, mm0
				punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
				punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
				paddd mm0, mm1				;// mm0 += mm1
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx
				psrlq mm0, 32				;// shift mm0 to get upper dword
				movd ebx, mm0				;// load lower dword of mm0 into ebx
				add eax, ebx				;// eax += ebx

				add esi, edx				;// esi += edx
				add edi, edx				;// edi += edx
				dec ecx						;// decrement ecx
				jnz dist2__l4				;// loop while not zero
				mov s, eax					;// s = eax
				emms						;// empty MMX state
			}
		}
		else
		{
			s = 0;
			p1 = blk1;
			p2 = blk2;
			p1a = p1 + lx;
			for (j=0; j<h; j++)
			{
				for (i=0; i<16; i++)
				{
					v = ((unsigned int)(p1[i]+p1[i+1]+p1a[i]+p1a[i+1]+2)>>2) - p2[i];
					s+= v*v;
				}
				p1 = p1a;
				p1a+= lx;
				p2+= lx;
			}
		}
	}

	return s;
}


/*
 *(16*h) 块和双向预测之间的绝对误差
 *
 
 */
static int bdist1(pf,pb,p2,lx,hxf,hyf,hxb,hyb,h)
unsigned char *pf,*pb,*p2;
int lx,hxf,hyf,hxb,hyb,h;
{
	unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
	int i,j;
	int s,v;

	if(fastMotionCompensationLevel)
	{
		lx <<= fastMotionCompensationLevel;
		h >>= fastMotionCompensationLevel;
	}

	pfa = pf + hxf;
	pfb = pf + lx*hyf;
	pfc = pfb + hxf;

	pba = pb + hxb;
	pbb = pb + lx*hyb;
	pbc = pbb + hxb;

	if(cpu_MMX)
	{
		_asm
		{
			mov eax, 0					;// eax = s
			mov edx, lx					;// edx = lx
bdist1__l1:
			;// [0..3]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
			movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
			movd mm2, [ebx]				;// lower 4 bytes in mm2 = ebx[0..3]
			movd mm3, [ecx]				;// lower 4 bytes in mm3 = ecx[0..3]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi]				;// lower 4 bytes in mm4 = esi[0..3]
			movd mm5, [edi]				;// lower 4 bytes in mm5 = edi[0..3]
			movd mm6, [ebx]				;// lower 4 bytes in mm6 = ebx[0..3]
			movd mm7, [ecx]				;// lower 4 bytes in mm7 = ecx[0..3]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi]				;// mm1 = esi[0..3]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			movq mm2, mm0
			psubusw mm0, mm1
			psubusw mm1, mm2
			por mm0, mm1				;// mm0 = abs(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[4..7]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi+4]			;// lower 4 bytes in mm0 = esi[4..7]
			movd mm1, [edi+4]			;// lower 4 bytes in mm1 = edi[4..7]
			movd mm2, [ebx+4]			;// lower 4 bytes in mm2 = ebx[4..7]
			movd mm3, [ecx+4]			;// lower 4 bytes in mm3 = ecx[4..7]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi+4]			;// lower 4 bytes in mm4 = esi[4..7]
			movd mm5, [edi+4]			;// lower 4 bytes in mm5 = edi[4..7]
			movd mm6, [ebx+4]			;// lower 4 bytes in mm6 = ebx[4..7]
			movd mm7, [ecx+4]			;// lower 4 bytes in mm7 = ecx[4..7]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+4]			;// mm1 = esi[4..7]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			movq mm2, mm0
			psubusw mm0, mm1
			psubusw mm1, mm2
			por mm0, mm1				;// mm0 = abs(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the lower 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[8..11]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi+8]			;// lower 4 bytes in mm0 = esi[8..11]
			movd mm1, [edi+8]			;// lower 4 bytes in mm1 = edi[8..11]
			movd mm2, [ebx+8]			;// lower 4 bytes in mm2 = ebx[8..11]
			movd mm3, [ecx+8]			;// lower 4 bytes in mm3 = ecx[8..11]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi+8]			;// lower 4 bytes in mm4 = esi[8..11]
			movd mm5, [edi+8]			;// lower 4 bytes in mm5 = esi[8..11]
			movd mm6, [ebx+8]			;// lower 4 bytes in mm6 = esi[8..11]
			movd mm7, [ecx+8]			;// lower 4 bytes in mm7 = esi[8..11]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+8]			;// mm1 = esi[8..11]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			movq mm2, mm0
			psubusw mm0, mm1
			psubusw mm1, mm2
			por mm0, mm1				;// mm0 = abs(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[12..15]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// esi = pfa
			mov ebx, pfb				;// esi = pfb
			mov ecx, pfc				;// esi = pfc
			movd mm0, [esi+12]			;// lower 4 bytes in mm0 = esi[12..15]
			movd mm1, [edi+12]			;// lower 4 bytes in mm1 = edi[12..15]
			movd mm2, [ebx+12]			;// lower 4 bytes in mm2 = ebx[12..15]
			movd mm3, [ecx+12]			;// lower 4 bytes in mm3 = ecx[12..15]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// esi = pba
			mov ebx, pbb				;// esi = pbb
			mov ecx, pbc				;// esi = pbc
			movd mm4, [esi+12]			;// lower 4 bytes in mm4 = esi[12..15]
			movd mm5, [edi+12]			;// lower 4 bytes in mm5 = edi[12..15]
			movd mm6, [ebx+12]			;// lower 4 bytes in mm6 = ebx[12..15]
			movd mm7, [ecx+12]			;// lower 4 bytes in mm7 = ecx[12..15]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+12]			;// mm1 = esi[12..15]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			movq mm2, mm0
			psubusw mm0, mm1
			psubusw mm1, mm2
			por mm0, mm1				;// mm0 = abs(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			add p2, edx					;// p2 += edx
			add pf, edx					;// pf += edx
			add pfa, edx				;// pfa += edx
			add pfb, edx				;// pfb += edx
			add pfc, edx				;// pfc += edx
			add pb, edx					;// pb += edx
			add pba, edx				;// pba += edx
			add pbb, edx				;// pbb += edx
			add pbc, edx				;// pbc += edx
			dec dword ptr h				;// decrement h
			jnz bdist1__l1				;// loop while not zero
			mov s, eax					;// s = eax
			emms						;// empty MMX state
		}
	}
	else
	{
		s = 0;
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = ((((unsigned int)(*pf++ + *pfa++ + *pfb++ + *pfc++ + 2)>>2) +
					((unsigned int)(*pb++ + *pba++ + *pbb++ + *pbc++ + 2)>>2) + 1)>>1)
					- *p2++;

				if (v>=0)
					s+= v;
				else
					s-= v;
			}

			p2+= lx-16;
			pf+= lx-16;
			pfa+= lx-16;
			pfb+= lx-16;
			pfc+= lx-16;
			pb+= lx-16;
			pba+= lx-16;
			pbb+= lx-16;
			pbc+= lx-16;
		}
	}

	return s;
}

/*
 * (16*h) 块和双向预测之间的均方误差
 */
static int bdist2(pf,pb,p2,lx,hxf,hyf,hxb,hyb,h)
unsigned char *pf,*pb,*p2;
int lx,hxf,hyf,hxb,hyb,h;
{
	unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
	int i,j;
	int s,v;

	if(fastMotionCompensationLevel)
	{
		lx <<= fastMotionCompensationLevel;
		h >>= fastMotionCompensationLevel;
	}

	pfa = pf + hxf;
	pfb = pf + lx*hyf;
	pfc = pfb + hxf;

	pba = pb + hxb;
	pbb = pb + lx*hyb;
	pbc = pbb + hxb;

	if(cpu_MMX)
	{
		_asm
		{
			mov eax, 0					;// eax = s
			mov edx, lx					;// edx = lx
bdist2__l1:
			;// [0..3]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi]				;// lower 4 bytes in mm0 = esi[0..3]
			movd mm1, [edi]				;// lower 4 bytes in mm1 = edi[0..3]
			movd mm2, [ebx]				;// lower 4 bytes in mm2 = ebx[0..3]
			movd mm3, [ecx]				;// lower 4 bytes in mm3 = ecx[0..3]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi]				;// lower 4 bytes in mm4 = esi[0..3]
			movd mm5, [edi]				;// lower 4 bytes in mm5 = edi[0..3]
			movd mm6, [ebx]				;// lower 4 bytes in mm6 = ebx[0..3]
			movd mm7, [ecx]				;// lower 4 bytes in mm7 = ecx[0..3]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi]				;// mm1 = esi[0..3]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			psubw mm0, mm1
			pmullw mm0, mm0				;// mm0 = sqr(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[4..7]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi+4]			;// lower 4 bytes in mm0 = esi[4..7]
			movd mm1, [edi+4]			;// lower 4 bytes in mm1 = edi[4..7]
			movd mm2, [ebx+4]			;// lower 4 bytes in mm2 = ebx[4..7]
			movd mm3, [ecx+4]			;// lower 4 bytes in mm3 = ecx[4..7]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi+4]			;// lower 4 bytes in mm4 = esi[4..7]
			movd mm5, [edi+4]			;// lower 4 bytes in mm5 = edi[4..7]
			movd mm6, [ebx+4]			;// lower 4 bytes in mm6 = ebx[4..7]
			movd mm7, [ecx+4]			;// lower 4 bytes in mm7 = ecx[4..7]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+4]			;// mm1 = esi[4..7]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			psubw mm0, mm1
			pmullw mm0, mm0				;// mm0 = sqr(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[8..11]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// edi = pfa
			mov ebx, pfb				;// ebx = pfb
			mov ecx, pfc				;// ecx = pfc
			movd mm0, [esi+8]			;// lower 4 bytes in mm0 = esi[8..11]
			movd mm1, [edi+8]			;// lower 4 bytes in mm1 = edi[8..11]
			movd mm2, [ebx+8]			;// lower 4 bytes in mm2 = ebx[8..11]
			movd mm3, [ecx+8]			;// lower 4 bytes in mm3 = ecx[8..11]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// edi = pba
			mov ebx, pbb				;// ebx = pbb
			mov ecx, pbc				;// ecx = pbc
			movd mm4, [esi+8]			;// lower 4 bytes in mm4 = esi[8..11]
			movd mm5, [edi+8]			;// lower 4 bytes in mm5 = esi[8..11]
			movd mm6, [ebx+8]			;// lower 4 bytes in mm6 = esi[8..11]
			movd mm7, [ecx+8]			;// lower 4 bytes in mm7 = esi[8..11]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+8]			;// mm1 = esi[8..11]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			psubw mm0, mm1
			pmullw mm0, mm0				;// mm0 = sqr(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			;//[12..15]
			mov esi, pf					;// esi = pf
			mov edi, pfa				;// esi = pfa
			mov ebx, pfb				;// esi = pfb
			mov ecx, pfc				;// esi = pfc
			movd mm0, [esi+12]			;// lower 4 bytes in mm0 = esi[12..15]
			movd mm1, [edi+12]			;// lower 4 bytes in mm1 = edi[12..15]
			movd mm2, [ebx+12]			;// lower 4 bytes in mm2 = ebx[12..15]
			movd mm3, [ecx+12]			;// lower 4 bytes in mm3 = ecx[12..15]
			mov esi, pb					;// esi = pb
			mov edi, pba				;// esi = pba
			mov ebx, pbb				;// esi = pbb
			mov ecx, pbc				;// esi = pbc
			movd mm4, [esi+12]			;// lower 4 bytes in mm4 = esi[12..15]
			movd mm5, [edi+12]			;// lower 4 bytes in mm5 = edi[12..15]
			movd mm6, [ebx+12]			;// lower 4 bytes in mm6 = ebx[12..15]
			movd mm7, [ecx+12]			;// lower 4 bytes in mm7 = ecx[12..15]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			punpcklbw mm4, PACKED_0		;// unpack the lower 4 bytes into mm4
			punpcklbw mm5, PACKED_0		;// unpack the lower 4 bytes into mm5
			punpcklbw mm6, PACKED_0		;// unpack the lower 4 bytes into mm6
			punpcklbw mm7, PACKED_0		;// unpack the lower 4 bytes into mm7
			mov esi, p2					;// esi = p2
			paddw mm0, mm1
			paddw mm2, mm3
			paddw mm4, mm5
			paddw mm6, mm7
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			paddw mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			movd mm1, [esi+12]			;// mm1 = esi[12..15]
			paddw mm0, PACKED_2			;// mm0 += (2, 2, 2, 2)
			paddw mm4, PACKED_2			;// mm4 += (2, 2, 2, 2)
			psrlw mm0, 2				;// mm0 >>= 2
			psrlw mm4, 2				;// mm4 >>= 2
			paddw mm0, mm4
			paddw mm0, PACKED_1			;// mm0 += mm4 + (1, 1, 1, 1)
			psrlw mm0, 1				;// mm0 >>= 1

			punpcklbw mm1, PACKED_0		;// unpack lower 4 bytes into mm1
			psubw mm0, mm1
			pmullw mm0, mm0				;// mm0 = sqr(mm0 - mm1)
			movq mm1, mm0
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm0, mm1				;// mm0 += mm1
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx

			add p2, edx					;// p2 += edx
			add pf, edx					;// pf += edx
			add pfa, edx				;// pfa += edx
			add pfb, edx				;// pfb += edx
			add pfc, edx				;// pfc += edx
			add pb, edx					;// pb += edx
			add pba, edx				;// pba += edx
			add pbb, edx				;// pbb += edx
			add pbc, edx				;// pbc += edx
			dec dword ptr h				;// decrement h
			jnz bdist2__l1				;// loop while not zero
			mov s, eax					;// s = eax
			emms						;// empty MMX state
		}
	}
	else
	{
		s = 0;
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = ((((unsigned int)(*pf++ + *pfa++ + *pfb++ + *pfc++ + 2)>>2) +
					((unsigned int)(*pb++ + *pba++ + *pbb++ + *pbc++ + 2)>>2) + 1)>>1)
					- *p2++;

				s+=v*v;
			}
			p2+= lx-16;
			pf+= lx-16;
			pfa+= lx-16;
			pfb+= lx-16;
			pfc+= lx-16;
			pb+= lx-16;
			pba+= lx-16;
			pbb+= lx-16;
			pbc+= lx-16;
		}
	}

	return s;
}

/*
 * 计算一个(16*16) 块的方差，并乘以256
 * p:  块左上角像素的地址
 * lx: 相邻像素的垂直距离（以字节为单位）
 */
static int variance(p,lx)
unsigned char *p;
int lx;
{
	unsigned int i,j;
	unsigned int v,s,s2;

	if(cpu_MMX)
	{
		_asm
		{
			mov esi, p					;// esi = p
			mov edx, lx					;// edx = lx
			mov eax, 0					;// eax = s
			mov edi, 0					;// edi = s2
			mov ecx, 16					;// ecx = 16
variance__l1:
			movd mm0, [esi]				;// lower 4 bytes of mm0 = esi[0..3]
			movd mm1, [esi+4]			;// lower 4 bytes of mm1 = esi[4..7]
			movd mm2, [esi+8]			;// lower 4 bytes of mm2 = esi[8..11]
			movd mm3, [esi+12]			;// lower 4 bytes of mm3 = esi[12..15]
			punpcklbw mm0, PACKED_0		;// unpack the lower 4 bytes into mm0
			punpcklbw mm1, PACKED_0		;// unpack the lower 4 bytes into mm1
			punpcklbw mm2, PACKED_0		;// unpack the lower 4 bytes into mm2
			punpcklbw mm3, PACKED_0		;// unpack the lower 4 bytes into mm3
			movq mm4, mm0
			movq mm5, mm1
			movq mm6, mm2
			movq mm7, mm3
			paddw mm0, mm1
			paddw mm2, mm3
			pmaddwd mm4, mm4			;// mm4 = sqr for each word in mm4 and add them to make two dwords
			paddw mm0, mm2				;// mm0 += mm1 + mm2 + mm3
			pmaddwd mm5, mm5			;// mm5 = sqr for each word in mm5 and add them to make two dwords
			movq mm1, mm0
			pmaddwd mm6, mm6			;// mm6 = sqr for each word in mm6 and add them to make two dwords
			punpcklwd mm0, PACKED_0		;// unpack the lower 2 words into mm0
			pmaddwd mm7, mm7			;// mm7 = sqr for each word in mm6 and add them to make two dwords
			punpckhwd mm1, PACKED_0		;// unpack the upper 2 words into mm1
			paddd mm4, mm5
			paddd mm0, mm1				;// mm0 += mm1
			paddd mm6, mm7
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			paddd mm4, mm6				;// mm4 += mm5 + mm6 + mm7
			add eax, ebx				;// eax += ebx
			psrlq mm0, 32				;// shift mm0 to get upper dword
			movd ebx, mm0				;// load lower dword of mm0 into ebx
			add eax, ebx				;// eax += ebx
			movd ebx, mm4				;// load lower dword of mm4 into ebx
			add edi, ebx				;// edi += ebx
			psrlq mm4, 32				;// shift mm4 to get upper dword
			movd ebx, mm4				;// load lower dword of mm4 into ebx
			add edi, ebx				;// edi += ebx

			add esi, edx				;// esi += edx
			dec ecx						;// decrement ecs
			jnz variance__l1			;// loop while not zero
			mov s, eax					;// s = eax
			mov s2, edi					;// s2 = edi
			emms						;// empty MMX state
		}
	}
	else
	{
		s = s2 = 0;
		for (j=0; j<16; j++)
		{
			for (i=0; i<16; i++)
			{
				v = *p++;
				s+= v;
				s2+= v*v;
			}
			p+= lx-16;
		}
	}

	return s2 - (s*s)/256;
}

static int calcDist16(unsigned char *p1In, unsigned char *p2In)
{

  unsigned char *p1,*p2;
  int v;
  int s = 0;

  

  p1 = p1In;
  p2 = p2In;

  if (cpu_MMX)
  {
     __asm {
		mov         eax, dword ptr [p2]
		movq        mm1, qword ptr [eax]   ; //put p2 into mm1
		mov         eax, dword ptr [p1]
		movq        mm0, qword ptr [eax]   ; //put p1 into mm0
		psubusb     mm0, mm1               ; //subtract with saturate (mm0 = mm0 - mm1)
		movq        mm3, qword ptr [eax]   ; //put p1 in mm3
		psubusb     mm1, mm3               ; //subtract with saturate (mm1 = mm1 - mm3)
		por         mm1, mm0               ; //put all the results back together in mm1
		movq        mm2, mm1               ; //mm2 = mm1

		movq        mm0, mm4               ;
		pxor        mm4, mm0               ; //mm4 = 0

		punpcklbw   mm1, mm4               ; //unpack the lower 4 bytes into mm1
		punpckhbw   mm2, mm4               ; //unpack the upper 4 bytes into mm2

		paddw       mm1, mm2               ; //add (mm1 = mm1 + mm2)
		movq        mm5, mm1               ; //mm5 = mm1

		mov         eax, dword ptr [p2]
		movq        mm1, qword ptr [eax+8] ; //put p2+8 into mm2
		mov         eax, dword ptr [p1]
		movq        mm0, qword ptr [eax+8] ; //put p1+8 into mm0
		psubusb     mm0, mm1               ; //subtract with saturate (mm0 = mm0 - mm1)
		movq        mm3, qword ptr [eax+8] ; //put p1 back in mm0
		psubusb     mm1, mm3               ; //subtract with saturate (mm1 = mm1 - mm0)
		por         mm1, mm0               ; //put all the results back together in mm1
		movq        mm2, mm1               ; //mm2 = mm1

		punpcklbw   mm1, mm4               ; //unpack the lower 4 bytes into mm1
		punpckhbw   mm2, mm4               ; //unpack the upper 4 bytes into mm2
		paddw       mm1, mm2               ; //add (mm1 = mm1 + mm2)

		paddw       mm1, mm5               ; //mm1 has 16bit values to add
		movq        mm5, mm1

		punpcklwd   mm1, mm4
		punpckhwd   mm5, mm4
		paddd       mm1, mm5               ; //mm1 has two 32bit values to add

		movd        eax, mm1               ; //eax = lower 32bit value
		psrlq       mm1, 0x20

		movd        ebx, mm1               ; //ebx = upper 32bit value
		add         eax, ebx               ; //get final difference and add to s
		add           s, eax
		emms
	 }
  }
  else
  {
	if ((v = p1[0]  - p2[0])<0)  v = -v; s+= v;
	if ((v = p1[1]  - p2[1])<0)  v = -v; s+= v;
	if ((v = p1[2]  - p2[2])<0)  v = -v; s+= v;
	if ((v = p1[3]  - p2[3])<0)  v = -v; s+= v;
	if ((v = p1[4]  - p2[4])<0)  v = -v; s+= v;
	if ((v = p1[5]  - p2[5])<0)  v = -v; s+= v;
	if ((v = p1[6]  - p2[6])<0)  v = -v; s+= v;
	if ((v = p1[7]  - p2[7])<0)  v = -v; s+= v;
	if ((v = p1[8]  - p2[8])<0)  v = -v; s+= v;
	if ((v = p1[9]  - p2[9])<0)  v = -v; s+= v;
	if ((v = p1[10] - p2[10])<0) v = -v; s+= v;
	if ((v = p1[11] - p2[11])<0) v = -v; s+= v;
	if ((v = p1[12] - p2[12])<0) v = -v; s+= v;
	if ((v = p1[13] - p2[13])<0) v = -v; s+= v;
	if ((v = p1[14] - p2[14])<0) v = -v; s+= v;
	if ((v = p1[15] - p2[15])<0) v = -v; s+= v;
  }

  return s;
}

#pragma warning( default : 4799 )

