/* avi2m1v.h, 
   该文件包含了定义和数据类型*/





#define PICTURE_START_CODE 0x100L
#define SLICE_MIN_START    0x101L
#define SLICE_MAX_START    0x1AFL
#define USER_START_CODE    0x1B2L
#define SEQ_START_CODE     0x1B3L
#define EXT_START_CODE     0x1B5L
#define SEQ_END_CODE       0x1B7L
#define GOP_START_CODE     0x1B8L
#define ISO_END_CODE       0x1B9L
#define PACK_START_CODE    0x1BAL
#define SYSTEM_START_CODE  0x1BBL

/* 图像编码类型 */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/* 图像结构  */
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3

/* 宏块类型 */
#define MB_INTRA    1
#define MB_PATTERN  2
#define MB_BACKWARD 4
#define MB_FORWARD  8
#define MB_QUANT    16

/* 运动类型 */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* 运动向量格式 */
#define MV_FIELD 0
#define MV_FRAME 1

/* 色度格式 */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

/* 扩展起始编码ID */

#define SEQ_ID       1
#define DISP_ID      2
#define QUANT_ID     3
#define SEQSCAL_ID   5
#define PANSCAN_ID   7
#define CODING_ID    8
#define SPATSCAL_ID  9
#define TEMPSCAL_ID 10

/* 宏块信息*/
struct mbinfo {
  int mb_type;            /* 帧内/前向/后向/交织*/
  int motion_type;        /* 帧/场/16*8/dual_prime */
  int dct_type;           /* 场或帧DCT */
  int mquant;             /* 量化参数*/
  int cbp;                /* 编码后块模型*/
  int skipped;            /* skipped 宏块*/
  int MV[2][2][2];        /* 运动向量*/
  int mv_field_sel[2][2]; /* 运动场选择*/
  int dmvector[2];        /* dual prime向量 */
  double act;             /* activity measure */
  int var;                /* 用于调试*/
};

/* motion data */
struct motion_data {
  int forw_hor_f_code,forw_vert_f_code; 
  int sxf,syf;                          
  int back_hor_f_code,back_vert_f_code;
  int sxb,syb;
};
