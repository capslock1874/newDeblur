/*************************************************************************
合并音频视频码流
*************************************************************************/





#include "mplex.h"

extern unsigned int audio_time_out;
extern unsigned int video_time_out;
extern unsigned int audio_end_early;
extern unsigned int video_end_early;

/*************************************************************************
    Main
*************************************************************************/

#ifdef TIMER
    long total_sec = 0;
    long total_usec = 0;
    long global_sec = 0;
    long global_usec = 0;
    struct timeval  tp_start;
    struct timeval  tp_end;
    struct timeval  tp_global_start;
    struct timeval  tp_global_end;
#endif

int mplex (vfile, afile, mfile)

char *vfile, *afile, *mfile;
{
    char 	*audio_file = NULL;
    char        *video_file = NULL;
    char        *multi_file = NULL;	

    char	*video_units = NULL;
    char	*audio_units = NULL;

    Video_struc video_info;
    Audio_struc audio_info;
    unsigned int audio_bytes, video_bytes;
    //unsigned int which_streams=0;
    unsigned int which_streams=STREAMS_BOTH;
    double	startup_delay=0;

	audio_time_out = 0;
	video_time_out = 0;
	audio_end_early = 0;
	video_end_early = 0;

    check_files (vfile, afile, mfile, &audio_file, &video_file, &multi_file,
		 &audio_bytes, &video_bytes, &which_streams);

    empty_video_struc (&video_info);
    empty_audio_struc (&audio_info);
    
    if (which_streams & STREAMS_VIDEO) {
	video_units=tempnam ("./","tmp_v");
	get_info_video (video_file, video_units, &video_info, &startup_delay,
			video_bytes);
    }

    if (which_streams & STREAMS_AUDIO) {
	audio_units=tempnam ("./","tmp_a");
	get_info_audio (audio_file, audio_units, &audio_info, &startup_delay,
			audio_bytes);
    }

    outputstream (video_file, video_units, &video_info,
		  audio_file, audio_units, &audio_info, multi_file, which_streams );

    return (0);	
}

			
