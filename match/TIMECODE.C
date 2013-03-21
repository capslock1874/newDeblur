
#include "mplex.h"
/*************************************************************************

    下面的程序是用来计算时间码的。
*************************************************************************/

void empty_timecode_struc (timecode)
Timecode_struc *timecode;
{
    timecode->msb=0;
    timecode->lsb=0;
}

void offset_timecode (time1, time2, offset)
Timecode_struc *time1, *time2, *offset;
{
    offset->msb = time2->msb - time1->msb;
    offset->lsb = time2->lsb - time1->lsb;
}

void copy_timecode (time_original, time_copy)
Timecode_struc *time_original, *time_copy;
{
    time_copy->lsb=time_original->lsb;
    time_copy->msb=time_original->msb;
}

void make_timecode (timestamp, pointer)
double timestamp;
Timecode_struc *pointer;
{
    if (timestamp > MAX_FFFFFFFF)
    {
	pointer->msb=1;
	timestamp -= MAX_FFFFFFFF;
	pointer->lsb=(unsigned long)timestamp;
    } else
    {
	pointer->msb=0;
	pointer->lsb=(unsigned long)timestamp;
    }
}


void add_to_timecode (add, to)
Timecode_struc *add;
Timecode_struc *to;
{
    to->msb = (add->msb ^ to->msb);


    if (((add->lsb & 0x80000000) & (to->lsb & 0x80000000))>>31)
    {
	to->msb = to->msb ^ 1;
	to->lsb = (to->lsb & 0x7fffffff)+(add->lsb & 0x7fffffff);
    }


    else if (((add->lsb & 0x80000000) | (to->lsb & 0x80000000))>>31)
    {
	to->msb = to->msb ^ 
	  ((((add->lsb & 0x7fffffff)+(to->lsb & 0x7fffffff)) & 0x80000000)>>31);
	to->lsb = ((to->lsb & 0x7fffffff)+(add->lsb & 0x7fffffff)^0x80000000);
    }


    else
    {
	to->lsb = to->lsb + add->lsb;
    }
}


/*************************************************************************
    在缓存中复制TimeCode，并根据MPEG系统的规定将其分配到各比特域中
*************************************************************************/

void buffer_timecode (pointer, marker, buffer)

Timecode_struc *pointer;
unsigned char  marker;
unsigned char **buffer;

{
    unsigned char temp;

    temp = (unsigned char)((marker << 4) | (pointer->msb <<3) |
		((pointer->lsb >> 29) & 0x6) | 1);
    *((*buffer)++)=temp;
    temp = (unsigned char)((pointer->lsb & 0x3fc00000) >> 22);
    *((*buffer)++)=temp;
    temp = (unsigned char)(((pointer->lsb & 0x003f8000) >> 14) | 1);
    *((*buffer)++)=temp;
    temp = (unsigned char)((pointer->lsb & 0x7f80) >> 7);
    *((*buffer)++)=temp;
    temp = (unsigned char)(((pointer->lsb & 0x007f) << 1) | 1);
    *((*buffer)++)=temp;

}


int comp_timecode (TS1, TS2)
Timecode_struc *TS1;
Timecode_struc *TS2;
{
    double Time1;
    double Time2;

    Time1 = (TS1->msb * MAX_FFFFFFFF) + (TS1->lsb);
    Time2 = (TS2->msb * MAX_FFFFFFFF) + (TS2->lsb);

    return (Time1 <= Time2);
}
