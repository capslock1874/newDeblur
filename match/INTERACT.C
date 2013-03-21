
#include <windows.h>
#include <vfw.h>

#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "mplex.h"


int open_file(name, bytes)			
char *name;
unsigned int *bytes;				
{
    FILE* datei;

    datei=fopen (name, "rwb");
    if (datei==NULL)
    {	
	printf("File %s not found.\n", name);
	return (TRUE);
    }
    fseek (datei, 0, 2);
    *bytes = ftell(datei);
    fclose(datei);
    return (FALSE);
}


/*************************************************************************
	ask_continue
	在显示视频流信息后，检查是否需要继续计算


*************************************************************************/

void ask_continue ()
{
    char input[20];

    printf ("\nContinue processing (y/n) : ");
    do gets (input);
    while (input[0]!='N'&&input[0]!='n'&&input[0]!='y'&&input[0]!='Y');

    if (input[0]=='N' || input[0]=='n')
    {
	printf ("\nStop processing.\n\n");
	exit (0);

    }

}


unsigned char ask_verbose ()
{
    char input[20];

    printf ("\nVery verbose mode (y/n) : ");
    do gets (input);
    while (input[0]!='N'&&input[0]!='n'&&input[0]!='y'&&input[0]!='Y');

    if (input[0]=='N' || input[0]=='n') return (FALSE); else return (TRUE);
}



void status_info (nsectors_a, nsectors_v, nsectors_p, nbytes, 
		  buf_v, buf_a,verbose)
unsigned int nsectors_a;
unsigned int nsectors_v;
unsigned int nsectors_p;
unsigned int nbytes;
unsigned int buf_v;
unsigned int buf_a;
unsigned char verbose;
{
    printf ("| %7d | %7d |",nsectors_a,nsectors_v);
    printf (" %7d | %11d |",nsectors_p,nbytes);
    printf (" %6d | %6d |",buf_a,buf_v);
    printf ((verbose?"\n":"\r"));
    fflush (stdout);
}

void status_header ()
{
    status_footer();
    printf("|  Audio  |  Video  | Padding | Bytes  MPEG | Audio  | Video  |\n");
    printf("| Sectors | Sectors | Sectors | System File | Buffer | Buffer |\n");
    status_footer();
}


void status_message (what)
unsigned char what;
{
  switch (what)
  {
  case STATUS_AUDIO_END:
  //printf("\n|file  end|         |         |             |        |        |\n");
	  audio_end_early++;
  break;
  case STATUS_AUDIO_TIME_OUT:
  //printf("\n|time  out|         |         |             |        |        |\n");
	  audio_time_out++;
  break;
  case STATUS_VIDEO_END:
  //printf("\n|         |file  end|         |             |        |        |\n");
	  video_end_early++;
  break;
  case STATUS_VIDEO_TIME_OUT:
  //printf("\n|         |time  out|         |             |        |        |\n");
	  video_time_out++;
  break;
  default:
	  fprintf(stderr, "\nInvalid mplex status message\n");
  }
}

void status_footer ()
{
  printf("+---------+---------+---------+-------------+--------+--------+\n");
}
