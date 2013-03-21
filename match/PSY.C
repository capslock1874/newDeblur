/* psy.c 心理声学模型 */

#include "common.h"
#include "encoder.h"
#include "absthr_1.h"

void psycho_anal(buffer,savebuf,chn,lay,snr32,sfreq)
short int *buffer;
short int savebuf[1056];
int   chn, lay;
FLOAT snr32[32];
double sfreq;        
{
 unsigned int   i, j, k;
 FLOAT          r_prime, phi_prime;
 FLOAT          freq_mult, bval_lo, minthres, sum_energy;
 double         tb, temp1, temp2, temp3;



 static int     new = 0, old = 1, oldest = 0;
 static int     init = 0, flush, sync_flush, syncsize, sfreq_idx;


 static double  nmt = 5.5;

 static FLOAT   crit_band[27] = {0,  100,  200, 300, 400, 510, 630,  770,
                               920, 1080, 1270,1480,1720,2000,2320, 2700,
                              3150, 3700, 4400,5300,6400,7700,9500,12000,
                             15500,25000,30000};

 static FLOAT   bmax[27] = {20.0, 20.0, 20.0, 20.0, 20.0, 17.0, 15.0,
                            10.0,  7.0,  4.4,  4.5,  4.5,  4.5,  4.5,
                             4.5,  4.5,  4.5,  4.5,  4.5,  4.5,  4.5,
                             4.5,  4.5,  4.5,  3.5,  3.5,  3.5};

/* 下面的指针变量指向mem_alloc()函数分配的动态内存
/*                                   */

 FLOAT          *grouped_c, *grouped_e, *nb, *cb, *ecb, *bc;
 FLOAT          *wsamp_r, *wsamp_i, *phi, *energy;
 FLOAT          *c, *fthr;
 F32            *snrtmp;

 static int     *numlines;
 static int     *partition;
 static FLOAT   *cbval, *rnorm;
 static FLOAT   *window;
 static FLOAT   *absthr;
 static double  *tmn;
 static FCB     *s;
 static FHBLK   *lthr;
 static F2HBLK  *r, *phi_sav;



 grouped_c = (FLOAT *) mem_alloc(sizeof(FCB), "grouped_c");
 grouped_e = (FLOAT *) mem_alloc(sizeof(FCB), "grouped_e");
 nb = (FLOAT *) mem_alloc(sizeof(FCB), "nb");
 cb = (FLOAT *) mem_alloc(sizeof(FCB), "cb");
 ecb = (FLOAT *) mem_alloc(sizeof(FCB), "ecb");
 bc = (FLOAT *) mem_alloc(sizeof(FCB), "bc");
 wsamp_r = (FLOAT *) mem_alloc(sizeof(FBLK), "wsamp_r");
 wsamp_i = (FLOAT *) mem_alloc(sizeof(FBLK), "wsamp_i");
 phi = (FLOAT *) mem_alloc(sizeof(FBLK), "phi");
 energy = (FLOAT *) mem_alloc(sizeof(FBLK), "energy");
 c = (FLOAT *) mem_alloc(sizeof(FHBLK), "c");
 fthr = (FLOAT *) mem_alloc(sizeof(FHBLK), "fthr");
 snrtmp = (F32 *) mem_alloc(sizeof(F2_32), "snrtmp");

 if(init==0){


     numlines = (int *) mem_alloc(sizeof(ICB), "numlines");
     partition = (int *) mem_alloc(sizeof(IHBLK), "partition");
     cbval = (FLOAT *) mem_alloc(sizeof(FCB), "cbval");
     rnorm = (FLOAT *) mem_alloc(sizeof(FCB), "rnorm");
     window = (FLOAT *) mem_alloc(sizeof(FBLK), "window");
     absthr = (FLOAT *) mem_alloc(sizeof(FHBLK), "absthr");
     tmn = (double *) mem_alloc(sizeof(DCBB), "tmn");
     s = (FCB *) mem_alloc(sizeof(FCBCB), "s");
     lthr = (FHBLK *) mem_alloc(sizeof(F2HBLK), "lthr");
     r = (F2HBLK *) mem_alloc(sizeof(F22HBLK), "r");
     phi_sav = (F2HBLK *) mem_alloc(sizeof(F22HBLK), "phi_sav");

     i = (unsigned int)(sfreq + 0.5);
     switch(i){
        //case 32000: sfreq_idx = 0; break;
        case 44100: sfreq_idx = 1; break;
        //case 48000: sfreq_idx = 2; break;
        default:    printf("error, invalid sampling frequency: %d Hz\n",i);
        exit(24);
     }
     //printf("absthr[] sampling frequency index: %d\n",sfreq_idx);
     read_absthr(absthr, sfreq_idx);
     if(lay==1){
        flush = 384;
        syncsize = 1024;
        sync_flush = 576;
     }
     else {
        flush = (int)(384*3.0/2.0);
        syncsize = 1056;
        sync_flush = syncsize - flush;
     }
     for(i=0;i<BLKSIZE;i++)window[i]=0.5*(1-cos(2.0*PI*(i-0.5)/BLKSIZE));
     for(i=0;i<HBLKSIZE;i++){
        r[0][0][i]=r[1][0][i]=r[0][1][i]=r[1][1][i]=0;
        phi_sav[0][0][i]=phi_sav[1][0][i]=0;
        phi_sav[0][1][i]=phi_sav[1][1][i]=0;
        lthr[0][i] = 60802371420160.0;
        lthr[1][i] = 60802371420160.0;
     }
/*****************************************************************************
 * 下面进行初始化*
 *    partition[HBLKSIZE] = 每个频率线的分割数*
 *    cbval[CBANDS]       = 每个分割的中心bark值*
 *                                                        *
 *    numlines[CBANDS]    = 每个分割的频率数*
 *    tmn[CBANDS]         = 音调掩蔽噪声*
 *****************************************************************************/
     freq_mult = sfreq/BLKSIZE;
 
     for(i=0;i<HBLKSIZE;i++){
        temp1 = i*freq_mult;
        j = 1;
        while(temp1>crit_band[j])j++;
        fthr[i]=j-1+(temp1-crit_band[j-1])/(crit_band[j]-crit_band[j-1]);
     }
     partition[0] = 0;
/* temp2 is the counter of the number of frequency lines in each partition */
     temp2 = 1;
     cbval[0]=fthr[0];
     bval_lo=fthr[0];
     for(i=1;i<HBLKSIZE;i++){
        if((fthr[i]-bval_lo)>0.33){
           partition[i]=partition[i-1]+1;
           cbval[partition[i-1]] = cbval[partition[i-1]]/temp2;
           cbval[partition[i]] = fthr[i];
           bval_lo = fthr[i];
           numlines[partition[i-1]] = (int)temp2;
           temp2 = 1;
        }
        else {
           partition[i]=partition[i-1];
           cbval[partition[i]] += fthr[i];
           temp2++;
        }
     }
     numlines[partition[i-1]] = (int)temp2;
     cbval[partition[i-1]] = cbval[partition[i-1]]/temp2;
 
/************************************************************************
 * 下面计算传播函数
 ************************************************************************/
     for(j=0;j<CBANDS;j++){
        for(i=0;i<CBANDS;i++){
           temp1 = (cbval[i] - cbval[j])*1.05;
           if(temp1>=0.5 && temp1<=2.5){
              temp2 = temp1 - 0.5;
              temp2 = 8.0 * (temp2*temp2 - 2.0 * temp2);
           }
           else temp2 = 0;
           temp1 += 0.474;
           temp3 = 15.811389+7.5*temp1-17.5*sqrt((double) (1.0+temp1*temp1));
           if(temp3 <= -100) s[i][j] = 0;
           else {
              temp3 = (temp2 + temp3)*LN_TO_LOG10;
              s[i][j] = exp(temp3);
           }
        }
     }

  /* Calculate Tone Masking Noise values */
     for(j=0;j<CBANDS;j++){
        temp1 = 15.5 + cbval[j];
        tmn[j] = (temp1>24.5) ? temp1 : 24.5;
  /* Calculate normalization factors for the net spreading functions */
        rnorm[j] = 0;
        for(i=0;i<CBANDS;i++){
           rnorm[j] += s[j][i];
        }
     }
     init++;
 }
 
/************************* End of Initialization *****************************/
 switch(lay) {
  case 1:
  case 2:
     for(i=0; i<(unsigned)lay; i++){
        for(j=0; j<(unsigned)syncsize; j++){
           if(j<((unsigned)sync_flush))savebuf[j] = savebuf[j+flush];
           else savebuf[j] = *buffer++;
           if(j<BLKSIZE){
/**window data with HANN window***********************************************/
              wsamp_r[j] = window[j]*((FLOAT) savebuf[j]);
              wsamp_i[j] = 0;
           }
        }
/**Compute FFT****************************************************************/
        fft(wsamp_r,wsamp_i,energy,phi,1024);
         if(lay==2 || (lay==1 && chn==0) ){
           if(new==0){new = 1; oldest = 1;}
           else {new = 0; oldest = 0;}
           if(old==0)old = 1; else old = 0;
        }
        for(j=0; j<HBLKSIZE; j++){
           r_prime = 2.0 * r[chn][old][j] - r[chn][oldest][j];
           phi_prime = 2.0 * phi_sav[chn][old][j] - phi_sav[chn][oldest][j];
           r[chn][new][j] = sqrt((double) energy[j]);
           phi_sav[chn][new][j] = phi[j];
temp1=r[chn][new][j] * cos((double) phi[j]) - r_prime * cos((double) phi_prime);
temp2=r[chn][new][j] * sin((double) phi[j]) - r_prime * sin((double) phi_prime);
           temp3=r[chn][new][j] + fabs((double)r_prime);
           if(temp3 != 0)c[j]=sqrt(temp1*temp1+temp2*temp2)/temp3;
           else c[j] = 0;
        }
        for(j=1;j<CBANDS;j++){
           grouped_e[j] = 0;
           grouped_c[j] = 0;
        }
        grouped_e[0] = energy[0];
        grouped_c[0] = energy[0]*c[0];
        for(j=1;j<HBLKSIZE;j++){
           grouped_e[partition[j]] += energy[j];
           grouped_c[partition[j]] += energy[j]*c[j];
        }
        for(j=0;j<CBANDS;j++){
           ecb[j] = 0;
           cb[j] = 0;
           for(k=0;k<CBANDS;k++){
              if(s[j][k] != 0.0){
                 ecb[j] += s[j][k]*grouped_e[k];
                 cb[j] += s[j][k]*grouped_c[k];
              }
           }
           if(ecb[j] !=0)cb[j] = cb[j]/ecb[j];
           else cb[j] = 0;
        }
/*****************************************************************************
 * 为每个频率计算SNR
 *****************************************************************************/
        for(j=0;j<CBANDS;j++){
           if(cb[j]<.05)cb[j]=0.05;
           else if(cb[j]>.5)cb[j]=0.5;
           tb = -0.434294482*log((double) cb[j])-0.301029996;
           bc[j] = tmn[j]*tb + nmt*(1.0-tb);
           k = (unsigned int)(cbval[j] + 0.5);
           bc[j] = (bc[j] > bmax[k]) ? bc[j] : bmax[k];
           bc[j] = exp((double) -bc[j]*LN_TO_LOG10);
        }
        for(j=0;j<CBANDS;j++)
           if(rnorm[j] && numlines[j])
              nb[j] = ecb[j]*bc[j]/(rnorm[j]*numlines[j]);
           else nb[j] = 0;
        for(j=0;j<HBLKSIZE;j++){
/*temp1 is the preliminary threshold */
           temp1=nb[partition[j]];
           temp1=(temp1>absthr[j])?temp1:absthr[j];
           if(lay==1){
              fthr[j] = (temp1 < lthr[chn][j]) ? temp1 : lthr[chn][j];
              temp2 = temp1 * 0.00316;
              fthr[j] = (temp2 > fthr[j]) ? temp2 : fthr[j];
           }
           else fthr[j] = temp1;
           lthr[chn][j] = LXMIN*temp1;
        }
        for(j=0;j<193;j += 16){
           minthres = 60802371420160.0;
           sum_energy = 0.0;
           for(k=0;k<17;k++){
              if(minthres>fthr[j+k])minthres = fthr[j+k];
              sum_energy += energy[j+k];
           }
           snrtmp[i][j/16] = sum_energy/(minthres * 17.0);
           snrtmp[i][j/16] = 4.342944819 * log((double)snrtmp[i][j/16]);
        }
        for(j=208;j<(HBLKSIZE-1);j += 16){
           minthres = 0.0;
           sum_energy = 0.0;
           for(k=0;k<17;k++){
              minthres += fthr[j+k];
              sum_energy += energy[j+k];
           }
           snrtmp[i][j/16] = sum_energy/minthres;
           snrtmp[i][j/16] = 4.342944819 * log((double)snrtmp[i][j/16]);
        }
     }
     for(i=0; i<32; i++){
        if(lay==2)
           snr32[i]=(snrtmp[0][i]>snrtmp[1][i])?snrtmp[0][i]:snrtmp[1][i];
        else snr32[i]=snrtmp[0][i];
     }
     break;
  case 3:
     printf("layer 3 is not currently supported\n");
     break;
  default:
     printf("error, invalid MPEG/audio coding layer: %d\n",lay);
 }


 mem_free((void **) &grouped_c);
 mem_free((void **) &grouped_e);
 mem_free((void **) &nb);
 mem_free((void **) &cb);
 mem_free((void **) &ecb);
 mem_free((void **) &bc);
 mem_free((void **) &wsamp_r);
 mem_free((void **) &wsamp_i);
 mem_free((void **) &phi);
 mem_free((void **) &energy);
 mem_free((void **) &c);
 mem_free((void **) &fthr);
 mem_free((void **) &snrtmp);
}


void read_absthr(absthr, table)
FLOAT *absthr;
int table;
{
 long j;
 
 switch(table)
 {
    case 1 :
		 for(j=0; j<HBLKSIZE; j++)
             absthr[j] = absthr_1[j];
		 break;
    default : printf("absthr table: Not valid table number\n");
 }
}
