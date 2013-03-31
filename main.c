#include <stdio.h>
#include <time.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include  <pthread.h>
#include "deblur.h"

IplImage *images[MAX_IMAGE];
IplImage *images_luck[MAX_IMAGE];
double luck[MAX_IMAGE];
CvMat *hom[MAX_IMAGE][MAX_IMAGE];
CvSize image_size;
int fps;
long long fourcc;
int mid = 0 ;
int image_num = 0 ;
static int RANK_ELEM = 0 ;
extern int st1, st2, st3, st4;

int input_image()
{
	
	image_num = 7 ;
	images[0] = cvLoadImage("1.jpg" , CV_LOAD_IMAGE_COLOR);
	images[1] = cvLoadImage("2.jpg", CV_LOAD_IMAGE_COLOR);
	images[2] = cvLoadImage("3.jpg", CV_LOAD_IMAGE_COLOR);
	images[3] = cvLoadImage("4.jpg", CV_LOAD_IMAGE_COLOR);
	images[4] = cvLoadImage("5.jpg", CV_LOAD_IMAGE_COLOR);
	images[5] = cvLoadImage("6.jpg", CV_LOAD_IMAGE_COLOR);
	images[6] = cvLoadImage("7.jpg", CV_LOAD_IMAGE_COLOR);
	/*images[7] = cvLoadImage("8.jpg", CV_LOAD_IMAGE_COLOR);*/
	/*images[8] = cvLoadImage("9.jpg", CV_LOAD_IMAGE_COLOR);*/
	image_size =cvGetSize(images[0]); 
	return image_num ;
	/*CvCapture *cap = cvCreateFileCapture("1.mp4");*/
	/*image_size = cvSize(cvGetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH), cvGetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT));*/
	/*//image_size = cvSize(640, 360);*/
	/*fps = cvGetCaptureProperty(cap, CV_CAP_PROP_FPS);*/
	/*fourcc = (int)cvGetCaptureProperty(cap, CV_CAP_PROP_FOURCC);*/
	/*printf("Video Info:\n");*/
	/*printf("FPS: %d\n", fps);*/
	/*printf("Size: %dx%d\n", image_size.width, image_size.height);*/
	/*printf("FourCC: %s\n", &fourcc);*/
	
	/*cvSetCaptureProperty(cap, CV_CAP_PROP_POS_FRAMES, 20);*/
	/*int i;*/
	/*for (i = 0; i < MAX_IMAGE; ++i)*/
	/*{*/
		/*IplImage *frame = cvQueryFrame(cap);*/
		/*if (frame != NULL)*/
		/*{*/
			/*images[i] = cvCreateImage(image_size, IPL_DEPTH_8U, 3);*/
			/*//cvResize(frame, images[i], CV_INTER_LINEAR);*/
			/*cvCopy(frame, images[i], NULL);*/
		/*}*/
		/*else break;*/
	/*}*/
	/*cvReleaseCapture(&cap);*/
	/*return i;*/
}

static void calLuck_pthread(void* flag)
{
	int mflag = (int)flag ;
	if( 0 == mflag )
	{
		for( int i = 1; i < mid ; i++ )
		{
			images_luck[i] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
			luck[i] = luck_image(images[i], images_luck[i], hom[i-1][i], hom[i][i+1]);
			/*printf("Frame %d : luckiness %f\n",i, luck[i]);*/
		}

	}
	else 
	{
		for( int i = mid; i < image_num - 1 ; i++ )
		{
			images_luck[i] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
			luck[i] = luck_image(images[i], images_luck[i], hom[i-1][i], hom[i][i+1]);
/*printf("Frame %d : luckiness %f\n",i, luck[i]);*/
		}
	}
}

static void calHomo_pthread(void* rank)
{
	int my_rank = (int)rank ;
	int low = my_rank * RANK_ELEM ;
	int high =( my_rank + 1) * RANK_ELEM - 1 ;

	for( int i = low;  i <= high ; ++i )
	{
		for( int j = 0 ; j < image_num  ; ++j )
			hom[i][j] = cvCreateMat(3, 3, CV_32FC1);
		calc_homography(images[i], images, hom[i], image_num);
	}
}

int main()
{
	int luckiestImage = 0 ;
	double luckMax = 0 ;
	long startT = 0 ;
	startT = clock();
	image_num = input_image();
	pthread_t* thread_handles ;
	thread_handles = malloc(4 * sizeof(pthread_t));

	printf("%d images acquired.\n", image_num);
	printf("Calculating homography...");

	pthread_create(&thread_handles[0] , NULL ,calHomo_pthread , (void*)0) ;
	pthread_create(&thread_handles[1] , NULL ,calHomo_pthread , (void*)1) ;
	pthread_create(&thread_handles[2] , NULL ,calHomo_pthread , (void*)2) ;
	pthread_create(&thread_handles[3] , NULL ,calHomo_pthread , (void*)3) ;
	pthread_join(thread_handles[0] , NULL) ;
	pthread_join(thread_handles[1] , NULL) ;
	pthread_join(thread_handles[2] , NULL) ;
	pthread_join(thread_handles[3] , NULL) ;
	free(thread_handles) ;
	
	for( int i = 4 * RANK_ELEM; i < image_num ; ++i )
	{
		for( int j = 0 ; j < image_num ; ++j )
			hom[i][j] = cvCreateMat(3, 3, CV_32FC1);
		calc_homography(images[i], images, hom[i], image_num);
	}
	
	for (int i = 0; i < image_num; ++i)
	{
		for (int j = 0; j < image_num; ++j)
			hom[i][j] = cvCreateMat(3, 3, CV_32FC1);
		calc_homography(images[i], images, hom[i], image_num);
	}
	printf("Done.\n");
	/*printf("Time Consuming:\n");*/
	/*printf("T1: %d ms\n", st1*1000/CLOCKS_PER_SEC);*/
	/*printf("T2: %d ms\n", st2*1000/CLOCKS_PER_SEC);*/
	/*printf("T3: %d ms\n", st3*1000/CLOCKS_PER_SEC);*/
	/*printf("T4: %d ms\n", st4*1000/CLOCKS_PER_SEC);*/
	
	
	printf("Calculating luckiness.\n");
	CvMat *id_mat = cvCreateMat(3, 3, CV_32FC1);
	cvSetIdentity(id_mat, cvRealScalar(1));
	images_luck[0] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
	images_luck[image_num - 1] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);

	luck[0] = luck_image(images[0], images_luck[0], id_mat, hom[0][1]) ; 
	mid = image_num / 2 ;
	thread_handles = malloc(2 * sizeof(pthread_t));
	pthread_create(&thread_handles[0] , NULL ,calLuck_pthread  , (void*)0) ;
	pthread_create(&thread_handles[1] , NULL ,calLuck_pthread  , (void*)1) ;
	pthread_join(thread_handles[0] , NULL) ;
	pthread_join(thread_handles[1] , NULL) ;

	free(thread_handles) ;
	luck[image_num -1] = luck_image(images[image_num -1], images_luck[image_num -1], hom[image_num-2][image_num -1], id_mat) ;
	cvReleaseMat(&id_mat);
	

	for( int i = 0 ; i < image_num ; i++ )
	{
		if( luck[i] > luckMax )
		{
			luckiestImage = i ;
			luckMax = luck[i];
		}
	}
	/*printf("luckiness is %f %d \n" , luck[luckiestImage] , luckiestImage);*/
	
	
	
	/*for (int i = 0; i < image_num; ++i)*/
	/*{*/
		/*char wname[16];*/
		/*sprintf(wname, "Frame%d", i);*/
		/*cvNamedWindow(wname, CV_WINDOW_AUTOSIZE);*/
		/*//cvMoveWindow(wname, i*50, i*50);*/
		/*cvMoveWindow(wname, 0, 0);*/
		/*cvShowImage(wname, images[i]);*/
	/*}*/
	/*cvWaitKey(0);*/
	/*cvDestroyAllWindows();*/
	

	IplImage *result = cvClone(images[luckiestImage]);
	IplImage *result_luck = cvCreateImage(image_size, IPL_DEPTH_32F, 4);
	deblur_image(image_num, luckiestImage, result, result_luck);
	cvReleaseImage(&result);
	long endT =  0 ;
	endT = clock();
	
	printf("waste time %d s\n" , (endT - startT)/ CLOCKS_PER_SEC) ;
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
