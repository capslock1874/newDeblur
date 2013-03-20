#include <math.h>
#include <opencv2/imgproc/imgproc_c.h>
#include "deblur.h"

#define SIGMA_L 20

static double luck_pixel(int x, int y, const CvMat *hom1, const CvMat *hom2)
{
	CvMat *invhom1 = cvCreateMat(3, 3, CV_32FC1);
	cvInvert(hom1, invhom1, CV_LU);
	
	CvPoint2D32f src = cvPoint2D32f(x, y);
	CvPoint2D32f d1, d2;
	CvMat pt_src = cvMat(1, 1, CV_32FC2, &src);
	CvMat pt_dst = cvMat(1, 1, CV_32FC2, &d1);
	cvPerspectiveTransform(&pt_src, &pt_dst, invhom1);
	pt_dst = cvMat(1, 1, CV_32FC2, &d2);
	cvPerspectiveTransform(&pt_src, &pt_dst, hom2);
	
	double dis = (src.x-d1.x)*(src.x-d1.x)+(src.y-d1.y)*(src.y-d1.y);
	dis += (src.x-d2.x)*(src.x-d2.x)+(src.y-d2.y)*(src.y-d2.y);
	double luck = exp(-dis/(2*SIGMA_L*SIGMA_L));
	cvReleaseMat(&invhom1);
	return luck;
}

double luck_image(const IplImage *img, IplImage *img_luck, const CvMat *hom1, const CvMat *hom2)
{
	double sum = 0;
	for (int i = 0; i < img->height; ++i)
		for (int j = 0; j < img->width; ++j)
		{
			double luck = luck_pixel(j, i, hom1, hom2);
			CvScalar p = cvGet2D(img, i, j);
			for (int k = 0; k < 3; ++k) p.val[k] /= 255.0;//?
			p.val[3] = luck;
			cvSet2D(img_luck, i, j, p);
			sum += luck;
		}
	sum /= img->width * img->height;
	return sum;
}