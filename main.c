#include <stdio.h>
#include <time.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include "deblur.h"

IplImage *images[MAX_IMAGE];
IplImage *images_luck[MAX_IMAGE];
double luck[MAX_IMAGE];
CvMat *hom[MAX_IMAGE][MAX_IMAGE];
CvSize image_size;
int fps;
long long fourcc;

extern int st1, st2, st3, st4;

int input_image()
{
	
	CvCapture *cap = cvCreateFileCapture("1.mp4");
	image_size = cvSize(cvGetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH), cvGetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT));
	//image_size = cvSize(640, 360);
	fps = cvGetCaptureProperty(cap, CV_CAP_PROP_FPS);
	fourcc = (int)cvGetCaptureProperty(cap, CV_CAP_PROP_FOURCC);
	printf("Video Info:\n");
	printf("FPS: %d\n", fps);
	printf("Size: %dx%d\n", image_size.width, image_size.height);
	printf("FourCC: %s\n", &fourcc);
	
	cvSetCaptureProperty(cap, CV_CAP_PROP_POS_FRAMES, 20);
	int i;
	for (i = 0; i < MAX_IMAGE; ++i)
	{
		IplImage *frame = cvQueryFrame(cap);
		if (frame != NULL)
		{
			images[i] = cvCreateImage(image_size, IPL_DEPTH_8U, 3);
			//cvResize(frame, images[i], CV_INTER_LINEAR);
			cvCopy(frame, images[i], NULL);
		}
		else break;
	}
	cvReleaseCapture(&cap);
	return i;
}

int main()
{
	
	int image_num = input_image();
	printf("%d images acquired.\n", image_num);
	printf("Calculating homography...");
	for (int i = 0; i < image_num; ++i)
	{
		for (int j = 0; j < image_num; ++j)
			hom[i][j] = cvCreateMat(3, 3, CV_32FC1);
		calc_homography(images[i], images, hom[i], image_num);
	}
	printf("Done.\n");
	
	printf("Time Consuming:\n");
	printf("T1: %d ms\n", st1*1000/CLOCKS_PER_SEC);
	printf("T2: %d ms\n", st2*1000/CLOCKS_PER_SEC);
	printf("T3: %d ms\n", st3*1000/CLOCKS_PER_SEC);
	printf("T4: %d ms\n", st4*1000/CLOCKS_PER_SEC);
	
	
	printf("Calculating luckiness.\n");
	CvMat *id_mat = cvCreateMat(3, 3, CV_32FC1);
	cvSetIdentity(id_mat, cvRealScalar(1));
	for (int i = 0; i < image_num; ++i)
	{
		images_luck[i] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
		if (i > 0 && i < image_num-1)
		{
			luck[i] = luck_image(images[i], images_luck[i], hom[i-1][i], hom[i][i+1]);
		}
		else if (i == 0)
		{
			luck[i] = luck_image(images[i], images_luck[i], id_mat, hom[i][i+1]);
		}
		else
		{
			luck[i] = luck_image(images[i], images_luck[i], hom[i-1][i], id_mat);
		}
		printf("Frame %d : luckiness %f\n",i, luck[i]);
	}
	cvReleaseMat(&id_mat);
	
	
	
	for (int i = 0; i < image_num; ++i)
	{
		char wname[16];
		sprintf(wname, "Frame%d", i);
		cvNamedWindow(wname, CV_WINDOW_AUTOSIZE);
		//cvMoveWindow(wname, i*50, i*50);
		cvMoveWindow(wname, 0, 0);
		cvShowImage(wname, images[i]);
	}
	cvWaitKey(0);
	cvDestroyAllWindows();
	
	IplImage *result = cvCreateImage(image_size, IPL_DEPTH_8U, 3);
	IplImage *result_luck = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
	deblur_image(image_num, 3, result, result_luck);
	cvReleaseImage(&result);
	
	cvWaitKey(0);
	cvDestroyAllWindows();
	for (int i = 0; i < image_num; ++i)
	{
		cvReleaseImage(&images[i]);
		cvReleaseImage(&images_luck[i]);
	}
	for (int i = 0; i < image_num; ++i)
		for (int j = 0; j < image_num; ++j)
		{
			cvReleaseMat(&hom[i][j]);
		}
}
