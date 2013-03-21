
/***********************************************************************
*
*  编码器头文件
*
***********************************************************************/


/* 全局声明 */

/* 缺省输入参数 */

#define DFLT_LAY        2     
#define DFLT_MOD        'j'    
#define DFLT_PSY        2      
#define DFLT_SFQ        44.1   
#define DFLT_BRT        128    
#define DFLT_EMP        'n'    
#define DFLT_EXT        ".mp2" 


#define NOISY_MIN_MNR   0.0

/* 心理声学模型1 */

#define CB_FRACTION     0.33
#define MAX_SNR         1000
#define NOISE           10
#define TONE            20
#define DBMIN           -200.0
#define LAST            -1
#define STOP            -100
#define POWERNORM       90.3090 

/*心理声学模型2 */

#define LOGBLKSIZE      10
#define BLKSIZE         1024
#define HBLKSIZE        513
#define CBANDS          63
#define LXMIN           32.0



#define PCM_BUFFER		2304

/***********************************************************************
*
*  数据类型 
***********************************************************************/

/* 心理声学模型1*/

typedef int        IFFT2[FFT_SIZE/2];
typedef int        IFFT[FFT_SIZE];
typedef double     D9[9];
typedef double     D10[10];
typedef double     D640[640];
typedef double     D1408[1408];
typedef double     DFFT2[FFT_SIZE/2];
typedef double     DFFT[FFT_SIZE];
typedef double     DSBL[SBLIMIT];
typedef double     D2SBL[2][SBLIMIT];

typedef struct {
        int        line;
        double     bark, hear, x;
} g_thres, *g_ptr;

typedef struct {
        double     x;
        int        type, next, map;
} mask, *mask_ptr;

/* 心理声学模型2 */

typedef int        ICB[CBANDS];
typedef int        IHBLK[HBLKSIZE];
typedef FLOAT      F32[32];
typedef FLOAT      F2_32[2][32];
typedef FLOAT      FCB[CBANDS];
typedef FLOAT      FCBCB[CBANDS][CBANDS];
typedef FLOAT      FBLK[BLKSIZE];
typedef FLOAT      FHBLK[HBLKSIZE];
typedef FLOAT      F2HBLK[2][HBLKSIZE];
typedef FLOAT      F22HBLK[2][2][HBLKSIZE];
typedef double     DCBB[CBANDS];

/***********************************************************************
*
*  函数声明
*
***********************************************************************/


extern void   obtain_parameters(frame_params*, int*, unsigned long*,
                           char[MAX_NAME_SIZE], char[MAX_NAME_SIZE]);
extern void   parse_args(int, char**, frame_params*, int*, unsigned long*,
                           char[MAX_NAME_SIZE], char[MAX_NAME_SIZE]);
extern void   print_config(frame_params*, int*, unsigned long*,
                           char[MAX_NAME_SIZE], char[MAX_NAME_SIZE]);
extern void   usage(void);


extern unsigned long    read_samples(short[2304], unsigned long,
                           unsigned long);
extern unsigned long    get_audio(short[2][1152], unsigned long,
                           int, int);
extern void   read_ana_window(double[HAN_SIZE]);
extern void   window_subband(short**, double[HAN_SIZE], int);
extern void   create_ana_filter(double[SBLIMIT][64]);
extern void   filter_subband(double[HAN_SIZE], double[SBLIMIT]);
extern void   encode_info(frame_params*, Bit_stream_struc*);
extern double mod(double);
extern void   I_combine_LR(double[2][3][SCALE_BLOCK][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT]);
extern void   II_combine_LR(double[2][3][SCALE_BLOCK][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], int);
extern void   I_scale_factor_calc(double[][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[][3][SBLIMIT], int);
extern void   II_scale_factor_calc(double[][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[][3][SBLIMIT], int, int);
extern void   pick_scale(unsigned int[2][3][SBLIMIT], frame_params*,
                           double[2][SBLIMIT]);
extern void   put_scale(unsigned int[2][3][SBLIMIT], frame_params*,
                           double[2][SBLIMIT]);
extern void   II_transmission_pattern(unsigned int[2][3][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*);
extern void   II_encode_scale(unsigned int[2][SBLIMIT],
                           unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern void   I_encode_scale(unsigned int[2][3][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern int    II_bits_for_nonoise(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           frame_params*);
extern void   II_main_bit_allocation(double[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           int*, frame_params*);
extern int    II_a_bit_allocation(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], int*, frame_params*);
extern int    I_bits_for_nonoise(double[2][SBLIMIT], frame_params*);
extern void   I_main_bit_allocation(double[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], int*, frame_params*);
extern int    I_a_bit_allocation(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           int*, frame_params*);
extern void   I_subband_quantization(unsigned int[2][3][SBLIMIT],
                           double[2][3][SCALE_BLOCK][SBLIMIT], unsigned int[3][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SCALE_BLOCK][SBLIMIT], frame_params*);
extern void   II_subband_quantization(unsigned int[2][3][SBLIMIT],
                           double[2][3][SCALE_BLOCK][SBLIMIT], unsigned int[3][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SCALE_BLOCK][SBLIMIT], frame_params*);
extern void   II_encode_bit_alloc(unsigned int[2][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern void   I_encode_bit_alloc(unsigned int[2][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern void   I_sample_encoding(unsigned int[2][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern void   II_sample_encoding(unsigned int[2][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*,
                           Bit_stream_struc*);
extern void   encode_CRC(unsigned int, Bit_stream_struc*);


extern void        psycho_anal(short int*, short int[1056], int, int,
                           FLOAT[32], double);


extern void        fft(FLOAT[BLKSIZE], FLOAT[BLKSIZE], FLOAT[BLKSIZE],
                           FLOAT[BLKSIZE], int );

