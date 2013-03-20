#include <stdio.h>
#include <time.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/video/tracking.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include "deblur.h"

#define MAX_CORNERS 1000
#define WIN_SIZE 10

int st1 = 0;
int st2 = 0;
int st3 = 0;
int st4 = 0;

void calc_homography(const IplImage *src, IplImage *dst[], CvMat *hom[], int image_num)
{
	CvSize size = cvSize(src->width, src->height);
	IplImage *img_prev = cvCreateImage(size, src->depth, 1);
	IplImage *img_curr = cvCreateImage(size, src->depth, 1);
	cvCvtColor(src, img_prev, CV_BGR2GRAY);
	
	CvPoint2D32f features[MAX_CORNERS];
	CvPoint2D32f features_curr[MAX_CORNERS];
	int corner_count = MAX_CORNERS;
	
	int t1 = clock();
	cvGoodFeaturesToTrack(img_prev, NULL, NULL, features, &corner_count, 0.02, 0.5, NULL, 3, 0, 0.04);
	st1 += clock()-t1;
	
	t1 = clock();
	cvFindCornerSubPix(img_prev, features, corner_count, cvSize(WIN_SIZE,WIN_SIZE), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 20, 0.03));
	st2 += clock()-t1;
	
	char feature_found[MAX_CORNERS];
	float feature_error[MAX_CORNERS];
	CvPoint2D32f good_src[MAX_CORNERS];
	CvPoint2D32f good_dst[MAX_CORNERS];
	CvSize pyr_size = cvSize(img_prev->width + 8, img_prev->height/3);
	IplImage *pyr_prev = cvCreateImage(pyr_size, IPL_DEPTH_32F, 1);
	IplImage *pyr_curr = cvCreateImage(pyr_size, IPL_DEPTH_32F, 1);
	
	for (int k = 0; k < image_num; ++k)
	{
		cvCvtColor(dst[k], img_curr, CV_BGR2GRAY);
		t1 = clock();
		cvCalcOpticalFlowPyrLK(img_prev, img_curr, pyr_prev, pyr_curr, features, features_curr, corner_count, cvSize(WIN_SIZE,WIN_SIZE), 5, feature_found, feature_error, cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 20, 0.03), 0);
		st3 += clock()-t1;
	
		int good_num = 0;
		for (int i = 0; i < corner_count; ++i)
		{
			if (feature_found[i] != 0 && feature_error[i] < 550)
			{
				good_src[good_num] = features[i];
				good_dst[good_num] = features_curr[i];
				++good_num;
			}
		}
	
		if (good_num >= 4)
		{
			CvMat pt_src = cvMat(1, good_num, CV_32FC2, good_src);
			CvMat pt_dst = cvMat(1, good_num, CV_32FC2, good_dst);
			
			t1 = clock();
			cvFindHomography(&pt_src, &pt_dst, hom[k], CV_RANSAC, 5, NULL);
			st4 += clock()-t1;
		}
		else fprintf(stderr, "Unable to calc homography : %d\n", k);
	}
	cvReleaseImage(&pyr_prev);
	cvReleaseImage(&pyr_curr);
	cvReleaseImage(&img_prev);
	cvReleaseImage(&img_curr);
}