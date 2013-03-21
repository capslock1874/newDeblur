
#include "mplex.h"
#ifdef TIMER
    extern long total_sec;
    extern long total_usec;
    extern long global_sec;
    extern long global_usec;
    extern struct timeval  tp_start;
    extern struct timeval  tp_end;
    extern struct timeval  tp_global_start;
    extern struct timeval  tp_global_end;
#endif
extern unsigned int vcd_parm;
/******************************************************************
   复用迭代程序
   
    将视频和音频码流复用在一起	 

******************************************************************/

void outputstream (video_file, video_units, video_info,
		   audio_file, audio_units, audio_info,
		   multi_file, which_streams )

char 		*video_file;
char 		*video_units;
Video_struc 	*video_info;
char 		*audio_file;
char 		*audio_units;
Audio_struc 	*audio_info;
char 		*multi_file;
unsigned int    which_streams;

{

    FILE *istream_v;			
    FILE *istream_a;			
    FILE *ostream;			
    FILE *vunits_info;			
    FILE *aunits_info;			

    Vaunit_struc video_au;		
    Aaunit_struc audio_au;		

    unsigned int data_rate=0;		
    unsigned int video_rate=0;
    unsigned int audio_rate=0;
    double delay,audio_delay,video_delay;
    double clock_cycles;
    double audio_next_clock_cycles;
    double video_next_clock_cycles;
    unsigned int bytes_output;
    double dmux_rate;
    unsigned long sectors_delay,video_delay_ms,audio_delay_ms;
    unsigned int mux_rate;
    unsigned char picture_start;
    unsigned char audio_frame_start;
    unsigned int audio_bytes;
    unsigned int video_bytes;

    unsigned int nsec_a=0;
    unsigned int nsec_v=0;
    unsigned int nsec_p=0;

    unsigned char* index;
	
    Timecode_struc SCR_audio_delay;
    Timecode_struc SCR_video_delay;
    Timecode_struc current_SCR;
    Timecode_struc audio_next_SCR;
    Timecode_struc video_next_SCR;

    Buffer_struc video_buffer;
    Buffer_struc audio_buffer;

    Pack_struc 		pack;
    Sys_header_struc 	sys_header;
    Sector_struc 	sector;

    unsigned long sector_size;
    unsigned long min_packet_data;
    unsigned long max_packet_data;
    unsigned long packets_per_pack;
    unsigned long audio_buffer_size;
    unsigned long video_buffer_size;

    unsigned long write_pack;
    unsigned char marker_pack;
    unsigned long packet_data_size;
    unsigned char verbose;

  

    if (which_streams & STREAMS_VIDEO) istream_v = fopen (video_file, "rb");
    if (which_streams & STREAMS_AUDIO) istream_a = fopen (audio_file, "rb");
    if (which_streams & STREAMS_VIDEO) vunits_info = fopen (video_units, "rb");
    if (which_streams & STREAMS_AUDIO) aunits_info = fopen (audio_units, "rb");
    ostream	= fopen (multi_file, "wb");

  
    picture_start     = FALSE;
    audio_frame_start = FALSE;
    empty_vaunit_struc (&video_au);
    empty_aaunit_struc (&audio_au);

    if (which_streams & STREAMS_AUDIO) {
	fread (&audio_au, sizeof(Aaunit_struc), 1, aunits_info);
	audio_frame_start = TRUE;
    }
    if (which_streams & STREAMS_VIDEO) {
	fread (&video_au, sizeof(Vaunit_struc), 1, vunits_info);
	picture_start = TRUE;
    }


		if(vcd_parm)
			sector_size = 2324;
		else
			sector_size = 2048;
   
	packets_per_pack = 1;
	if(vcd_parm)
		video_buffer_size = 46;
	else
		video_buffer_size = 40;
	audio_buffer_size = 4;

    write_pack = packets_per_pack;
    video_buffer_size *= 1024;
    audio_buffer_size *= 1024;
    min_packet_data = sector_size - PACK_HEADER_SIZE - SYS_HEADER_SIZE -
	PACKET_HEADER_SIZE - AFTER_PACKET_LENGTH;
    max_packet_data = sector_size - PACKET_HEADER_SIZE - AFTER_PACKET_LENGTH;


     if (which_streams != STREAMS_BOTH) { 
 	min_packet_data += 3; 
     } 	


    init_buffer_struc (&video_buffer,video_buffer_size);
    init_buffer_struc (&audio_buffer,audio_buffer_size);


    if (which_streams & STREAMS_VIDEO) {
	if (video_info->bit_rate > video_info->comp_bit_rate)
	    video_rate = video_info->bit_rate * 50;
	else
	    video_rate = video_info->comp_bit_rate * 50;
    }
    if (which_streams & STREAMS_AUDIO)
	audio_rate = bitrate_index[3-audio_info->layer][audio_info->bit_rate]*128;

    data_rate = video_rate + audio_rate;

    dmux_rate =  ceil((double)(data_rate) *
		 ((double)(sector_size)/(double)(min_packet_data) +
		 ((double)(sector_size)/(double)(max_packet_data) *
		 (double)(packets_per_pack-1.))) / (double)(packets_per_pack) );
    data_rate = (unsigned int)ceil(dmux_rate/50.)*50;

	if(vcd_parm)
		dmux_rate = 176400;
	sectors_delay = 8;
	video_delay_ms = 0;
	audio_delay_ms = 0;

    video_delay = (double)video_delay_ms*(double)(CLOCKS/1000);
    audio_delay = (double)audio_delay_ms*(double)(CLOCKS/1000);

	verbose=FALSE;

#ifdef TIMER
    gettimeofday (&tp_global_start,NULL);
#endif

	if (vcd_parm)
		mux_rate = 3528;
	else
		mux_rate = (unsigned int)ceil(dmux_rate/50.);

    dmux_rate= mux_rate * 50.;

	if (vcd_parm)
		delay = 400.0 * (CLOCKS/1000.0);
	else
		delay = ((double)sectors_delay +
			ceil((double)video_au.length/(double)min_packet_data)  +
			ceil((double)audio_au.length/(double)min_packet_data )) *
			(double)sector_size/dmux_rate*(double)CLOCKS;

    audio_delay += delay;
    video_delay += delay;

    make_timecode (audio_delay, &SCR_audio_delay);
    make_timecode (video_delay, &SCR_video_delay);

    add_to_timecode	(&SCR_video_delay, &video_au.DTS);
    add_to_timecode	(&SCR_video_delay, &video_au.PTS);
    add_to_timecode	(&SCR_audio_delay, &audio_au.PTS);

	if (vcd_parm)
	{
		bytes_output = 70560; /* 400 ms worth */
		clock_cycles = 10440000.0; /* 400 - 13.3333333 ms */
	}
	else
		bytes_output = 0;


    while ((video_au.length + audio_au.length) > 0)
    {

	if (write_pack-- == packets_per_pack) 
	{
	    marker_pack = TRUE;
	    packet_data_size = min_packet_data;
	} else 
	{
	    marker_pack = FALSE;
	    packet_data_size = max_packet_data;
	}

	if (write_pack == 0) write_pack = packets_per_pack;

	audio_bytes = (audio_au.length/min_packet_data)*sector_size +
		(audio_au.length%min_packet_data)+(sector_size-min_packet_data);
	video_bytes = (video_au.length/min_packet_data)*sector_size +
		(video_au.length%min_packet_data)+(sector_size-min_packet_data);


	if (vcd_parm)
		clock_cycles += 360000.0;
    else
		clock_cycles = (double)(bytes_output+LAST_SCR_BYTE_IN_PACK)*
			CLOCKS/dmux_rate;

	  audio_next_clock_cycles = (double)(bytes_output+sector_size+
		audio_bytes)/dmux_rate*CLOCKS;
	video_next_clock_cycles = (double)(bytes_output+sector_size+
		video_bytes)/dmux_rate*CLOCKS;

	make_timecode (clock_cycles, &current_SCR);
	make_timecode (audio_next_clock_cycles, &audio_next_SCR);
	make_timecode (video_next_clock_cycles, &video_next_SCR);

	if (which_streams & STREAMS_AUDIO) buffer_clean (&audio_buffer, &current_SCR);
	if (which_streams & STREAMS_VIDEO) buffer_clean (&video_buffer, &current_SCR);


	if ( (buffer_space (&video_buffer) >= packet_data_size)
	     && (video_au.length>0)
	     && ((comp_timecode (&audio_next_SCR, &audio_au.PTS)) ||
		 (audio_au.length==0) ))
	{
	    output_video (&current_SCR, &SCR_video_delay, vunits_info,
		istream_v, ostream, &pack, &sys_header, &sector,
		&video_buffer, &video_au, &picture_start,
		&bytes_output, mux_rate, audio_buffer_size, video_buffer_size,
		packet_data_size, marker_pack, which_streams);

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	}


	else if ( (buffer_space (&audio_buffer) >= packet_data_size)
		  && (audio_au.length>0)
		  && ((comp_timecode (&video_next_SCR, &video_au.DTS)) ||
		      (video_au.length==0) ))
	{

	    output_audio (&current_SCR, &SCR_audio_delay, aunits_info,
		istream_a, ostream, &pack, &sys_header, &sector,
		&audio_buffer, &audio_au, &audio_frame_start,
		&bytes_output, mux_rate, audio_buffer_size, video_buffer_size,
		packet_data_size, marker_pack, which_streams);

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	}


	else if ( (buffer_space (&audio_buffer) >= packet_data_size)
		  && (audio_au.length>0)
		  &! comp_timecode (&audio_next_SCR, &audio_au.PTS))
	{
	    output_audio (&current_SCR, &SCR_audio_delay, aunits_info,
		istream_a, ostream, &pack, &sys_header, &sector,
		&audio_buffer, &audio_au, &audio_frame_start,
		&bytes_output, mux_rate, audio_buffer_size, video_buffer_size,
		packet_data_size, marker_pack, which_streams);
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    status_message (STATUS_AUDIO_TIME_OUT);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	}

	else if ( (buffer_space (&video_buffer) >= packet_data_size)
		   && (video_au.length>0)
		   &! comp_timecode (&video_next_SCR, &video_au.DTS))
	{
	    output_video (&current_SCR, &SCR_video_delay, vunits_info,
		istream_v, ostream, &pack, &sys_header, &sector,
		&video_buffer, &video_au, &picture_start,
		&bytes_output, mux_rate, audio_buffer_size, video_buffer_size,
		packet_data_size, marker_pack, which_streams);

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    status_message (STATUS_VIDEO_TIME_OUT);

#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	}


	else
	{
	    output_padding (&current_SCR, ostream, &pack, &sys_header,
		&sector, &bytes_output, mux_rate, audio_buffer_size, 
		video_buffer_size,packet_data_size, marker_pack, which_streams);

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	}
    }


    index = sector.buf;

    *(index++) = (unsigned char)((ISO11172_END)>>24);
    *(index++) = (unsigned char)((ISO11172_END & 0x00ff0000)>>16);
    *(index++) = (unsigned char)((ISO11172_END & 0x0000ff00)>>8);
    *(index++) = (unsigned char)(ISO11172_END & 0x000000ff);

    fwrite (sector.buf, sizeof (unsigned char), 4, ostream);
    bytes_output += 4;

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
    if (!verbose) printf ("\n");
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
    /* close all In- and Outputfiles				*/

    fclose (ostream);

    if (which_streams & STREAMS_AUDIO) fclose (aunits_info);
    if (which_streams & STREAMS_VIDEO) fclose (vunits_info);
    if (which_streams & STREAMS_AUDIO) fclose (istream_a);
    if (which_streams & STREAMS_VIDEO) fclose (istream_v);

    /* delete tmp files	*/

    if (which_streams & STREAMS_VIDEO) unlink (video_units);
    if (which_streams & STREAMS_AUDIO) unlink (audio_units); 


#ifdef TIMER
    gettimeofday (&tp_global_end, NULL);
    global_sec = 10*(tp_global_end.tv_sec - tp_global_start.tv_sec);
    global_usec= 10*(tp_global_end.tv_usec - tp_global_start.tv_usec);
    global_sec += (global_usec / 100000);
    total_sec *= 10;
    total_sec  += (total_usec  / 100000);

    printf ("Timing global: %10.1f secs\n",(float)global_sec/10.);
    printf ("Timing IO    : %10.1f secs\n",(float)total_sec/10.);
#endif
    
}


/******************************************************************
	Next_Video_Access_Unit
	从临时文件中获取下一个存储单元的信息

******************************************************************/

void next_video_access_unit (buffer, video_au, bytes_left, vunits_info,
			picture_start, SCR_delay)
Buffer_struc *buffer;
Vaunit_struc *video_au;
unsigned int *bytes_left;
FILE *vunits_info;
unsigned char *picture_start;
Timecode_struc *SCR_delay;

{

	int i;

	if (*bytes_left == 0)
	    return;

	while (video_au->length < *bytes_left)
	{
	    queue_buffer (buffer, video_au->length, &video_au->DTS);
	    *bytes_left -= video_au->length;
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    i=fread (video_au, sizeof(Vaunit_struc), 1, vunits_info);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	    if (i != 1)
	    {
		empty_vaunit_struc (video_au);
		//status_message(STATUS_VIDEO_END);
		return;
	    }
	    *picture_start = TRUE;
	    add_to_timecode (SCR_delay, &video_au->DTS);
	    add_to_timecode (SCR_delay, &video_au->PTS);
	};

	if (video_au->length > *bytes_left)
	{
	    queue_buffer (buffer, *bytes_left, &video_au->DTS);
	    video_au->length -= *bytes_left;
	    *picture_start = FALSE;
	} else
	if (video_au->length == *bytes_left)
	{
	    queue_buffer (buffer, *bytes_left, &video_au->DTS);
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    i=fread (video_au, sizeof(Vaunit_struc), 1, vunits_info);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	    if (i != 1)
	    {
		empty_vaunit_struc (video_au);
		//status_message(STATUS_VIDEO_END);
		return;
	    }
	    *picture_start = TRUE;
	    add_to_timecode (SCR_delay, &video_au->DTS);
	    add_to_timecode (SCR_delay, &video_au->PTS);
	};

}


/******************************************************************
	Output_Video
	从视频流中产生Pack/Sys_Header/Packet信息
******************************************************************/

void output_video (SCR, SCR_delay, vunits_info, istream_v, ostream,
		   pack, sys_header, sector, buffer, video_au,
		   picture_start, bytes_output, mux_rate,
		   audio_buffer_size, video_buffer_size,
		   packet_data_size, marker_pack, which_streams)

Timecode_struc *SCR;
Timecode_struc *SCR_delay;
FILE *vunits_info;
FILE *istream_v;
FILE *ostream;
Pack_struc *pack;
Sys_header_struc *sys_header;
Sector_struc *sector;
Buffer_struc *buffer;
Vaunit_struc *video_au;
unsigned char *picture_start;
unsigned int  *bytes_output;
unsigned int mux_rate;
unsigned long audio_buffer_size;
unsigned long video_buffer_size;
unsigned long packet_data_size;
unsigned char marker_pack;
unsigned int which_streams;

{

    unsigned int bytes_left;
    unsigned int temp;
    Pack_struc *pack_ptr;
    Sys_header_struc *sys_header_ptr;
    unsigned char timestamps;


    if (marker_pack)
    {
    	create_pack (pack, SCR, mux_rate);
    	create_sys_header (sys_header, mux_rate, 1, 1, 1, 1, 1, 1,
			AUDIO_STR_0, 0, audio_buffer_size/128,
			VIDEO_STR_0, 1, video_buffer_size/1024, which_streams );
	pack_ptr = pack;
	sys_header_ptr = sys_header;
    } else
    {
	pack_ptr = NULL;
	sys_header_ptr = NULL;
    }

    if (*picture_start)
    {
	if (video_au->type == BFRAME)
	    timestamps=TIMESTAMPS_PTS;
	else
	    timestamps=TIMESTAMPS_PTS_DTS;

	create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
			TRUE, &video_au->PTS, &video_au->DTS,
		        timestamps, which_streams );

	bytes_left = sector->length_of_packet_data;

	next_video_access_unit (buffer, video_au, &bytes_left, vunits_info,
				picture_start, SCR_delay);

    }
    else if (!(*picture_start) && (video_au->length >= packet_data_size))
    {
	create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
			TRUE, NULL, NULL,
			TIMESTAMPS_NO, which_streams );

	bytes_left = sector->length_of_packet_data;

	next_video_access_unit (buffer, video_au, &bytes_left, vunits_info,
				picture_start, SCR_delay);

    }

    else if (!(*picture_start) && (video_au->length < packet_data_size))
    {
	temp = video_au->length;
	queue_buffer (buffer, video_au->length, &video_au->DTS);


#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	if (fread (video_au, sizeof(Vaunit_struc), 1, vunits_info)==1)
	{
	    if (video_au->type == BFRAME)
		timestamps=TIMESTAMPS_PTS;
	    else
		timestamps=TIMESTAMPS_PTS_DTS;

	    *picture_start = TRUE;
	    add_to_timecode (SCR_delay, &video_au->DTS);
	    add_to_timecode (SCR_delay, &video_au->PTS);
	    create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
			TRUE, &video_au->PTS, &video_au->DTS,
			timestamps, which_streams );
	bytes_left = sector->length_of_packet_data - temp;

	next_video_access_unit (buffer, video_au, &bytes_left, vunits_info,
				picture_start, SCR_delay);
	} else
	{
	    empty_vaunit_struc (video_au);
	    create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
			TRUE, NULL, NULL,
			TIMESTAMPS_NO, which_streams );
	};
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif

    }


#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
    fwrite (sector->buf, sector->length_of_sector, 1, ostream);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
             total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
    *bytes_output += sector->length_of_sector;
	
}


/******************************************************************
	Next_Audio_Access_Unit
	从临时文件中获取存储单元的信息

******************************************************************/

void next_audio_access_unit (buffer, audio_au, bytes_left, aunits_info,
			audio_frame_start, SCR_delay)
Buffer_struc *buffer;
Aaunit_struc *audio_au;
unsigned int *bytes_left;
FILE *aunits_info;
unsigned char *audio_frame_start;
Timecode_struc *SCR_delay;

{

	int i;

	if (*bytes_left == 0)
	    return;

	while (audio_au->length < *bytes_left)
	{
	    queue_buffer (buffer, audio_au->length, &audio_au->PTS);
	    *bytes_left -= audio_au->length;
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    i=fread (audio_au, sizeof(Aaunit_struc), 1, aunits_info);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	    if (i != 1)
	    {
		empty_aaunit_struc (audio_au);
		//status_message(STATUS_AUDIO_END);
		return;
	    }
	    *audio_frame_start = TRUE;
	    add_to_timecode (SCR_delay, &audio_au->PTS);
	};

	if (audio_au->length > *bytes_left)
	{
	    queue_buffer (buffer, *bytes_left, &audio_au->PTS);
	    audio_au->length -= *bytes_left;
	    *audio_frame_start = FALSE;
	} else
	if (audio_au->length == *bytes_left)
	{
	    queue_buffer (buffer, *bytes_left, &audio_au->PTS);
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	    i=fread (audio_au, sizeof(Aaunit_struc), 1, aunits_info);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
	    if (i != 1)
	    {
		empty_aaunit_struc (audio_au);
		//status_message(STATUS_AUDIO_END);
		return;
	    }
	    *audio_frame_start = TRUE;
	    add_to_timecode (SCR_delay, &audio_au->PTS);
	};

}

/******************************************************************
	Output_Audio
	从音频流中获取Pack/Sys Header/Packet信息
	
******************************************************************/

void output_audio (SCR, SCR_delay, aunits_info, istream_a, ostream,
		   pack, sys_header, sector, buffer, audio_au,
		   audio_frame_start, bytes_output, mux_rate,
		   audio_buffer_size, video_buffer_size,
		   packet_data_size, marker_pack, which_streams)

Timecode_struc *SCR;
Timecode_struc *SCR_delay;
FILE *aunits_info;
FILE *istream_a;
FILE *ostream;
Pack_struc *pack;
Sys_header_struc *sys_header;
Sector_struc *sector;
Buffer_struc *buffer;
Aaunit_struc *audio_au;
unsigned char *audio_frame_start;
unsigned int  *bytes_output;
unsigned int mux_rate;
unsigned long audio_buffer_size;
unsigned long video_buffer_size;
unsigned long packet_data_size;
unsigned char marker_pack;
unsigned int which_streams;

{

    unsigned int bytes_left;
    unsigned int temp;
    Pack_struc *pack_ptr;
    Sys_header_struc *sys_header_ptr;

    if (marker_pack)
    {
    	create_pack (pack, SCR, mux_rate);
    	create_sys_header (sys_header, mux_rate, 1, 1, 1, 1, 1, 1,
			AUDIO_STR_0, 0, audio_buffer_size/128,
			VIDEO_STR_0, 1, video_buffer_size/1024, which_streams );
	pack_ptr = pack;
	sys_header_ptr = sys_header;
    }
    else
    {
	pack_ptr = NULL;
	sys_header_ptr = NULL;
    }

    if (*audio_frame_start)
    {
	create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
			TRUE, &audio_au->PTS, NULL,
			TIMESTAMPS_PTS, which_streams);

	bytes_left = sector->length_of_packet_data;

	next_audio_access_unit (buffer, audio_au, &bytes_left, aunits_info,
				audio_frame_start, SCR_delay);
    }

    else if (!(*audio_frame_start) && (audio_au->length >= packet_data_size))
    {
	create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
			TRUE, NULL, NULL,
			TIMESTAMPS_NO, which_streams );

	bytes_left = sector->length_of_packet_data;

	next_audio_access_unit (buffer, audio_au, &bytes_left, aunits_info,
				audio_frame_start, SCR_delay);
    }

    else if (!(*audio_frame_start) && (audio_au->length < packet_data_size))
    {
	temp = audio_au->length;
	queue_buffer (buffer, audio_au->length, &audio_au->PTS);


#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
	if (fread (audio_au, sizeof(Aaunit_struc), 1, aunits_info)==1)
	{
	    *audio_frame_start = TRUE;
	    add_to_timecode (SCR_delay, &audio_au->PTS);
	    create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
			TRUE, &audio_au->PTS, NULL,
			TIMESTAMPS_PTS, which_streams );

	bytes_left = sector->length_of_packet_data - temp;

	next_audio_access_unit (buffer, audio_au, &bytes_left, aunits_info,
				audio_frame_start, SCR_delay);
	} else
	{
	    //status_message(STATUS_AUDIO_END);
	    empty_aaunit_struc (audio_au);
	    create_sector (sector, pack_ptr, sys_header_ptr,
			packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
			istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
			TRUE, NULL, NULL,
			TIMESTAMPS_NO, which_streams );
	};
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif


    }
#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
    fwrite (sector->buf, sector->length_of_sector, 1, ostream);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
    *bytes_output += sector->length_of_sector;

	
}

/******************************************************************
	Output_Padding
	为填充流产生Pack/Sys Header/Packet信息
******************************************************************/

void output_padding (SCR,  ostream,
		   pack, sys_header, sector, bytes_output, mux_rate,
		   audio_buffer_size, video_buffer_size,
		   packet_data_size, marker_pack, which_streams)

Timecode_struc *SCR;
FILE *ostream;
Pack_struc *pack;
Sys_header_struc *sys_header;
Sector_struc *sector;
unsigned int  *bytes_output;
unsigned int mux_rate;
unsigned long audio_buffer_size;
unsigned long video_buffer_size;
unsigned long packet_data_size;
unsigned char marker_pack;
unsigned int which_streams;

{
    //unsigned int temp;
    Pack_struc *pack_ptr;
    Sys_header_struc *sys_header_ptr;

    if (marker_pack)
    {
    	create_pack (pack, SCR, mux_rate);

    	create_sys_header (sys_header, mux_rate, 1, 1, 1, 1, 1, 1,
			AUDIO_STR_0, 0, audio_buffer_size/128,
			VIDEO_STR_0, 1, video_buffer_size/1024, which_streams );
	pack_ptr = pack;
	sys_header_ptr = sys_header;
    }
    else
    {
	pack_ptr = NULL;
	sys_header_ptr = NULL;
    }

    create_sector (sector, pack_ptr, sys_header_ptr,
		packet_data_size+PACKET_HEADER_SIZE+AFTER_PACKET_LENGTH,
		NULL, PADDING_STR, 0, 0,
		FALSE, NULL, NULL,
		TIMESTAMPS_NO, which_streams );

#ifdef TIMER
            gettimeofday (&tp_start,NULL);
#endif 
    fwrite (sector->buf, sector->length_of_sector*sizeof (unsigned char), 1,
	    ostream);
#ifdef TIMER
            gettimeofday (&tp_end,NULL);
            total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
            total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
    *bytes_output += sector->length_of_sector;
	
}

