/* putbits.c, 比特级输出*/


#include <stdio.h>

extern FILE *outfile; 

static unsigned char outbfr;
static int outcnt;
static int bytecnt;
static int freespace;				/* 缓存中的可用空间*/
static unsigned char *bufptr;		
static unsigned char buffer[65536];	/* 大小为64KB缓存去 */

/* 初始化缓存*/
void initbits()
{
  outcnt = 8;
  bytecnt = 0;
  freespace = sizeof(buffer);
  bufptr = buffer;
}

void flushbits()
{
	fwrite(buffer, 1, sizeof(buffer) - freespace, outfile);
	bufptr = buffer;
	freespace = sizeof(buffer);
}

/* 向输出文件中写入最右的n比特*/
void putbits(val,n)
int val;
int n;
{
  int i;
  unsigned int mask;

  mask = 1 << (n-1); 

  for (i=0; i<n; i++)
  {
    outbfr <<= 1;

    if (val & mask)
      outbfr|= 1;

    mask >>= 1; // select next bit
    outcnt--;

    if (outcnt==0) // 8 bit buffer full
    {
		*bufptr++ = outbfr;
		if(--freespace <= 0)
			flushbits();
		outcnt = 8;
		bytecnt++;
    }
  }
}

void alignbits()
{
  if (outcnt!=8)
    putbits(0,outcnt);
}

/* 返回产生的所有比特数*/
int bitcount()
{
  return 8*bytecnt + (8-outcnt);
}
