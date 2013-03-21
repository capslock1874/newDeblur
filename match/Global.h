/* global.h, 声明全局变量和函数      */



#include "avi2m1v.h"
#include <windows.h>
#include <vfw.h>
#include <time.h>

/* 全局函数的声明 */


#ifndef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif

/* avi2mpg1.c */
void TestCPU(void);

/* conform.c */
void range_checks(void);

/* fdctref.c */
void init_fdct(void);
void fdct(short *block);

/* idct.c */
void idct(short *block);
void init_idct(void);

/* motion.c */
void motion_estimation(unsigned char *oldorg, unsigned char *neworg,
  unsigned char *oldref, unsigned char *newref, unsigned char *cur,
  unsigned char *curref, int sxf, int syf, int sxb, int syb,
  struct mbinfo *mbi, int secondfield, int ipflag);

/* avi2m1v.c */
int avi2m1v(char *);
void error(char *text);

/* predict.c */
void predict(unsigned char *reff[], unsigned char *refb[],
  unsigned char *cur[3], int secondfield, struct mbinfo *mbi);

/* putbits.c */
void initbits(void);
void putbits(int val, int n);
void alignbits(void);
void flushbits(void);
int bitcount(void);

/* puthdr.c */
void putseqhdr(void);
void putseqext(void);
void putseqdispext(void);
void putuserdata(char *userdata);
void putgophdr(int frame, int closed_gop);
void putpicthdr(void);
void putpictcodext(void);
void putseqend(void);

/* putmpg.c */
void putintrablk(short *blk, int cc);
void putnonintrablk(short *blk);
void putmv(int dmv, int f_code);

/* putpic.c */
void putpict(unsigned char *frame);

/* putseq.c */
void putseq(void);

/* putvlc.c */
void putDClum(int val);
void putDCchrom(int val);
void putACfirst(int run, int val);
void putAC(int run, int signed_level, int vlcformat);
void putaddrinc(int addrinc);
void putmbtype(int pict_type, int mb_type);
void putmotioncode(int motion_code);
void putdmv(int dmv);
void putcbp(int cbp);

/* quantize.c */
int quant_intra(short *src, short *dst, int dc_prec,
  unsigned char *quant_mat, unsigned short *recip_quant_mat, int mquant);
int quant_non_intra(short *src, short *dst,
  unsigned char *quant_mat, unsigned short *recip_quant_mat, int mquant);
void iquant_intra(short *src, short *dst, int dc_prec,
  unsigned char *quant_mat, int mquant);
void iquant_non_intra(short *src, short *dst,
  unsigned char *quant_mat, int mquant);

/* ratectl.c */
void rc_init_seq(void);
void rc_init_GOP(int np, int nb);
void rc_init_pict(unsigned char *frame);
void rc_update_pict(void);
int rc_start_mb(void);
int rc_calc_mquant(int j);
void vbv_end_of_picture(void);
void calc_vbv_delay(void);

/* readpic.c */
void readframe(unsigned long lCurFrame, unsigned char *frame[]);

/* stats.c */
void calcSNR(unsigned char *org[3], unsigned char *rec[3]);
void stats(void);

/* transfrm.c */
void transform(unsigned char *pred[], unsigned char *cur[],
  struct mbinfo *mbi, short blocks[][64]);
void itransform(unsigned char *pred[], unsigned char *cur[],
  struct mbinfo *mbi, short blocks[][64]);
void dct_type_estimation(unsigned char *pred, unsigned char *cur,
  struct mbinfo *mbi);

/* writepic.c */
void writeframe(char *fname, unsigned char *frame[]);


/* global variables */

EXTERN int wr_seqh2gop; 

EXTERN char version[]
#ifdef GLOBAL
  ="AVI2MPG1 V1.11, July 10th, 2000"
#endif
;

EXTERN char author[]
#ifdef GLOBAL
  ="(C) 1997, 98, 99, 2000 John Schlichther"
#endif
;

EXTERN unsigned long length;
EXTERN WAVEFORMATEX pWavFormat;
EXTERN unsigned int vbv_ovflow;
EXTERN unsigned int vbv_unflow;
EXTERN char video_param_filename[_MAX_PATH];
EXTERN unsigned int use_v_param_file;
EXTERN unsigned long last_good_video_frame;
EXTERN unsigned int fake_bad_frames;
EXTERN unsigned int video_only;
EXTERN unsigned int frame_repl;
EXTERN unsigned int vbv_size;
EXTERN unsigned int bad_frame_count;
EXTERN unsigned int bad_audio_count;
EXTERN int m_search_size;
EXTERN time_t   start_time;
EXTERN unsigned long bytes_processed;

// multiplex error counts

EXTERN unsigned int audio_time_out;
EXTERN unsigned int video_time_out;
EXTERN unsigned int audio_end_early;
EXTERN unsigned int video_end_early;

/* zig-zag 扫描 */
EXTERN unsigned char zig_zag_scan[64]
#ifdef GLOBAL
=
{
  0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
  12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
  58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
}
#endif
;

/* 另一种扫描方式 */
EXTERN unsigned char alternate_scan[64]
#ifdef GLOBAL
=
{
  0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
  41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
  51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
  53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
}
#endif
;

/* 默认的帧内量化矩阵 */
EXTERN unsigned char default_intra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
   8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83
}
#endif
;

/* 非线性量化系数表 */
EXTERN unsigned char non_linear_mquant_table[32]
#ifdef GLOBAL
=
{
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112
}
#endif
;



EXTERN unsigned char map_non_linear_mquant[113] 
#ifdef GLOBAL
=
{
0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
}
#endif
;

/* 图像数据数组 */

/* 重建帧 */
EXTERN unsigned char *newrefframe[3], *oldrefframe[3], *auxframe[3];
/* 原始帧 */
EXTERN unsigned char *neworgframe[3], *oldorgframe[3], *auxorgframe[3];
/* 当前帧预测 */
EXTERN unsigned char *predframe[3];
/* 8*8 块数据 */
EXTERN short (*blocks)[64];
/* 量化矩阵 */
EXTERN unsigned char intra_q[64], inter_q[64];
EXTERN unsigned short recip_intra_q[64], recip_inter_q[64]; 
EXTERN unsigned char chrom_intra_q[64],chrom_inter_q[64];
/* DCT系数(0,0)的预测值 */
EXTERN int dc_dct_pred[3];
/* 宏块边信息矩阵 */
EXTERN struct mbinfo *mbinfo;
/* 运动预测参数 */
EXTERN struct motion_data *motion_data;
/* 限幅表 */
EXTERN unsigned char *clp;

/* U V 临时缓存 */
EXTERN unsigned char *Ubuffer, *Vbuffer, *Ubuffer422, *Vbuffer422, *TempFilterBuffer;

/* 名称字符串 */
EXTERN char id_string[256], tplorg[256], tplref[256];
EXTERN char iqname[256], niqname[256];
EXTERN char statname[256];
EXTERN char errortext[256];

EXTERN FILE *outfile, *statfile; /* 文件描述 */
EXTERN int inputtype;            /* 输入帧格式 */

EXTERN int quiet;                /* 关闭警告信息 */


/* 编码模型参数 */

EXTERN int N;           /* 图像组中的帧的数目 */
EXTERN int M;           /* I/P帧之间的距离*/
EXTERN int P;           /* 片断刷新间隔 */
EXTERN int nframes;     /* 用来编码的帧的总数 */
EXTERN int frame0, tc0; /* 第一帧的编号及时间代码 */
EXTERN int fieldpic;    /* 使用场图片 */

EXTERN unsigned int system_byterate_parm;
EXTERN unsigned int audio_bitrate_parm;
EXTERN unsigned int audio_layer_parm;
EXTERN unsigned int joint_stereo_parm;
EXTERN unsigned int vcd_parm;
EXTERN unsigned int crop_size;
EXTERN unsigned int crop_horz;
EXTERN unsigned int crop_vert;
EXTERN unsigned int forced_frame_rate;


EXTERN int horizontal_size, vertical_size; /* 帧的大小*/
EXTERN int width, height;                  /* 编码后的帧的大小再乘上16或32 */
EXTERN int chrom_width,chrom_height,block_count;
EXTERN int mb_width, mb_height;            /* 宏块大小 */
EXTERN int width2, height2, mb_height2, chrom_width2; /* 图像大小 */
EXTERN int aspectratio;                    /* 高宽比信息 */
EXTERN int frame_rate_code;                /* 帧速率的编码值 */
EXTERN double frame_rate;                  /* 每秒帧数 */
EXTERN double bit_rate;                    /* 每秒比特数 */
EXTERN int vbv_buffer_size;                /* VBV 缓存器的大小 */
EXTERN int constrparms;                    /* 约束参数标志(MPEG-1) */
EXTERN int load_iquant, load_niquant;      /* 不使用缺省的量化矩阵 */
EXTERN int load_ciquant,load_cniquant;
EXTERN unsigned int use_image_noise_reduction; /* 滤除输入AVI的噪声*/



EXTERN int profile, level; /* 语法/参数约束*/
EXTERN int prog_seq;       /* 前向序列 */
EXTERN int chroma_format;
EXTERN int low_delay;     



EXTERN int video_format;             /* PAL, NTSC, SECAM 或MAC */
EXTERN int color_primaries;          /* 源的色坐标 */
EXTERN int transfer_characteristics; 
EXTERN int matrix_coefficients;      /* Eg,Eb,Er / Y,Cb,Cr 矩阵系数 */
EXTERN int display_horizontal_size, display_vertical_size; /* 显示大小 */



EXTERN int temp_ref;  
EXTERN int pict_type; /* 图像编码类型 (I, P 或 B) */
EXTERN int vbv_delay; /* 视频缓存确认器延时(1/90000 seconds) */



EXTERN int forw_hor_f_code, forw_vert_f_code;
EXTERN int back_hor_f_code, back_vert_f_code; /* 运动向量范围 */
EXTERN int dc_prec;                           /* 帧内编码块的DC系数精度*/
EXTERN int pict_struct;                       /* 图像结构*/
EXTERN int topfirst;                          /* 先显示上半部分场图像 */
                                    /* 仅用帧预测和帧DCT (I,P,B,current) */
EXTERN int frame_pred_dct_tab[3], frame_pred_dct;
EXTERN int conceal_tab[3];                    /* 使用隐藏运动向量 */
EXTERN int qscale_tab[3], q_scale_type;       /* 线性/非线性量化表 */
EXTERN int intravlc_tab[3], intravlc;         /* 帧内vlc格式 (I,P,B,current) */
EXTERN int altscan_tab[3], altscan;           /* 其他扫描方式(I,P,B,current) */
EXTERN int repeatfirst;                       /* 在第二场图像之后重复第一场*/
EXTERN int prog_frame;                        /* 前向帧 */

/*与CPU有关的参数*/
EXTERN unsigned int cpu_MMX;
EXTERN unsigned int cpu_3DNow;

/* 内部算法参数*/
EXTERN unsigned int doublePrecision;
EXTERN unsigned int fastQuantization;
EXTERN unsigned int fastMotionCompensationLevel;
