
#define      UNIX            
#define      MS_DOS          

/***********************************************************************
*
* 全局的头文件
*
***********************************************************************/

#include        <stdio.h>
#include        <string.h>
#include        <math.h>

#include		<stdlib.h>
#include		<malloc.h>

/***********************************************************************
*
*  全局的宏声明
*
***********************************************************************/

/* General Definitions */

#define         FLOAT                   double

#define         FALSE                   0
#define         TRUE                    1
#define         NULL_CHAR               '\0'

#define         BITS_IN_A_BYTE          8
#define         WORD                    16



#define         MINIMUM         4    /* 缓存器最小值*/
#define         MAX_LENGTH      32   
#define         READ_MODE       0
#define         WRITE_MODE      1
#define         ALIGNING        8
#define         BINARY          0
#define         ASCII           1
#define         BS_FORMAT       BINARY 
#define         BUFFER_SIZE     4096

#define         MIN(A, B)       ((A) < (B) ? (A) : (B))
#define         MAX(A, B)       ((A) > (B) ? (A) : (B))


typedef struct  bit_stream_struc {
    FILE        *pt;            
    unsigned char *buf;         
    int         buf_size;       
    long        totbit;         
    int         buf_byte_idx;   
    int         buf_bit_idx;    
    int         mode;           
    int         eob;           
    int         eobs;          
    char        format;
    
  
} Bit_stream_struc;


/***********************************************************************
*
*  声明全局函数
*
***********************************************************************/


extern int            memcheck(char*, int, int);
extern void           *mem_alloc(unsigned long, char*);
extern void           mem_free(void**);
extern void            refill_buffer(Bit_stream_struc*);
extern void           empty_buffer(Bit_stream_struc*, int);
extern void           open_bit_stream_w(Bit_stream_struc*, char*, int);
extern void           open_bit_stream_r(Bit_stream_struc*, char*, int);
extern void           close_bit_stream_r(Bit_stream_struc*);
extern void           close_bit_stream_w(Bit_stream_struc*);
extern void           alloc_buffer(Bit_stream_struc*, int);
extern void           desalloc_buffer(Bit_stream_struc*);
extern void           back_track_buffer(Bit_stream_struc*, int);
extern unsigned int   get1bit(Bit_stream_struc*);
extern void           put1bit(Bit_stream_struc*, int);
extern unsigned long  look_ahead(Bit_stream_struc*, int);
extern unsigned long  getbits(Bit_stream_struc*, int);
extern void           byte_ali_putbits(Bit_stream_struc*, unsigned int, int);
extern unsigned long  byte_ali_getbits(Bit_stream_struc*, int);
extern unsigned long  sstell(Bit_stream_struc*);
extern int            end_bs(Bit_stream_struc*);
extern int            seek_sync(Bit_stream_struc*, long, int);

