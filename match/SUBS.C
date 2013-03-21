
#include "common.h"
#include "encoder.h"

/*****************************************************************************
 ************************** Start of Subroutines *****************************
 *****************************************************************************/

/*****************************************************************************
 * 该程序计算 BLKSIZE 个采样数据的FFT。它的算法是基于Oppenheim 和 Schafer的  *
 *  《数字信号处理》一书。参考pages 304 (流图) 和330-332                 *
 *  若进行逆变换则将第20行改成                             *

 *                 w_imag[L] = sin(PI/le1);                                  *
 *                                                                           *
 *  要用到的常量为：                                                     *
 *         #define      PI          3.14159265358979                         *
 *         #define      BLKSIZE     1024                                     *
 *         #define      LOGBLKSIZE  10                                       *
 *         #define      BLKSIZE_S   256                                      *
 *         #define      LOGBLKSIZE_S 8                                       *
 *                                                                           *
 *****************************************************************************/
#define      BLKSIZE_S   256
#define      LOGBLKSIZE_S 8

void fft(x_real,x_imag, energy, phi, N)
FLOAT x_real[BLKSIZE], x_imag[BLKSIZE], energy[BLKSIZE], phi[BLKSIZE];
int	N;
{
 int     M,MM1;
 static int     init=0;
 int     NV2, NM1, MP;
 static double  w_real[2][LOGBLKSIZE], w_imag[2][LOGBLKSIZE];
 int            i,j,k,L;
 int            ip, le,le1;
 double         t_real, t_imag, u_real, u_imag;

 if(init==0) {
    memset((char *) w_real, 0, sizeof(w_real));  /* preset statics to 0 */
    memset((char *) w_imag, 0, sizeof(w_imag));  /* preset statics to 0 */
    M = LOGBLKSIZE;
    for(L=0; L<M; L++){
       le = 1 << (M-L);
       le1 = le >> 1;
       w_real[0][L] = cos(PI/le1);
       w_imag[0][L] = -sin(PI/le1);
    }          
    M = LOGBLKSIZE_S;
    for(L=0; L<M; L++){
       le = 1 << (M-L);
       le1 = le >> 1;
       w_real[1][L] = cos(PI/le1);
       w_imag[1][L] = -sin(PI/le1);
    }          
    init++;
 }
 switch(N) {
	case BLKSIZE:
			M = LOGBLKSIZE;
			MP = 0;
			break;
	case BLKSIZE_S:
			M = LOGBLKSIZE_S;
			MP = 1;
			break;
	default:	printf("Error: Bad FFT Size in subs.c\n");
			exit(30);
 }
 MM1 = M-1;
 NV2 = N >> 1;
 NM1 = N - 1;
 for(L=0; L<MM1; L++){
    le = 1 << (M-L);
    le1 = le >> 1;
    u_real = 1;
    u_imag = 0;
    for(j=0; j<le1; j++){
       for(i=j; i<N; i+=le){
          ip = i + le1;
          t_real = x_real[i] + x_real[ip];
          t_imag = x_imag[i] + x_imag[ip];
          x_real[ip] = x_real[i] - x_real[ip];
          x_imag[ip] = x_imag[i] - x_imag[ip];
          x_real[i] = t_real;
          x_imag[i] = t_imag;
          t_real = x_real[ip];
          x_real[ip] = x_real[ip]*u_real - x_imag[ip]*u_imag;
          x_imag[ip] = x_imag[ip]*u_real + t_real*u_imag;
       }
       t_real = u_real;
       u_real = u_real*w_real[MP][L] - u_imag*w_imag[MP][L];
       u_imag = u_imag*w_real[MP][L] + t_real*w_imag[MP][L];
    }
 }
 for(i=0; i<N; i+=2){
    ip = i + 1;
    t_real = x_real[i] + x_real[ip];
    t_imag = x_imag[i] + x_imag[ip];
    x_real[ip] = x_real[i] - x_real[ip];
    x_imag[ip] = x_imag[i] - x_imag[ip];
    x_real[i] = t_real;
    x_imag[i] = t_imag;
    energy[i] = x_real[i]*x_real[i] + x_imag[i]*x_imag[i];
    if(energy[i] <= 0.0005){phi[i] = 0;energy[i] = 0.0005;}
    else phi[i] = atan2((double) x_imag[i],(double) x_real[i]);
    energy[ip] = x_real[ip]*x_real[ip] + x_imag[ip]*x_imag[ip];
    if(energy[ip] == 0)phi[ip] = 0;
    else phi[ip] = atan2((double) x_imag[ip],(double) x_real[ip]);
 }
 j = 0;
 for(i=0; i<NM1; i++){
    if(i<j){
       t_real = x_real[j];
       t_imag = x_imag[j];
       x_real[j] = x_real[i];
       x_imag[j] = x_imag[i];
       x_real[i] = t_real;
       x_imag[i] = t_imag;
       t_real = energy[j];
       energy[j] = energy[i];
       energy[i] = t_real;
       t_real = phi[j];
       phi[j] = phi[i];
       phi[i] = t_real;
    }
    k=NV2;
    while(k<=j){
       j = j-k;
       k = k >> 1;
    }
    j = j+k;
 }
}
