#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "deblur.h"
#define PATCH_SIZE 5 
#define DEBLUR_WIN_SIZE 21
#define SIGMA_W 10
#define LAMBDA 10
#define LEFT_TOP_CORNER 1
#define TOP 2
#define RIGHT_TOP_CORNER 3
#define LEFT 4
#define CENTER 5 
#define RIGHT 6
#define LEFT_BOTTOM_CORNER 7
#define BOTTOM 8
#define RIGHT_BOTTOM_CORNER 9
#define P(E) printf(#E"=%d\n",E);
#define PF(E) printf(#E"=%f\n",E);
#define PTHREAD_NUM 4 

extern IplImage *images[MAX_IMAGE];
extern IplImage *images_luck[MAX_IMAGE];
extern CvMat *hom[MAX_IMAGE][MAX_IMAGE];
extern CvSize image_size;

static int grid_patch ;
static int grid_c ;
static int grid_r ;
static IplImage *trans[MAX_IMAGE];
static IplImage *trans_luck[MAX_IMAGE];
static IplImage *blur[MAX_IMAGE];
static int global_image_num ;
static int global_n ;
static CvMat *patch ;

// 用OpenCv函数实现，但是性能不佳
// 纯手工，性能比上面那个好
double sqrdiff(const IplImage *p1, const IplImage *p2)
{
	CvRect roi = cvGetImageROI(p1);
	int v = p1->nChannels;
	double sum = 0;
	double t = 0 ;
	for (int i = 0; i < roi.height; ++i)
		for (int j = 0; j < roi.width; ++j)
		{
			CvScalar s1 = cvGet2D(p1, i, j);
			CvScalar s2 = cvGet2D(p2, i, j);
			for (int k = 0; k < v; ++k)
			{
				t = s1.val[k]-s2.val[k];
				sum += t*t;
			}
		}
	return sum;
}
static double luck_diff(const IplImage *luck)
{
	CvRect roi = cvGetImageROI(luck);
	double sum = 0;
	double t = 0 ;
	for (int i = 0; i < roi.height; ++i)
		for (int j = 0; j < roi.width; ++j)
		{
			CvScalar s = cvGet2D(luck, i, j);
			t = 1-s.val[3];
			sum += t*t;
		}
	return sum;
}
//直接传入ROI区域得函数
double sqrdiff_without_roi(const IplImage *p1, const IplImage *p2 , int p1x , int p1y ,  int p2x , int p2y , int height , int width)
{
	int v = p1->nChannels;
	double sum = 0;
	double t = 0 ;
	CvScalar s1 , s2 ;
	for (int i = 0; i < height; ++i)
		for (int j = 0; j < width; ++j)
		{
			s1 = cvGet2D(p1, p1y + i, p1x + j);
			s2 = cvGet2D(p2, p2y + i, p2x + j);
			sum += (s1.val[0] - s2.val[0]) *(s1.val[0] - s2.val[0])
				+ (s1.val[1] - s2.val[1]) *(s1.val[1] - s2.val[1])
				+ (s1.val[2] - s2.val[2]) *(s1.val[2] - s2.val[2]) ;
		}
	return sum;
}
static double luck_diff_without_roi(const IplImage *luck , int x , int y , int height , int width)
{
	double sum = 0;
	double t = 0 ;
	CvScalar s ;
	for (int i = 0; i < height; ++i)
		for (int j = 0; j < width; ++j)
		{
			s = cvGet2D(luck, y + i, x + j);
			t = 1-s.val[3];
			sum += t*t;
		}
	return sum;
}
static void search_aux(IplImage *blur , IplImage *image , IplImage *luck ,int points[][2] , int pointsNum ,  int* index , double* centerDiff ,
		int roix , int roiy)
{
	double mindiff = *centerDiff;
	double diff , diff2 ; 
	int pos = 1 ;
	for( int i = 0 ; i < pointsNum ; i++ )
	{
		diff = sqrdiff_without_roi(image , blur , roix , roiy ,  points[i][0] - PATCH_SIZE/2, 
				points[i][1] - PATCH_SIZE/2 , PATCH_SIZE , PATCH_SIZE ) ;
		diff2 = luck_diff_without_roi(luck , points[i][0] - PATCH_SIZE/2 , points[i][1] - PATCH_SIZE/2 , PATCH_SIZE , PATCH_SIZE );
		diff += LAMBDA * diff2 ; 
		if( diff < mindiff )
		{
			*index = pos ;
			mindiff = diff;
		}
		++pos ;
	}
	*centerDiff = mindiff ;
}
static void  searchLeftTopCorner(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy)
{
	int points[5][2] = {{*x - 2 , *y + 2} , {*x - 2 , *y } ,{*x - 2 , *y - 2 } ,{*x , *y - 2 }, {*x + 2 , *y - 2}} ;
	int status[6] = { CENTER ,LEFT_BOTTOM_CORNER , LEFT , LEFT_TOP_CORNER , TOP , RIGHT_TOP_CORNER };
	int pos = 0 ;
    search_aux(blur , image , luck , points , 5 , &pos , centerDiff , roix , roiy);
	*index = status[pos];	
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void  searchRightTopCorner(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy)
{
	int points[5][2] = {{*x - 2 , *y - 2} , {*x  , *y - 2 } ,{*x + 2 , *y - 2 } ,{*x + 2 , *y }, {*x + 2 , *y + 2}} ;
	int status[6] = { CENTER , LEFT_TOP_CORNER , TOP , RIGHT_TOP_CORNER , RIGHT , RIGHT_BOTTOM_CORNER};
	int pos = 0 ;
    search_aux(blur , image , luck , points , 5 , &pos , centerDiff , roix , roiy);
	*index = status[pos];	
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void  searchLeftBottomCorner(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy)
{
	int points[5][2] = {{*x - 2 , *y - 2} , {*x - 2  , *y } ,{*x - 2 , *y + 2 } ,{*x , *y + 2 }, {*x + 2 , *y + 2}} ;
	int status[6] = { CENTER , LEFT_TOP_CORNER , LEFT  , LEFT_BOTTOM_CORNER, BOTTOM , RIGHT_BOTTOM_CORNER};
	int pos = 0 ;
    search_aux(blur , image , luck , points , 5 , &pos , centerDiff , roix , roiy);
	*index = status[pos];	
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void  searchRightBottomCorner(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy)
{
	int points[5][2] = {{*x - 2 , *y + 2} , {*x  , *y + 2 } ,{*x + 2 , *y + 2 } ,{*x + 2 , *y }, {*x + 2 , *y - 2}} ;
	int status[6] = { CENTER , LEFT_BOTTOM_CORNER, BOTTOM , RIGHT_BOTTOM_CORNER, RIGHT, RIGHT_TOP_CORNER};
	int pos = 0 ;
    search_aux(blur , image , luck , points , 5 , &pos , centerDiff , roix , roiy);
	*index = status[pos];	
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void searchTop(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy) 
{
	int points[3][2] = {{*x - 2  , *y - 2} ,{ *x , *y -2 } , {*x + 2 , *y - 2 }} ;
	int status[4] = { CENTER , LEFT_TOP_CORNER , TOP , RIGHT_TOP_CORNER} ;
	int pos = 0 ;
	search_aux(blur , image , luck , points , 3 , &pos , centerDiff , roix , roiy);
	*index = status[pos] ;
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void searchRight(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy) 
{
	int points[3][2] = {{*x + 2  , *y - 2} ,{ *x + 2, *y } , {*x + 2 , *y + 2 }} ;
	int status[4] = { CENTER , RIGHT_TOP_CORNER , RIGHT , RIGHT_BOTTOM_CORNER} ;
	int pos = 0 ;
	search_aux(blur , image , luck , points , 3 , &pos , centerDiff , roix , roiy);
	*index = status[pos]; 
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void searchBottom(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy) 
{
	int points[3][2] = {{*x - 2  , *y + 2} ,{ *x , *y + 2 } , {*x + 2 , *y + 2 }} ;
	int status[4] = { CENTER , LEFT_BOTTOM_CORNER , BOTTOM , RIGHT_BOTTOM_CORNER} ;
	int pos = 0 ;
	search_aux(blur , image , luck , points , 3 , &pos , centerDiff , roix , roiy);
	*index = status[pos] ;
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static void searchLeft(IplImage *blur , IplImage *image , IplImage *luck , int *x , int *y , int* index , double* centerDiff , int roix , int roiy) 
{
	int points[3][2] = {{*x - 2  , *y - 2} ,{ *x - 2, *y } , {*x - 2 , *y + 2 }} ;
	int status[4] = { CENTER , LEFT_TOP_CORNER , LEFT  , LEFT_BOTTOM_CORNER} ;
	int pos = 0 ;
	search_aux(blur , image , luck , points , 3 , &pos , centerDiff, roix , roiy);
	*index = status[pos] ;
	if( CENTER != *index )
	{
		*x = points[pos-1][0] ;
		*y = points[pos-1][1] ;
	}
}
static double search(IplImage *blur , IplImage *image , IplImage *luck ,  int* x , int* y , int* index , int step , int roix , int roiy)
{
	int minx = -1;
	int miny = -1;
	double mindiff = 20000000000 ;
	double diff , diff2 ; 
	int pos = 1 ;
	for( int i = *x - step ; i <= *x + step ; i += step)
	{
		for( int j = *y - step; j <= *y + step ; j += step)
		{
			diff = sqrdiff_without_roi(image , blur  , roix , roiy , i - PATCH_SIZE/2 , j - PATCH_SIZE/2 , PATCH_SIZE , PATCH_SIZE) ;
			diff2 = luck_diff_without_roi(luck , i - PATCH_SIZE/2 ,  j - PATCH_SIZE/2 , PATCH_SIZE , PATCH_SIZE);
			diff += LAMBDA * diff2 ; 
			if( diff < mindiff )
			{
				*index = pos ;
				mindiff = diff;
				minx = i;
				miny = j;
			}
			++pos ;
		}
	}
	*x = minx ;
	*y = miny ;
	return mindiff ; 
}
static int deblur_patch(IplImage *blur[], IplImage *luck[], int image_num, int n, int x, int y, CvScalar *res)
{
	if (n < 1 || n >= image_num-1 || x-PATCH_SIZE/2 < 0 || y-PATCH_SIZE/2 < 0 || x+PATCH_SIZE/2 >= image_size.width || y+PATCH_SIZE/2 >= image_size.height) return -1;
	int minj = -21;
	int minx = -21;
	int miny = -21;
	int mx = x , my = y ; 
	double mindiff = 200000000000 ;
	double tmpDiff = 200000000000 ;
	int pos = 0 ;
	typedef void (*searchFuncPtr)(IplImage*, IplImage*, IplImage*, int* , int* , int* , double* , int , int ) ;
	searchFuncPtr searchFuncArray[10] = {NULL , searchLeftTopCorner , searchTop , searchRightTopCorner  ,
		searchLeft , NULL , searchRight ,
		searchLeftBottomCorner , searchBottom , searchRightBottomCorner} ;
	float oriIndex[] = {(float)x , (float)y , 1.0} ;
	float warpIndex[] = {(float)x , (float)y , 1.0} ;
	CvMat oriMat , warpMat ;
	int left , right , top , bottom ;
	cvInitMatHeader(&oriMat , 3 , 1 , CV_32FC1 ,oriIndex ,  CV_AUTOSTEP) ;
	cvInitMatHeader(&warpMat , 3 , 1 , CV_32FC1 , warpIndex , CV_AUTOSTEP) ;
	for (int j = 0; j < image_num; ++j)
	{
		cvMatMul(hom[n][j] ,&oriMat,  &warpMat) ;
		/*printf("1========= %f %f %f \n2==========%f %f %f\n" ,oriIndex[0] , oriIndex[1] , oriIndex[2] , warpIndex[0] , warpIndex[1] , warpIndex[2]) ;*/
		pos = 0 ;
		mx = (int)warpIndex[0] ;
		my = (int)warpIndex[1] ;
		left = mx-DEBLUR_WIN_SIZE/2-PATCH_SIZE/2;
		left = left < 0 ? PATCH_SIZE/2 : left+PATCH_SIZE/2;
		right = mx+DEBLUR_WIN_SIZE/2+PATCH_SIZE/2;
		right = right >= image_size.width ? image_size.width-1-PATCH_SIZE/2 : right-PATCH_SIZE/2;
		top = my-DEBLUR_WIN_SIZE/2-PATCH_SIZE/2;
		top = top < 0 ? PATCH_SIZE/2 : top+PATCH_SIZE/2;
		bottom = my+DEBLUR_WIN_SIZE/2+PATCH_SIZE/2;
		bottom = bottom >= image_size.height ? image_size.height-1-PATCH_SIZE/2 : bottom-PATCH_SIZE/2;

		if (n < 1 || n >= image_num-1 || mx-PATCH_SIZE/2 < 0 || my-PATCH_SIZE/2 < 0 || mx+PATCH_SIZE/2 >= image_size.width || my+PATCH_SIZE/2 >= image_size.height) return -1;

		if( mx + 2 <= right && mx - 2 >= left
				&& my + 2 <= bottom && my -2 >= top)
		{
			int searchX , searchY ;
			searchX = mx ;
            searchY = my ;
			tmpDiff = search(blur[j] , images[n] , luck[j] , &searchX , &searchY , &pos , 2 , x - PATCH_SIZE/2, y - PATCH_SIZE/2) ;
			if( pos == CENTER )
			{
				mindiff = search(blur[j] , images[n] , luck[j] , &searchX , &searchY , &pos , 1  , x - PATCH_SIZE/2 , y - PATCH_SIZE/2) ;
				minj = j ;
				minx = searchX ;
				miny = searchY ;
				continue ;
			} else 
			{
				for( ; CENTER != pos && searchX - 2 >= left && searchX + 2 <= right 
						&& searchY + 2 <= bottom && searchY - 2 >= top ;  )
				{
					searchFuncPtr searchFunc = searchFuncArray[pos] ; 
					searchFunc( blur[j] , images[n] , luck[j] , &searchX , &searchY , &pos , &tmpDiff , x - PATCH_SIZE/2 , y - PATCH_SIZE/2);
				}
				if( CENTER == pos )
				{
					mindiff = tmpDiff ;
					minj = j ;
					minx = searchX ;
					miny = searchY ;
					continue ;
				}else
				{
					mindiff = tmpDiff ;
					minj = j ;
					minx = searchX ;
					miny = searchY ;
					continue ;
				}
			}
		}
	}
	res->val[0] = minj;
	res->val[1] = minx;
	res->val[2] = miny;
	res->val[3] = mindiff;
	if( minj == -21 )
	{
		return 1 ;	
	}
	return 0;
}
static CvScalar weighted_average(const CvScalar *s,  double *w, int n)
{
	CvScalar res = {{0, 0, 0, 0}};
	w[0] = w[0] * 10000000 + 1 ;
	w[1] = w[0] * 10000000 + 1 ;
	w[2] = w[0] * 10000000 + 1 ;
	w[3] = w[0] * 10000000 + 1 ;
	if (n > 0)
	{
		double wsum = 0;
		for (int i = 0; i < n; ++i)
		{
			wsum += w[i];
			/*for (int j = 0; j < 4; ++j)*/
			/*{*/
				/*res.val[j] += s[i].val[j]*w[i];*/
			/*}*/
			res.val[0] += s[i].val[0]*w[i];
			res.val[1] += s[i].val[1]*w[i];
			res.val[2] += s[i].val[2]*w[i];
			res.val[3] += s[i].val[3]*w[i];
		}
		/*for (int i = 0; i < 4; ++i)*/
		/*{*/
			/*res.val[i] /= wsum;*/
		/*}*/
		res.val[0] /= wsum;
		res.val[1] /= wsum;
		res.val[2] /= wsum;
		res.val[3] /= wsum;
	}
	return res;
}

//线程函数

void* deblur_Image_pthread(void* rank)
{
	long my_rank = (long)rank ;
	int my_first_row = my_rank * grid_patch ;
	int my_last_row =(my_rank + 1 ) * grid_patch - 1 ;
	/*printf("in rank %d with my_first_row is %d and my_last_row is %d\n" , my_rank , my_first_row , my_last_row) ;*/
	int x , y ;
	for (int i = my_first_row; i <= my_last_row; ++i)
	{
		y = (i+1)*(PATCH_SIZE/2)+2;
		int t1 = clock();
		for (int j = 0; j < grid_c; ++j)
		{
			CvScalar res;
			x = (j+1)*(PATCH_SIZE/2)+2;
			/*printf("in pthread x= %d y = %d\n" , x , y) ;*/
			if (deblur_patch(blur,images_luck , global_image_num, global_n, x, y, &res) != 0)
			{
				/*printf("deblur_patch: %d:%d,%d failed.\n", global_n, x, y);*/
				res.val[0] = global_n;
				res.val[1] = x;
				res.val[2] = y;
				res.val[3] = 1;
			}
			res.val[3] = exp(-res.val[3]/(2*SIGMA_W*SIGMA_W));
			CV_MAT_ELEM(*patch, CvScalar, i, j) = res;
		}
		int t2 = clock();
		/*printf("y:%d/%d  %d ms\n", y, image_size.height, (t2-t1)*1000/CLOCKS_PER_SEC);*/
	}
	return NULL ;
}

void deblur_image(int image_num, int n, IplImage *result, IplImage *result_luck)
{
	/*cvSetZero(result);*/
	/*cvSetZero(result_luck);*/
	/*IplImage *trans[MAX_IMAGE];*/
	/*IplImage *trans_luck[MAX_IMAGE];*/
	/*IplImage *blur[MAX_IMAGE];*/
	for (int i = 0; i < image_num; ++i)
	{
		/*trans[i] = cvCreateImage(image_size, IPL_DEPTH_8U, 3);*/
		/*cvWarpPerspective(images[i], trans[i], hom[i][n], CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));*/
		/*trans_luck[i] = cvCreateImage(image_size, IPL_DEPTH_32F, 4);*/
		/*cvWarpPerspective(images_luck[i], trans_luck[i], hom[i][n], CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));*/
		blur[i] = cvCreateImage(image_size, IPL_DEPTH_8U, 3);
		blur_function(images[i], blur[i], hom[n-1][n], hom[n][n+1]);
	}
	/*for (int i = 0; i < image_num; ++i)*/
	/*{*/
		/*char wname[16];*/
		/*sprintf(wname, "Homography%d", i);*/
		/*cvNamedWindow(wname, CV_WINDOW_AUTOSIZE);*/
		/*cvMoveWindow(wname, i*50, i*50);*/
		/*cvShowImage(wname, trans[i]);*/
		/*sprintf(wname, "Blurred%d", i);*/
		/*cvNamedWindow(wname, CV_WINDOW_AUTOSIZE);*/
		/*cvMoveWindow(wname, i*50+100, i*50);*/
		/*cvShowImage(wname, blur[i]);*/
	/*}*/
	/*cvWaitKey(0);*/
	/*cvDestroyAllWindows();*/
	grid_r = (image_size.height-PATCH_SIZE/2 - 5) / (PATCH_SIZE/2);
	grid_c = (image_size.width-PATCH_SIZE/2 - 5) / (PATCH_SIZE/2);
	grid_patch = grid_r / PTHREAD_NUM ;
	/*P(grid_r) ;*/
	/*P(grid_c) ;*/
	/*P(grid_patch) ;*/
	int x , y , t1 , t2 ;
	
	global_image_num = image_num ;
	global_n = n ;

	if (grid_r > 0 && grid_c > 0)
	{
		patch = cvCreateMat(grid_r, grid_c, CV_64FC4);
		pthread_t* thread_handles ;

		thread_handles = malloc(PTHREAD_NUM * sizeof(pthread_t));

		pthread_create(&thread_handles[0] , NULL , deblur_Image_pthread , (void*)0) ;
		pthread_create(&thread_handles[1] , NULL , deblur_Image_pthread , (void*)1) ;
		pthread_create(&thread_handles[2] , NULL , deblur_Image_pthread , (void*)2) ;
		pthread_create(&thread_handles[3] , NULL , deblur_Image_pthread , (void*)3) ;

		for (int i = grid_patch * PTHREAD_NUM ; i < grid_r; ++i)
		{
			y = (i+1)*(PATCH_SIZE/2)+2;
			t1 = clock();
			for (int j = 0; j < grid_c; ++j)
			{
				CvScalar res;
				x = (j+1)*(PATCH_SIZE/2)+2;
				if (deblur_patch(blur, images_luck , image_num, n, x, y, &res) != 0)
				{
					/*printf("deblur_patch: %d:%d,%d failed.\n", n, x, y);*/
					res.val[0] = n;
					res.val[1] = x;
					res.val[2] = y;
					res.val[3] = 1;
				}
				res.val[3] = exp(-res.val[3]/(2*SIGMA_W*SIGMA_W));
				CV_MAT_ELEM(*patch, CvScalar, i, j) = res;
			}
			t2 = clock();
			/*printf("y:%d/%d  %d ms\n", y, image_size.height, (t2-t1)*1000/CLOCKS_PER_SEC);*/
		}
		/*printf("start deblur\n");*/
		pthread_join(thread_handles[0] , NULL) ;
		pthread_join(thread_handles[1] , NULL) ;
		pthread_join(thread_handles[2] , NULL) ;
		pthread_join(thread_handles[3] , NULL) ;

		free(thread_handles) ;

		cvNamedWindow("origin", CV_WINDOW_AUTOSIZE);
		cvShowImage("origin", images[n]);
		cvNamedWindow("result", CV_WINDOW_AUTOSIZE);
		printf("deblur starting ...\n") ;
		CvScalar v[4];
		CvScalar pv ;
		// 中心部分
		for (int i = 1; i < grid_r; ++i)
		{
			int miny = i*(PATCH_SIZE/2);
			for (int j = 1; j < grid_c; ++j)
			{
				CvScalar pres1 = CV_MAT_ELEM(*patch, CvScalar, i-1, j-1);
				CvScalar pres2 = CV_MAT_ELEM(*patch, CvScalar, i-1, j);
				CvScalar pres3 = CV_MAT_ELEM(*patch, CvScalar, i, j-1);
				CvScalar pres4 = CV_MAT_ELEM(*patch, CvScalar, i, j);
				int minx = j*(PATCH_SIZE/2);
				for (int y = 0; y < PATCH_SIZE/2; ++y)
					for (int x = 0; x < PATCH_SIZE/2; ++x)
					{
						v[0] = cvGet2D(images[(int)pres1.val[0]], (int)pres1.val[2]+y, (int)pres1.val[1]+x);
						v[1] = cvGet2D(images[(int)pres2.val[0]], (int)pres2.val[2]+y, (int)pres2.val[1]+x-PATCH_SIZE/2);
						v[2] = cvGet2D(images[(int)pres3.val[0]], (int)pres3.val[2]+y-PATCH_SIZE/2, (int)pres3.val[1]+x);
						v[3] = cvGet2D(images[(int)pres4.val[0]], (int)pres4.val[2]+y-PATCH_SIZE/2, (int)pres4.val[1]+x-PATCH_SIZE/2);
						double w[4] = {pres1.val[3], pres2.val[3], pres3.val[3], pres4.val[3]};
						pv = weighted_average(v, w, 4);
						/*if( pv.val[0] == 0 && pv.val[1] == 0 && pv.val[2] == 0 )*/
						/*{*/
							/*CvScalar replace = {{112 ,112 ,112 ,112}} ;*/
							/*CvScalar oriEle = cvGet2D(images[n] , i , j);*/
							/*[>printf("ori is %f %f %f\n" , oriEle.val[0] , oriEle.val[1] , oriEle.val[2]);<]*/
							/*cvSet2D(result, miny+y, x + minx, oriEle);*/
							/*[>printf("it's black with %d %d\n" , miny + y , x + minx);<]*/
							/*continue ;*/
						/*}*/
						/*else */
							cvSet2D(result, y+miny, x+minx, pv);
					}
			}
			cvShowImage("result", result);
			cvWaitKey(20);
		}
		 /*四周特殊情况*/
		for (int y = 0; y < PATCH_SIZE/2; ++y)
			for (int x = 0; x < PATCH_SIZE/2; ++x)
			{
				CvScalar pres = CV_MAT_ELEM(*patch, CvScalar, 0, 0);
				CvScalar pv = cvGet2D(images[(int)pres.val[0]], (int)pres.val[2]+y-PATCH_SIZE/2, (int)pres.val[1]+x-PATCH_SIZE/2);
				cvSet2D(result, y, x, pv);
				pres = CV_MAT_ELEM(*patch, CvScalar, 0, grid_c-1);
				pv = cvGet2D(images[(int)pres.val[0]], (int)pres.val[2]+y-PATCH_SIZE/2, (int)pres.val[1]+x);
				cvSet2D(result, y, grid_c*(PATCH_SIZE/2)+x, pv);
				pres = CV_MAT_ELEM(*patch, CvScalar, grid_r-1, 0);
				pv = cvGet2D(images[(int)pres.val[0]], (int)pres.val[2]+y, (int)pres.val[1]+x-PATCH_SIZE/2);
				cvSet2D(result, grid_r*(PATCH_SIZE/2)+y, x, pv);
				pres = CV_MAT_ELEM(*patch, CvScalar, grid_r-1, grid_c-1);
				pv = cvGet2D(images[(int)pres.val[0]], (int)pres.val[2]+y, (int)pres.val[1]+x);
				cvSet2D(result, grid_r*(PATCH_SIZE/2)+y, grid_c*(PATCH_SIZE/2)+x, pv);
			}
		for (int j = 1; j < grid_c; ++j)
		{
			CvScalar pres1 = CV_MAT_ELEM(*patch, CvScalar, 0, j-1);
			CvScalar pres2 = CV_MAT_ELEM(*patch, CvScalar, 0, j);
			CvScalar pres3 = CV_MAT_ELEM(*patch, CvScalar, grid_r-1, j-1);
			CvScalar pres4 = CV_MAT_ELEM(*patch, CvScalar, grid_r-1, j);
			int minx = j*(PATCH_SIZE/2);
			for (int y = 0; y < PATCH_SIZE/2; ++y)
				for (int x = 0; x < PATCH_SIZE/2; ++x)
				{
					CvScalar v[2];
					v[0] = cvGet2D(images[(int)pres1.val[0]], (int)pres1.val[2]+y-PATCH_SIZE/2, (int)pres1.val[1]+x);
					v[1] = cvGet2D(images[(int)pres2.val[0]], (int)pres2.val[2]+y-PATCH_SIZE/2, (int)pres2.val[1]+x-PATCH_SIZE/2);
					double w[2] = {pres1.val[3], pres2.val[3]};
					CvScalar pv = weighted_average(v, w, 2);
					cvSet2D(result, y, minx+x, pv);
					v[0] = cvGet2D(images[(int)pres3.val[0]], (int)pres3.val[2]+y, (int)pres3.val[1]+x);
					v[1] = cvGet2D(images[(int)pres4.val[0]], (int)pres4.val[2]+y, (int)pres4.val[1]+x-PATCH_SIZE/2);
					w[0] = pres3.val[3];
					w[0] = pres4.val[3];
					pv = weighted_average(v, w, 2);
					cvSet2D(result, grid_r*(PATCH_SIZE/2)+y, minx+x, pv);
				}
		}
		for (int i = 1; i < grid_r; ++i)
		{
			CvScalar pres1 = CV_MAT_ELEM(*patch, CvScalar, i-1, 0);
			CvScalar pres2 = CV_MAT_ELEM(*patch, CvScalar, i, 0);
			CvScalar pres3 = CV_MAT_ELEM(*patch, CvScalar, i-1, grid_c-1);
			CvScalar pres4 = CV_MAT_ELEM(*patch, CvScalar, i, grid_c-1);
			int miny = i*(PATCH_SIZE/2);
			for (int y = 0; y < PATCH_SIZE/2; ++y)
				for (int x = 0; x < PATCH_SIZE/2; ++x)
				{
					CvScalar v[2];
					v[0] = cvGet2D(images[(int)pres1.val[0]], (int)pres1.val[2]+y, (int)pres1.val[1]+x-PATCH_SIZE/2);
					v[1] = cvGet2D(images[(int)pres2.val[0]], (int)pres2.val[2]+y-PATCH_SIZE/2, (int)pres2.val[1]+x-PATCH_SIZE/2);
					double w[2] = {pres1.val[3], pres2.val[3]};
					CvScalar pv = weighted_average(v, w, 2);
					cvSet2D(result, miny+y, x, pv);
					v[0] = cvGet2D(images[(int)pres3.val[0]], (int)pres3.val[2]+y, (int)pres3.val[1]+x);
					v[1] = cvGet2D(images[(int)pres4.val[0]], (int)pres4.val[2]+y-PATCH_SIZE/2, (int)pres4.val[1]+x);
					w[0] = pres3.val[3];
					w[0] = pres4.val[3];
					pv = weighted_average(v, w, 2);
					if( pv.val[0] == 0 && pv.val[1] == 0 && pv.val[2] == 0 )
					{
						CvScalar replace = {{112 ,112 ,112 ,112}} ;
						cvSet2D(result, miny+y, x, replace);
						/*printf("it's black\n");*/
						continue ;
					}
					else 
						cvSet2D(result, miny+y, grid_c*(PATCH_SIZE/2)+x, pv);
				}
		}

		/*CvScalar replace = {{133,133,133,133}} ;*/
		/*for( int i = 0; i < result->height ; i++ )*/
		/*{*/
			/*for( int j = 0; j < result->width ; j++ )*/
			/*{*/
				/*CvScalar ele = cvGet2D(result , i , j) ;*/
				/*if( ele.val[0] == 0 && ele.val[1] == 0 && ele.val[2] == 0 )*/
				/*{*/
					/*printf("it's black\n");*/
					/*cvSet2D(result , i , j , replace) ;*/
				/*}*/
			/*}*/
			
		/*}*/
		
		cvShowImage("result", result);
		char name[16];
		sprintf(name, "result%d.png", n);
		cvSaveImage(name, result, NULL);
		sprintf(name, "origin%d.png", n);
		cvSaveImage(name, images[n], NULL);
		cvReleaseMat(&patch);
	}
	for (int i = 0; i < image_num; ++i)
	{
		cvReleaseImage(&trans[i]);
		cvReleaseImage(&trans_luck[i]);
		cvReleaseImage(&blur[i]);
	}
}
