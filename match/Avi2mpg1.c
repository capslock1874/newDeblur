//
// avi2mpg1 - 
// 产生编码器的命令行窗口，即是avi2mpg1的主函数
//


#include <stdio.h>
#include <math.h>
#include "avi2mpg1.h"
#define GLOBAL          
#include "global.h"
#include "video.h"

//函数声明

void usage(void);
int ValidBitrate(unsigned int, unsigned int);
void StripExtension(char *, char *);
void StripPath(char *, char *);
int mplex(char *, char *, char *);
void videocd();

unsigned int pixsz;


int main(int argc, char **argv)
{
	int i=0,j=0, err=0;
	char avifilenames[MAX_AVI_FILES][_MAX_PATH], basefilename[_MAX_FNAME] = "", mpegfilename[_MAX_PATH] = "";
	char VideoTempFile[_MAX_FNAME] = "", AudioTempFile[_MAX_FNAME] = "", basepathname[_MAX_PATH];
	char testfilename[_MAX_PATH] = "", intfilename[_MAX_FNAME];
    AVIFILEINFO thisPfi;
	float fps;
	int sys_bit_specd = 0, sys_byte_specd = 0;
    PAVISTREAM pVideoStream;

	time_t current_time, el_time;

	bad_frame_count = 0;
	bad_audio_count = 0;
    numAviFiles = 0;

	vbv_size = 20;
	m_search_size = 0;
	crop_size = 0;
	forced_frame_rate = 0;

	cpu_MMX = 0;
	cpu_3DNow = 0;

	if(argc < 2)
	{
		usage();
	}

	TestCPU();

	system_byterate_parm = 150;
	audio_bitrate_parm = 128;
	audio_layer_parm = 2;
	joint_stereo_parm = 0;
	vcd_parm = 0;
	fake_bad_frames = 0;
	video_only = 0;
    use_image_noise_reduction = 0;
	fastQuantization = 0;
	fastMotionCompensationLevel = 0;

	if (cpu_MMX)
		doublePrecision = 0;
	else
	    doublePrecision = 1;



	while((++i < argc)&&(err == 0))
	{
		char c, *token, *arg, *nextArg, *nextArg2;
		int  argUsed;
 
		token = argv[i];
		if(*token++ == '-')
		{ 
			if(i+1 < argc)
				nextArg = argv[i+1];
			else
				nextArg = "";
			argUsed = 0;
			while(c = *token++)
			{
				if(*token /* NumericQ(token) */)
					arg = token;
				else
					arg = nextArg;
				switch(tolower(c))
				{
					case 'l':
						audio_layer_parm = atoi(arg);
						argUsed = 1;
						if((audio_layer_parm < 1)||(audio_layer_parm > 2))
						{
							fprintf(stderr,"\n-l layer must be 1 or 2, not %s\n", arg);
							err = 1;
						}
					break;

					case 'b':        
						argUsed = 1;
						system_byterate_parm = atoi(arg);
						sys_byte_specd = 1;
						if(sys_bit_specd)
						{
							fprintf(stderr, "\n can not specify both -b and -s!\n");
							err = 1;
						}
						if((system_byterate_parm < 10)||(system_byterate_parm > 5000))
						{
							fprintf(stderr, 
								"\n-b system byterate (K bytes/s) must be from 10 to 5000, not %s\n", arg);
							err = 1;
						}
					break;

					case 's':
						argUsed = 1;
						system_byterate_parm = atoi(arg)/8;
						sys_bit_specd = 1;
						if(sys_byte_specd)
						{
							fprintf(stderr, "\n can not specify both -b and -s!\n");
							err = 1;
						}
						if((system_byterate_parm < 10)||(atoi(arg) > 40000))
						{
							fprintf(stderr, "\n-s system bitrate (K bits/s) must be from 80 to 40000, not %s\n", arg);
							err = 1;
						}
					break;

					case 'a':
						argUsed = 1;
						audio_bitrate_parm = atoi(arg);
					break;

                    case 'r':
                        use_image_noise_reduction = 1;
                    break;

					case 'j':
						joint_stereo_parm = 1;
					break;

					case 'v':
						//vcd_parm = 1;
						videocd();
					break;

					case 'p':
						argUsed = 1;
						strcpy(video_param_filename, arg);
						use_v_param_file = 1;
					break;

					case 'e':
						fake_bad_frames = 1;
					break;

					case 'n':
						video_only = 1;
					break;

					case 'y':
						argUsed = 1;
						vbv_size = atoi(arg);
						if(vbv_size < 1)
						{
							fprintf(stderr, "\n vbv_buffer_size must be in range 1 <-> 1023.\n");
							err = 1;
						}
					break;

					case 'm':
						argUsed = 1;
						m_search_size = atoi(arg);
						if((m_search_size < 0 )||(m_search_size > 4))
						{
							fprintf(stderr, "\n -m search option must be from 0 to 4!\n");
							err = 1;
						}
					break;

					case 'c':
						if(i+2 < argc)
						{
							nextArg2 = argv[i+2];
							i++;
						}
						else
							nextArg2 = "";

						crop_size = 1;
						argUsed =1;
						crop_horz = atoi(arg);
						if((crop_horz < 16)||(crop_horz>4095))
						{
							fprintf(stderr, "\n -c horizontal size must be from 16 to 4095!\n");
							err = 1;
						}
						crop_vert = atoi(nextArg2);
						if((crop_vert < 16)||(crop_vert>4095))
						{
							fprintf(stderr, "\n -c vertical size must be from 16 to 4095!\n");
							err=1;
						}
					break;

					case 'f':
						argUsed = 1;
						forced_frame_rate = atoi(arg);
						if((forced_frame_rate < 1)||(forced_frame_rate > 5))
						{
							fprintf(stderr, "\n -f frame rate code must be from 1 to 5!\n");
							err=1;
						}
					break;

					case 'x': 
						cpu_MMX = 0;
						cpu_3DNow = 0;
						doublePrecision = 1;
					break;

					case 'd': 
						doublePrecision = 1;
					break;

					case 'q':
						fastQuantization = 1;
					break;

					case 't':
						argUsed = 1;
						fastMotionCompensationLevel = atoi(arg);
						if(fastMotionCompensationLevel < 0 || fastMotionCompensationLevel > 3)
						{
							fprintf(stderr, "\n -t level must be from 0 to 3!\n");
							err = 1;
						}
					break;

					case 'h':
						argUsed = 1;
						wr_seqh2gop = atoi(arg); 
					break;

					default:
						fprintf(stderr,"\nunrecognized option %c\n", c);
						err = 1;
					break;
				}
				if(argUsed)
				{
					if(arg == token)
						token = "";   
					else
						++i;          
					arg = ""; argUsed = 0;
				}
			}
		}
		else 
		{
			if((i != argc-1) || (numAviFiles==0))
            {
                if (numAviFiles < MAX_AVI_FILES)
				    strcpy(avifilenames[numAviFiles++], argv[i]);
                else
                    usage();
            }
			else
				strcpy(mpegfilename, argv[i]);
		}
	}

	if (cpu_MMX) 
		printf("MMX    capable CPU detected, using MMX extensions.\n");
	if (cpu_3DNow) 
		printf("3DNow! capable CPU detected, using 3DNow! extensions.\n");


	if(vcd_parm)
	{
		system_byterate_parm = 1158000/8/1024;
		audio_bitrate_parm = 224;
		audio_layer_parm = 2;
		joint_stereo_parm = 0;
	}

	if(!ValidBitrate(audio_layer_parm, audio_bitrate_parm))
	{//检查对应音频比特率对应层面是否合法
		fprintf(stderr, "\n-a bitrate %i not legal for layer %i", 
			audio_bitrate_parm, audio_layer_parm);
		err = 1;
	}
	if(vcd_parm&&video_only)
	{//vcd和video_only不能同时有效
		fprintf(stderr, "\n can not specify both -v and -n!\n");
		err = 1;
	}

	if(vcd_parm&&forced_frame_rate)
	{//vcd和forced_frame_rate不能同时有效
		fprintf(stderr, "\n can not specify both -v and -f!\n");
		err = 1;
	}

	if(forced_frame_rate)
		video_only = 1;

	if(err)
	{
		fprintf(stderr, "\n type avi2mpg1 <enter> for usage.\n");
		exit(6);
	}

	if(numAviFiles == 0)
		usage();  

	AVIFileInit();
    

    for(j=0;j<numAviFiles;j++)
    {
        StripExtension(basepathname, avifilenames[j]);//取出的文件名不带扩展名
 
	    if(strlen(avifilenames[j]) == strlen(basepathname))//给参数中不加扩展名的加上.avi扩展名
		    strcat(avifilenames[j], ".avi"); 

	   

		/*AVIFileOpen()函数和下面和上面类似的函数，
		都是在c:\Program Files\Microsoft Visual Studio\VC98\Include\VFW.H定义的*/
	    hResult = AVIFileOpen(&pfile, avifilenames[j], OF_READ, 0L);

	    if(hResult)
	    {
		    fprintf(stderr, "\ncould not open filename %s!\n", 
		    	avifilenames[j]);
		    exit(7);
	    }

	    hResult = AVIFileInfo(pfile, &thisPfi, sizeof(AVIFILEINFO));
		//pfile为AVIFileOpen()得到的指针，将avi的头部信息存到thisPfi
	    if(hResult)
	    {
		    fprintf(stderr, "\n%s does not appear to be an AVI file!\n", 
		    	avifilenames[j]);
		    exit(8);
	    }

        if (j==0)
            masterPfi=thisPfi;

	    hResult = AVIFileGetStream(pfile, &pVideoStream, streamtypeVIDEO, 0);
	    if(hResult)
	    {
		    fprintf(stderr, "\ncould not retrieve video stream from %s!\n", 
			    avifilenames[j]);
		    exit(31);
	    }

	    pget[j] = AVIStreamGetFrameOpen(pVideoStream, NULL);

	    fps = (float)thisPfi.dwRate/(float)thisPfi.dwScale;
	    fprintf(stderr, "%s file details:\nWidth = %u, Height = %u, FPS = %4.2f", 
		    avifilenames[j], thisPfi.dwWidth, thisPfi.dwHeight, fps);
		//输出对应avi文件图像的宽，高，fps（每秒传输帧数）=速率/时间尺度，	    
	    lpbi = AVIStreamGetFrame(pget[j], 0);
	    if(!lpbi)
	    {
		    fprintf(stderr, "\ncould not retrieve video details for %s!",avifilenames[j]);
		    fprintf(stderr, "\nyou probably do not have a codec for this compression scheme!");
		    fprintf(stderr, "\nor avi file may be corrupt!\n");
	    	exit(9);
	    }
	    last_good_video_frame = 0;

	 

	    pixsz = lpbi->biBitCount;
	    if((pixsz!=8)&&(pixsz!=16)&&(pixsz!=24)&&(pixsz!=32))
		{//仅支持以上几种图像
			fprintf(stderr, "\ncan only handle 8 bit palletized, 16, 24, or 32 bit RGB video!,");
		    fprintf(stderr, "\nthis file is %i bits!\n", pixsz);
		    exit(10);
	    }

	    fprintf(stderr, ", Bits per pixel = %u", pixsz);
        //输出每像素占用的位数
	    if (j==0)
        {
			//图像高度和宽度都不能超过4095
            if(thisPfi.dwWidth>4095)
	        {
		        fprintf(stderr, "\nHorizontal width must < 4096!");
	    	    err = 1;
	        }
	        if(thisPfi.dwHeight>4095)
	        {
		        fprintf(stderr, "\nVerical height must be < 4096!");
		        err = 1;
	        }
        }
        else
        {
			/*多个avi视频帧图像的高度和宽度，还有fps（每秒帧数）必须一致才能进行合并，*/
            if (thisPfi.dwWidth != masterPfi.dwWidth)
            {
                fprintf(stderr, "\nWidth of all AVI files must match!");
                err=1;
            }
            if (thisPfi.dwHeight != masterPfi.dwHeight)
            {
                fprintf(stderr, "\nHeight of all AVI files must match!");
                err=1;
            }
            if (fabs(fps - (float)masterPfi.dwRate/(float)masterPfi.dwScale) > 0.1)
            {
                fprintf(stderr, "\nFrame rate must be the same for all AVI files!");
                err=1;
            }
        }

	    if(err)
			exit(11);//因为视频格式不符合要求，程序结束
		/*打印出流的长度dwLength,也就是总的帧数，通过dwLength/fps得出总的播放时间*/
	    fprintf(stderr, "\nTotal Video Frames = %u, ", thisPfi.dwLength);
	    fprintf(stderr, "Length = %u sec.", 
			    			(unsigned int)thisPfi.dwLength/(int)(fps+.5));

        if (j==0)
            nextFileFrames[j]=thisPfi.dwLength;
        else
            nextFileFrames[j]=thisPfi.dwLength+nextFileFrames[j-1];


	    if (lpbi->biCompression != BI_RGB)
	    {//压缩的退出
		    fprintf(stderr, "\ncan not handle compressed DIBs from codec!\n");
		    err = 1;
	    }

	    if(!video_only)
	    {/*以下是获取音频流的信息*/

		    if (AVIFileGetStream(pfile, &pAudioStreams[j], streamtypeAUDIO, 0))
		    {//音频流的
		    	fprintf(stderr, "\nCould not open avi audio stream!");
		    	fprintf(stderr, "\nIf this avi has no audio, or you want to generate an");
		    	fprintf(stderr, "\nmpeg video only stream, use the -n option.");
		    	exit(12);
	    	}

		    if (AVIStreamInfo(pAudioStreams[j], &pAudioStreamInfo[j], sizeof(pAudioStreamInfo[j])))
		    {
			    fprintf(stderr, "\nCould not retrieve avi audio stream info!");
			    exit(13);
		    }

		    AVIStreamReadFormat(pAudioStreams[j], 0, 0, &length); // get length of format data
		    if(AVIStreamReadFormat(pAudioStreams[j], 0, &pWavFormat, &length))
		    {
			    fprintf(stderr, "\nCould not retrieve avi audio format info!");
			    exit(14);
		    }

			/*显示音频的一些信息，声道数，采样率，和采样位数*/
		    fprintf(stderr, "\nAudio Channels = %i, ", pWavFormat.nChannels);
		    fprintf(stderr, "Sample rate = %6.3f KHz, bits per sample = %u\n",
							    (float)pWavFormat.nSamplesPerSec/1000.0, pWavFormat.wBitsPerSample);

            if (j==0)
                nextFileSamples[j]=pAudioStreamInfo[j].dwLength;
            else
                nextFileSamples[j]=pAudioStreamInfo[j].dwLength+nextFileSamples[j-1];

		    if(vcd_parm&&(pWavFormat.nChannels!=2))
		    {
			    fprintf(stderr, "\n for Video CD audio must be stereo!\n"); 
			    err = 1;
		    }

		    if (pWavFormat.wFormatTag != WAVE_FORMAT_PCM)
		    {//必须得是pcm数据进行压缩
			    fprintf(stderr, "\ninput avi audio must be uncompressed PCM!\n");
			    err = 1;
		    }

		    if ((pWavFormat.nSamplesPerSec != 11025) && (pWavFormat.nSamplesPerSec != 22050) && (pWavFormat.nSamplesPerSec != 44100))
		    {//限制采样率
			    fprintf(stderr, "\ninput avi audio sample rate must = 11.025, 22.05, or 44.1 kHz!\n");
			    err = 1;
		    }
		    if ((pWavFormat.wBitsPerSample != 16) && (pWavFormat.wBitsPerSample != 8))
		    {//限制比特位
			    fprintf(stderr, "\ninput avi audio must be 8 or 16 bit!, this file is %i bits\n", pWavFormat.wBitsPerSample);
			    err = 1;
		    }
	    }
	    else
		    fprintf(stderr, "\n");

    }

	if (err == 1)
		exit(15);//因为音频格式不符合要求而退出程序
    totalFrames=nextFileFrames[numAviFiles-1];
    //得到几个avi文件总的帧数
	StripPath(basefilename, basepathname);//文件的名字，文件的路径

	if(mpegfilename[0] != '\0') 
	{
		StripExtension(testfilename, mpegfilename);
		StripPath(intfilename, testfilename); 
	}
	else 
	{
		StripPath(intfilename, basepathname);//如果mpegfilename中没有参数，那么用最后一个参数作为输出文件名
	}

	if(use_v_param_file)
	{
		// if parameter file used, add .par extension if none supplied
		StripExtension(testfilename, video_param_filename);
		if(strlen(video_param_filename) == strlen(testfilename))
			strcat(video_param_filename, ".par");
	}

	// call avi2m1v to encode video stream
	strcpy(VideoTempFile, intfilename);
	strcat(VideoTempFile, ".m1v");
	fprintf(stderr, "encoding video stream to %s", VideoTempFile);
	avi2m1v(VideoTempFile);//编码视频流，保存在临时文件VideoTempFile中
    for(j=0;j<numAviFiles;j++)
	    AVIStreamGetFrameClose(pget[j]);
	fprintf(stderr, "                                                                              \r");

	// if there were errors during encoding of video, print them
	if(vbv_ovflow!=0)
		fprintf(stderr, "*** WARNING: VBV_BUFFER OVERFLOWED %u TIMES.\n", vbv_ovflow);
	if(vbv_unflow!=0)
		fprintf(stderr, "*** WARNING: VBV_BUFFER UNDERFLOWED %u TIMES.\n", vbv_unflow);
	if(bad_frame_count)
		fprintf(stderr, "*** WARNING: %u CORRUPT VIDEO FRAMES REPLACED BY PREVIOUS GOOD FRAME!\n", bad_frame_count);
	fprintf(stderr, "video encoding complete,");
	time(&current_time);
	el_time = current_time - start_time;
	if(el_time <= 60)
		fprintf(stderr, " elapsed time = %i seconds\n", el_time);
	else if(el_time < 3600)
		fprintf(stderr, " elapsed time = %i minutes\n", (el_time/60)+1);
	else
		fprintf(stderr, " elapsed time = %i hour(s) %i minute(s)\n", el_time/3600, (el_time%3600)/60);
	fprintf(stderr, "average seconds per frame = %5.3f\n", (float)el_time/(float)(totalFrames*frame_repl));

	if(!video_only)
	{
		// call avi2mp2 to encode audio stream

		strcpy(AudioTempFile, intfilename);
		strcat(AudioTempFile, ".mp2");
		fprintf(stderr, "encoding audio stream to %s", AudioTempFile);
		fprintf(stderr, " using bitrate of %u:\n", audio_bitrate_parm*1024);
		avi2mp2(AudioTempFile);
		fprintf(stderr, "\raudio stream encoding complete.\n");
		if(bad_audio_count)
			fprintf(stderr, "***WARNING: %u CORRUPT AUDIO SAMPLES REPLACED BY 0 DATA!\n", bad_audio_count);
	}

	AVIFileRelease(pfile);
	AVIFileExit();

	if(!video_only)
	{
		// check if user supplied outputfile (if any) has an extension,
		// if not, add .mpg
		if(mpegfilename[0] != '\0')
		{
			StripExtension(testfilename, mpegfilename);
			if(strlen(mpegfilename) == strlen(testfilename))
				strcat(mpegfilename, ".mpg");
		}

		// call mplex to create .mpg file
		if(mpegfilename[0] == '\0')
		{
			strcpy(mpegfilename, basefilename); // no output file name supplied,
			strcat(mpegfilename, ".mpg");       // use input filename with .mpg
		}
		fprintf(stderr, "multiplexing video and audio streams to %s:\n", mpegfilename);
		mplex(VideoTempFile, AudioTempFile, mpegfilename);
		fprintf(stderr, "\rmultiplexing complete.\n");
		if(video_end_early)
			fprintf(stderr, "***WARNING: video stream ended early!\n");
		if(audio_end_early)
			fprintf(stderr, "***WARNING: audio stream ended early!\n");
		if(video_time_out)
			fprintf(stderr, "***WARNING: video stream timed out %u times.\n", video_time_out);
		if(audio_time_out)
			fprintf(stderr, "***WARNING: audio strean timed out %u times.\n", audio_time_out);

		// delete temporary files.
		fprintf(stderr, "deleting temporary video and audio files.\n");
	//	DeleteFile(VideoTempFile);
	//	DeleteFile(AudioTempFile);
	}

	return(0);
}


void usage()
{
		fprintf(stderr, "\navi2mpg1 - avi to mpeg-1 encoder version %s", VERSION);
		fprintf(stderr, "\nCopyright (C) 1997, 1998, 1999 by John Schlichther");
		fprintf(stderr, "\navi2mpg1 comes with ABSOLUTELY NO WARRANTY");
		fprintf(stderr, "\nThis is free software, and you are welcome to redistribute it");
		fprintf(stderr, "\nunder certain conditions; see file COPYING for details.");
		fprintf(stderr, "\n\n usage: avi2mpg1 [-options] inputfile.avi [inputfile2.avi ...] [outputfile.mpg]");
		fprintf(stderr, "\n  where:  -b byterate (total stream - default 150 KBytes/s)");
		fprintf(stderr, "\n          -s bitrate (total stream - default 1200 Kbits/s)");
		fprintf(stderr, "\n          -a bitrate (audio - default 128Kbits/s)");
		fprintf(stderr, "\n          -l layer (audio - default 2)");
		fprintf(stderr, "\n          -j  (joint stereo)");
		//fprintf(stderr, "\n          -v  (use Video-cd parameters - overides -b, -s, -a, -l, -n, -c)");
		fprintf(stderr, "\n          -p filename.par  video encoding parameter file (optional)");
		fprintf(stderr, "\n          -e ignore avi file errors");
		fprintf(stderr, "\n          -n generate video (.m1v) stream only");
		fprintf(stderr, "\n          -y vbv_buffer_size");
        fprintf(stderr, "\n          -m motion search magnitude, range 0 - 4, default 0");
		fprintf(stderr, "\n          -c horz_size vert_size cropping");
		fprintf(stderr, "\n          -f frame_rate_code 1->5, force frame rate");
		fprintf(stderr, "\n          -h n write sequence header every n-th GOP");
		fprintf(stderr, "\n          -x suppress usage of MMX extensions");
		fprintf(stderr, "\n          -d use double precision math (slower)");
		fprintf(stderr, "\n          -r remove noise from the input video (slow!)");
		fprintf(stderr, "\n          -q fast quantization (less accurate)");
		fprintf(stderr, "\n          -t fast motion compensation level, range 0 - 3, default 0 (normal)");
		fprintf(stderr, "\n          inputfile.avi  any valid avi file to encode");
		fprintf(stderr, "\n          outputfile.mpg  must be specified if there is more than 1 input file\n");
		exit(16);
}

void videocd()
{
		fprintf(stderr, "\nStarting with version 1.9 of avi2mpg1, the -v option");
		fprintf(stderr, "\nhas been disabled. Numerous issues with the -v option");
		fprintf(stderr, "\nprevented it from generating an accurate VideoCD file.");
		fprintf(stderr, "\n");
		fprintf(stderr, "\nA new program, avi2vcd has been written whose only");
		fprintf(stderr, "\npurpose is the generation of VideoCD 2.0 compliant files.");
		fprintf(stderr, "\nThe output of avi2vcd has been used to sucessfully create");
		fprintf(stderr, "\nVideoCD's which have been played back on a DVD player");
		fprintf(stderr, "\n");
		fprintf(stderr, "\nYou can download the latest version of avi2vcd from:");
		fprintf(stderr, "\n         www.mnsi.net/~jschlic1");
		fprintf(stderr, "\n");
		exit(16);
}

unsigned int abitrate[2][14] = {
          {32,64,96,128,160,192,224,256,288,320,352,384,416,448},
          {32,48,56,64,80,96,112,128,160,192,224,256,320,384},
        };

int ValidBitrate(layr, bRate)   /* return 1 if legal bit rate for layer, 0 otherwise */
unsigned int layr;           /* 1 or 2 */
unsigned int bRate;          /* legal rates from 32 to 448 */
{
int     index = 0;
int     found = 0;

    while(!found &&(index < 14))
	{
        if(abitrate[layr-1][index] == bRate)
            found = 1;
        else
            ++index;
    }
    if(found)
        return(1);
    else
        return(0);
}


void StripExtension(char *base, char *source)
{
	int i, period = 0, backslash = 0;

	strcpy(base, source);

	i = strlen(base);
	while((i > 0) && !period && !backslash)
	{
		i--;
		if(base[i] == '.')
			period = 1;
		else if (base[i] == '\\')
			backslash = 1;
	}

	if(i == 0)
		return; // scanned all the way to begining, there is no extension

	if(backslash)
		return; // backslash found before period, there can't be an extension

	// if we're not at i=0, and we didn't find a backslash,
	// then we have to have found a period!

	base[i] = '\0'; // strip off extension
}
//
// StripPath - copy source filename to base and strip
//                 path if any.
//
void StripPath(char *base, char *source)
{
	int i, backslash = 0;
	char *pSource;

	pSource = source;

	i = strlen(source);
	while((i > 0) && !backslash)
	{
		i--;
		if ((source[i] == '\\')||(source[i] == ':'))
			backslash = 1;
	}

	if(backslash)
	{
		pSource = pSource + i + 1;
		strcpy(base, pSource);
	}
	else
		strcpy(base, source);
	return;
}

//测试CPU是否支持MMX
void TestCPU(void)
{

	cpu_MMX = 0;
	cpu_3DNow = 0;


	#define cpuid __asm _emit 0x0F __asm _emit 0xA2
    __asm
    {
		pushfd                          ; 
		pop   eax                       ; 
		mov   ebx, eax                  ; 
		xor   eax, 0x00200000           ; 
		push  eax                       ; 
		popfd                           ; 
		pushfd                          ; 
		pop   eax                       ; 
		cmp   eax, ebx                  ; 
		jz    NO_CPUID                  ; 

	    mov   eax, 0x1
        cpuid
	    test  edx, 0x800000
	    je    NO_MMX
	    mov   cpu_MMX, 0x1


		mov   eax, 0x0
		cpuid
		mov eax, 0x68747541             ; reversed ASCII of "Auth"
		cmp eax, ebx
		jne NO_3DNOW
		mov eax, 0x69746E65             ; reversed ASCII of "enti"
		cmp eax, edx
		jne NO_3DNOW
		mov eax, 0x444D4163             ; reversed ASCII of "cAMD"
		cmp eax, ecx
		jne NO_3DNOW


		mov eax, 0x80000000           ; 
		cpuid
		cmp eax, 0x80000000
		jbe NO_3DNOW
		mov eax, 0x80000001
		cpuid
		test edx, 0x80000000          ; 
		jz NO_3DNOW                   ; 
		mov cpu_3DNow, 0x1            ; 

NO_MMX:
NO_3DNOW:
NO_CPUID:
	}
}
