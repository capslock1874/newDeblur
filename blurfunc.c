#include <opencv2/imgproc/imgproc_c.h>
#include "deblur.h"

void blur_function(const IplImage *latent_image, IplImage *blur_image, const CvMat *hom1, const CvMat *hom2)
{
	const int T = 20;
	const int tau = 10;
	CvMat *id_mat = cvCreateMat(3, 3, CV_32FC1);
	cvSetIdentity(id_mat, cvRealScalar(1));
	CvMat *invhom1 = cvCreateMat(3, 3, CV_32FC1);
	cvInvert(hom1, invhom1, CV_LU);
	
	CvMat *h1 = cvCreateMat(3, 3, CV_32FC1);
	CvMat *h2 = cvCreateMat(3, 3, CV_32FC1);
	CvSize size = cvSize(latent_image->width, latent_image->height);
	IplImage *temp = cvCreateImage(size, latent_image->depth, latent_image->nChannels);
	IplImage *blur = cvCreateImage(size, IPL_DEPTH_32F, latent_image->nChannels);
	cvSetZero(blur);
	
	for (int i = 1; i <= tau; ++i)
	{
		cvAddWeighted(id_mat, (double)(T-i)/T, invhom1, (double)i/T, 0, h1);
		cvAddWeighted(id_mat, (double)(T-i)/T, hom2, (double)i/T, 0, h2);
		cvWarpPerspective(latent_image, temp, h1, CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
		cvAdd(blur, temp, blur, NULL);
		cvWarpPerspective(latent_image, temp, h2, CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
		cvAdd(blur, temp, blur, NULL);
	}
	cvAdd(blur, latent_image, blur, NULL);
	cvConvertScale(blur, blur_image, 1.0/(2*tau+1), 0);
	
	cvReleaseMat(&id_mat);
	cvReleaseMat(&invhom1);
	cvReleaseMat(&h1);
	cvReleaseMat(&h2);
	cvReleaseImage(&temp);
	cvReleaseImage(&blur);
}