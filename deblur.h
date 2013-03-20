#ifndef _DEBLUR_H
#define _DEBLUR_H

#define MAX_IMAGE 10

int input_image();
void calc_homography(const IplImage *src, IplImage *dst[], CvMat *hom[], int image_num);
double luck_image(const IplImage *img, IplImage *img_luck, const CvMat *hom1, const CvMat *hom2);
void blur_function(const IplImage *latent_image, IplImage *blur_image, const CvMat *hom1, const CvMat *hom2);
void deblur_image(int image_num, int n, IplImage *result, IplImage *result_luck);
int sqrdiff(const IplImage *p1, const IplImage *p2);


#endif