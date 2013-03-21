/* conform.c, conformance checks                                            */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

/*
 * 4/4/97 - John Schlichther
 *
 * extensively altered to create avi2mpg1 - avi to mpeg-1 encoder
 *
 * Since avi file, and the avi subsystem are platform dependant, cross
 * platform compatibility removed, many optional features disabled or
 * removed, code generally trimmed to a minimum.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "global.h"

/* check for (level independent) parameter limits */
void range_checks()
{
  int i;

  /* range and value checks */

  if (horizontal_size<1 || horizontal_size>16383)
    error("horizontal_size must be between 1 and 16383");
  if (horizontal_size>4095)
    error("horizontal_size must be less than 4096 (MPEG-1)");
  if ((horizontal_size&4095)==0)
    error("horizontal_size must not be a multiple of 4096");
  if (chroma_format!=CHROMA444 && horizontal_size%2 != 0)
    error("horizontal_size must be a even (4:2:0 / 4:2:2)");

  if (vertical_size<1 || vertical_size>16383)
    error("vertical_size must be between 1 and 16383");
  if (vertical_size>4095)
    error("vertical size must be less than 4096 (MPEG-1)");
  if ((vertical_size&4095)==0)
    error("vertical_size must not be a multiple of 4096");
  if (chroma_format==CHROMA420 && vertical_size%2 != 0)
    error("vertical_size must be a even (4:2:0)");
  if(fieldpic)
  {
    if (vertical_size%2 != 0)
      error("vertical_size must be a even (field pictures)");
    if (chroma_format==CHROMA420 && vertical_size%4 != 0)
      error("vertical_size must be a multiple of 4 (4:2:0 field pictures)");
  }

    if (aspectratio<1 || aspectratio>14)
      error("pel_aspect_ratio must be between 1 and 14 (MPEG-1)");

  if (frame_rate_code<1 || frame_rate_code>8)
    error("frame_rate code must be between 1 and 8");

  if (bit_rate<=0.0)
    error("bit_rate must be positive");
  if (bit_rate > ((1<<30)-1)*400.0)
    error("bit_rate must be less than 429 Gbit/s");
  if (bit_rate > ((1<<18)-1)*400.0)
    error("bit_rate must be less than 104 Mbit/s (MPEG-1)");

  if (vbv_buffer_size<1 || vbv_buffer_size>0x3ffff)
    error("vbv_buffer_size must be in range 1..(2^18-1)");
  if (vbv_buffer_size>=1024)
    error("vbv_buffer_size must be less than 1024 (MPEG-1)");

  if (chroma_format<CHROMA420 || chroma_format>CHROMA444)
    error("chroma_format must be in range 1...3");

  if (video_format<0 || video_format>4)
    error("video_format must be in range 0...4");

  if (color_primaries<1 || color_primaries>7 || color_primaries==3)
    error("color_primaries must be in range 1...2 or 4...7");

  if (transfer_characteristics<1 || transfer_characteristics>7
      || transfer_characteristics==3)
    error("transfer_characteristics must be in range 1...2 or 4...7");

  if (matrix_coefficients<1 || matrix_coefficients>7 || matrix_coefficients==3)
    error("matrix_coefficients must be in range 1...2 or 4...7");

  if (display_horizontal_size<0 || display_horizontal_size>16383)
    error("display_horizontal_size must be in range 0...16383");
  if (display_vertical_size<0 || display_vertical_size>16383)
    error("display_vertical_size must be in range 0...16383");

  if (dc_prec<0 || dc_prec>3)
    error("intra_dc_precision must be in range 0...3");

  for (i=0; i<M; i++)
  {
    if (motion_data[i].forw_hor_f_code<1 || motion_data[i].forw_hor_f_code>9)
      error("f_code must be between 1 and 9");
    if (motion_data[i].forw_vert_f_code<1 || motion_data[i].forw_vert_f_code>9)
      error("f_code must be between 1 and 9");
    if (motion_data[i].forw_hor_f_code>7)
      error("f_code must be le less than 8");
    if (motion_data[i].forw_vert_f_code>7)
      error("f_code must be le less than 8");
    if (motion_data[i].sxf<=0)
      error("search window must be positive"); /* doesn't belong here */
    if (motion_data[i].syf<=0)
      error("search window must be positive");
    if (i!=0)
    {
      if (motion_data[i].back_hor_f_code<1 || motion_data[i].back_hor_f_code>9)
        error("f_code must be between 1 and 9");
      if (motion_data[i].back_vert_f_code<1 || motion_data[i].back_vert_f_code>9)
        error("f_code must be between 1 and 9");
      if (motion_data[i].back_hor_f_code>7)
        error("f_code must be le less than 8");
      if (motion_data[i].back_vert_f_code>7)
        error("f_code must be le less than 8");
      if (motion_data[i].sxb<=0)
        error("search window must be positive");
      if (motion_data[i].syb<=0)
        error("search window must be positive");
    }
  }
}

/* identifies valid profile / level combinations */
static char profile_level_defined[5][4] =
{
/* HL   H-14 ML   LL  */
  {1,   1,   1,   0},  /* HP   */
  {0,   1,   0,   0},  /* Spat */
  {0,   0,   1,   1},  /* SNR  */
  {1,   1,   1,   1},  /* MP   */
  {0,   0,   1,   0}   /* SP   */
};

static struct level_limits {
  int hor_f_code;
  int vert_f_code;
  int hor_size;
  int vert_size;
  int sample_rate;
  int bit_rate; /* Mbit/s */
  int vbv_buffer_size; /* 16384 bit steps */
} maxval_tab[4] =
{
  {9, 5, 1920, 1152, 62668800, 80, 597}, /* HL */
  {9, 5, 1440, 1152, 47001600, 60, 448}, /* H-14 */
  {8, 5,  720,  576, 10368000, 15, 112}, /* ML */
  {7, 4,  352,  288,  3041280,  4,  29}  /* LL */
};

//#define SP   5
//#define MP   4
//#define SNR  3
//#define SPAT 2
//#define HP   1

//#define LL  10
//#define ML   8
//#define H14  6
//#define HL   4

