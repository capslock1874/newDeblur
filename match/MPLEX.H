

#include <stdio.h>
#include "bitstrm.h"
#ifdef TIMER
#include <sys/time.h>
#endif


/*************************************************************************
    Definitionen
*************************************************************************/
 
#define MPLEX_VER    " 1.1  "
#define MPLEX_DATE   "06.06.95"

#define SEQUENCE_HEADER 	0x000001b3
#define SEQUENCE_END		0x000001b7
#define PICTURE_START		0x00000100
#define GROUP_START		0x000001b8
#define SYNCWORD_START		0x000001
#define IFRAME                  1
#define PFRAME                  2
#define BFRAME                  3
#define DFRAME                  4

#define AUDIO_SYNCWORD		0xfff

#define PACK_START		0x000001ba
#define SYS_HEADER_START	0x000001bb
#define ISO11172_END		0x000001b9
#define PACKET_START		0x000001

#define MAX_FFFFFFFF		4294967295.0 	

#define CLOCKS			90000.0		

#define AFTER_PACKET_LENGTH	15		
					
#define LAST_SCR_BYTE_IN_PACK	9		


#define SYS_HEADER_LENGTH	12		

#define SYS_HEADER_SIZE		18		
#define PACK_HEADER_SIZE	12

#define PACKET_HEADER_SIZE	6

#define MAX_SECTOR_SIZE		4096	

#define STREAMS_VIDEO           1
#define STREAMS_AUDIO           2
#define STREAMS_BOTH            3

#define AUDIO_STREAMS		0xb8		
#define VIDEO_STREAMS		0xb9		
#define AUDIO_STR_0		0xc0		
#define VIDEO_STR_0		0xe0		
#define PADDING_STR		0xbe		

#define ZERO_STUFFING_BYTE	0
#define STUFFING_BYTE		0xff
#define RESERVED_BYTE		0xff
#define TIMESTAMPS_NO		0	
#define TIMESTAMPS_PTS		1		
#define TIMESTAMPS_PTS_DTS	2		

#define MARKER_SCR		2		
#define MARKER_JUST_PTS		2		
#define MARKER_PTS		3		
#define MARKER_DTS		1		
#define MARKER_NO_TIMESTAMPS	0x0f		

#define STATUS_AUDIO_END	0		
#define STATUS_VIDEO_END	1	
#define STATUS_AUDIO_TIME_OUT	2		
#define STATUS_VIDEO_TIME_OUT	3		


typedef struct timecode_struc	
{   unsigned long msb;	
    unsigned long lsb;
} Timecode_struc;	

typedef struct vaunit_struc	
{   unsigned int length		;
    unsigned int type		;
    Timecode_struc DTS		;
    Timecode_struc PTS		;
} Vaunit_struc;

typedef struct aaunit_struc	
{   unsigned long length	;
    Timecode_struc PTS		;
} Aaunit_struc;

typedef struct video_struc	
{   unsigned int stream_length  ;
    unsigned int num_sequence 	;
    unsigned int num_seq_end	;
    unsigned int num_pictures 	;
    unsigned int num_groups 	;
    unsigned int num_frames[4] 	;
    unsigned int avg_frames[4]  ;
    
    unsigned int horizontal_size;
    unsigned int vertical_size 	;
    unsigned int aspect_ratio	;
    unsigned int picture_rate	;
    unsigned int bit_rate 	;
    unsigned int comp_bit_rate	;
    unsigned int vbv_buffer_size;
    unsigned int CSPF 		;
} Video_struc; 		

typedef struct audio_struc	
{   unsigned int stream_length  ;
    unsigned int num_syncword	;
    unsigned int num_frames [2]	;
    unsigned int size_frames[2] ;
    unsigned int layer		;
    unsigned int protection	;
    unsigned int bit_rate	;
    unsigned int frequency	;
    unsigned int mode		;
    unsigned int mode_extension ;
    unsigned int copyright      ;
    unsigned int original_copy  ;
    unsigned int emphasis	;
} Audio_struc; 	

typedef struct sector_struc	
{   unsigned char  buf [MAX_SECTOR_SIZE] ;
    unsigned int   length_of_sector  ;
    unsigned int   length_of_packet_data ;
    Timecode_struc TS                ;
} Sector_struc;

typedef struct pack_struc	
{   unsigned char  buf [PACK_HEADER_SIZE];
    Timecode_struc SCR;
} Pack_struc;

typedef struct sys_header_struc	
{   unsigned char  buf [SYS_HEADER_SIZE];
} Sys_header_struc;

typedef struct buffer_queue	
{   unsigned int size	;	
    Timecode_struc DTS	;
    struct buffer_queue *next	;
} Buffer_queue;
    

typedef struct buffer_struc	
{   unsigned int max_size;	
    Buffer_queue *first;
} Buffer_struc;
    
    


void check_files          ();	
				
int  open_file            ();	
void get_info_video       ();	
void output_info_video    ();	
void get_info_audio       ();	
void output_info_audio    ();	
void marker_bit           ();	
void empty_video_struc    ();	
void empty_audio_struc    ();	
void empty_vaunit_struc   ();	
void empty_aaunit_struc   ();	
void empty_sector_struc   ();	
void empty_timecode_struc ();	
void init_buffer_struc    ();	

void offset_timecode      ();	
void copy_timecode        ();	
void make_timecode        ();	
				
void add_to_timecode      ();	
void buffer_timecode      ();	
int  comp_timecode        ();

void create_sector	  ();
void create_sys_header	  ();	
void create_pack	  ();	

void output_video         ();
void output_audio         ();   
void output_padding       ();	

void next_video_access_unit ();	
void next_audio_access_unit ();	

void buffer_clean	  ();	
unsigned int  buffer_space         ();	
void queue_buffer         ();	

void outputstream         ();	
void status_info          ();	
			
void status_header	  ();	
void status_message	  ();	
void status_footer	  ();	

void ask_continue	  ();	
unsigned char ask_verbose ();	



static double picture_rates [9] = { 0., 24000./1001., 24., 25., 
	30000./1001., 30., 50., 60000./1001., 60. };

static double ratio [16] = { 0., 1., 0.6735, 0.7031, 0.7615, 0.8055,
	0.8437, 0.8935, 0.9157, 0.9815, 1.0255, 1.0695, 1.0950, 1.1575,
	1.2015, 0.};

static unsigned int bitrate_index [3][16] =
    {{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static double frequency [4] = {44.1, 48, 32, 0};
static unsigned int slots [4] = {12, 144, 0, 0};
static unsigned int samples [4] = {384, 1152, 0, 0};

static char mode [4][15] =
    { "stereo", "joint stereo", "dual channel", "single channel" };
static char copyright [2][20] =
    { "no copyright","copyright protected" };
static char original [2][10] =
    { "copy","original" };
static char emphasis [4][20] =
    { "none", "50/15 microseconds", "reserved", "CCITT J.17" };

