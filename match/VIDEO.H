/* video.h */


#define MAX_AVI_FILES 10

EXTERN  HRESULT	hResult;
EXTERN  PAVISTREAM pAudioStreams[MAX_AVI_FILES];
EXTERN  AVISTREAMINFO pAudioStreamInfo[MAX_AVI_FILES];
EXTERN  PAVIFILE	pfile;
EXTERN	PAVISTREAM	pstream;
EXTERN	unsigned long	framenum, startf, endf;
EXTERN	PGETFRAME	pget[MAX_AVI_FILES];
EXTERN	LPBITMAPINFOHEADER	lpbi;
EXTERN	unsigned char	*lpbitmp, *lpcurpix, *lpcurscanl;
EXTERN  unsigned short *lpcurpixw;
EXTERN	FILE	*bmpfile;
EXTERN	unsigned char	pix;
EXTERN	char	sourcefile[256], destfile[256];
EXTERN	AVIFILEINFO	masterPfi;
EXTERN  int totalFrames;
EXTERN  unsigned int nextFileFrames[MAX_AVI_FILES];
EXTERN  unsigned int nextFileSamples[MAX_AVI_FILES];
EXTERN  int numAviFiles;
