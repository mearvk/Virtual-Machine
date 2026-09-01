/*
 * QEMU Mixing engine
 *
 * Copyright (c) 2004-2005 Vassili Karpov (malc)
 * Copyright (c) 1998 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <limits.h>

#include <iprt/alloc.h>


#ifndef glue
# define _glue(x, y)    x ## y
# define glue(x, y)     _glue(x, y)
# define tostring(s)    #s
# define stringify(s)   tostring(s)
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline _inline
#endif

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#include "mixeng.h"

#define IN_T int8_t
#define IN_MIN SCHAR_MIN
#define IN_MAX SCHAR_MAX
#define SIGNED
#define SHIFT 7
#include "mixeng_template.h"
#undef SIGNED
#undef IN_MAX
#undef IN_MIN
#undef IN_T
#undef SHIFT

#define IN_T uint8_t
#define IN_MIN 0
#define IN_MAX UCHAR_MAX
#define SHIFT 8
#include "mixeng_template.h"
#undef IN_MAX
#undef IN_MIN
#undef IN_T
#undef SHIFT

#define IN_T int16_t
#define IN_MIN SHRT_MIN
#define IN_MAX SHRT_MAX
#define SIGNED
#define SHIFT 15
#include "mixeng_template.h"
#undef SIGNED
#undef IN_MAX
#undef IN_MIN
#undef IN_T
#undef SHIFT

#define IN_T uint16_t
#define IN_MIN 0
#define IN_MAX USHRT_MAX
#define SHIFT 16
#include "mixeng_template.h"
#undef IN_MAX
#undef IN_MIN
#undef IN_T
#undef SHIFT

t_sample *mixeng_conv[2][2][2] = {
    {
        {
            conv_uint8_t_to_mono,
            conv_uint16_t_to_mono
        },
        {
            conv_int8_t_to_mono,
            conv_int16_t_to_mono
        }
    },
    {
        {
            conv_uint8_t_to_stereo,
            conv_uint16_t_to_stereo
        },
        {
            conv_int8_t_to_stereo,
            conv_int16_t_to_stereo
        }
    }
};

f_sample *mixeng_clip[2][2][2] = {
    {
        {
            clip_uint8_t_from_mono,
            clip_uint16_t_from_mono
        },
        {
            clip_int8_t_from_mono,
            clip_int16_t_from_mono
        }
    },
    {
        {
            clip_uint8_t_from_stereo,
            clip_uint16_t_from_stereo
        },
        {
            clip_int8_t_from_stereo,
            clip_int16_t_from_stereo
        }
    }
};

/*
 * August 21, 1998
 * Copyright 1998 Fabrice Bellard.
 *
 * [Rewrote completly the code of Lance Norskog And Sundry
 * Contributors with a more efficient algorithm.]
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * Sound Tools rate change effect file.
 */
/*
 * Linear Interpolation.
 *
 * The use of fractional increment allows us to use no buffer. It
 * avoid the problems at the end of the buffer we had with the old
 * method which stored a possibly big buffer of size
 * lcm(in_rate,out_rate).
 *
 * Limited to 16 bit samples and sampling frequency <= 65535 Hz. If
 * the input & output frequencies are equal, a delay of one sample is
 * introduced.  Limited to processing 32-bit count worth of samples.
 *
 * 1 << FRAC_BITS evaluating to zero in several places.  Changed with
 * an (unsigned long) cast to make it safe.  MarkMLl 2/1/99
 */

/* Private data */
typedef struct ratestuff {
    uint64_t opos;
    uint64_t opos_inc;
    uint32_t ipos;              /* position in the input stream (integer) */
    st_sample_t ilast;          /* last sample in the input stream */

    st_sample_t *lastInputSample;    /* The input sample, which was read. */
    st_sample_t *lastReturnedSample; /* The (filtered or original) sample returned to the rate convertor. */

    int fFilter;

    int64_t xvl[7], xvr[7]; /* The filter input l/r channel. */
    int64_t yvl[7], yvr[7]; /* The filter output l/r channel. */
    st_sample_t filtered;   /* The filtered sample. */
} *rate_t;

/*
 * Prepare processing.
 */
void *st_rate_start (int inrate, int outrate, int fFilter)
{
    rate_t rate;

    if (inrate >= 65535 || outrate >= 65535) {
        return NULL;
    }

    rate = (rate_t) RTMemAllocZ (sizeof (struct ratestuff));

    if (!rate) {
        return NULL;
    }

    rate->opos = 0;

    /* increment */
    rate->opos_inc = ((uint64_t)inrate << 32) / outrate;

    rate->ipos = 0;
    rate->ilast.l = 0;
    rate->ilast.r = 0;

    rate->lastInputSample = NULL;
    rate->lastReturnedSample = NULL;
    rate->fFilter = fFilter;

    return rate;
}

void st_rate_update (void *opaque, int inrate, int outrate)
{
    rate_t rate = (rate_t) opaque;

    /* increment */
    rate->opos_inc = ((uint64_t)inrate << 32) / outrate;
}

#define NAME st_rate_flow_mix
#define OP(a, b) a += b
#include "rate_template.h"

#define NAME st_rate_flow
#define OP(a, b) a = b
#include "rate_template.h"

/* Butterworth filter: 6th order, freq = 0.2 sample rate, that is 8820 at 44100.
 *
 *  x[n] = input / 96.96617316;
 *
 *  y[n] = (  1 * x[n- 6])
 *       + (  6 * x[n- 5])
 *       + ( 15 * x[n- 4])
 *       + ( 20 * x[n- 3])
 *       + ( 15 * x[n- 2])
 *       + (  6 * x[n- 1])
 *       + (  1 * x[n- 0])
 *
 *       + ( -0.0050225266 * y[n- 6])
 *       + (  0.0517530339 * y[n- 5])
 *       + ( -0.2634693483 * y[n- 4])
 *       + (  0.6743275253 * y[n- 3])
 *       + ( -1.3052133493 * y[n- 2])
 *       + (  1.1876006802 * y[n- 1]);
 *
 *   output = y[n];
 *
 * Converted to fixed point with 1024 scale.
 */
static int64_t filter_bw_lp_0_2(int64_t x, int64_t *xv, int64_t *yv)
{
    xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5]; xv[5] = xv[6];
    xv[6] = x * 10;
    yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5]; yv[5] = yv[6];
    yv[6] =   (xv[0] + xv[6]) + 6 * (xv[1] + xv[5]) + 15 * (xv[2] + xv[4]) + 20 * xv[3]
                 + (  (    -5 * yv[0]) + (    53 * yv[1])
                    + (  -270 * yv[2]) + (   690 * yv[3])
                    + ( -1336 * yv[4]) + (  1216 * yv[5])) / 1024;
    return yv[6] / 1024;
}

static st_sample_t *st_rate_filter(rate_t rate, st_sample_t *x)
{
    if (!rate->fFilter)
        return x;

    rate->filtered.l = filter_bw_lp_0_2(x->l, rate->xvl, rate->yvl);
    rate->filtered.r = filter_bw_lp_0_2(x->r, rate->xvr, rate->yvr);
    return &rate->filtered;
}

static st_sample_t *st_rate_get_sample(rate_t rate, st_sample_t *s)
{
    /* Make sure that the filter is applied to each sample only once. */
    if (s != rate->lastInputSample)
    {
        rate->lastInputSample = s;
        rate->lastReturnedSample = st_rate_filter(rate, s);
    }

    return rate->lastReturnedSample;
}

void st_rate_flow_ex (void *opaque, st_sample_t *ibuf, st_sample_t *obuf,
                      int *isamp, int *osamp)
{
    rate_t rate = (rate_t) opaque;
    st_sample_t *istart, *iend;
    st_sample_t *ostart, *oend;
    st_sample_t ilast, icur, out;
    int64_t t;

    ilast = rate->ilast;

    istart = ibuf;
    iend = ibuf + *isamp;

    ostart = obuf;
    oend = obuf + *osamp;

    if (rate->opos_inc == 1ULL << 32) {
        int i, n = *isamp > *osamp ? *osamp : *isamp;
        for (i = 0; i < n; i++) {
            /* Original: obuf[i] = ibuf[i]; */
            obuf[i] = *st_rate_get_sample(rate, &ibuf[i]);
        }
        *isamp = n;
        *osamp = n;
        return;
    }

    while (obuf < oend) {

        /* Safety catch to make sure we have input samples.  */
        if (ibuf >= iend)
            break;

        /* read as many input samples so that ipos > opos */

        while (rate->ipos <= (rate->opos >> 32)) {
            /* Original: ilast = *ibuf++; */
            ilast = *st_rate_get_sample(rate, ibuf++);
            rate->ipos++;
            /* See if we finished the input buffer yet */
            if (ibuf >= iend) goto the_end;
        }

        /* Original: icur = *ibuf; */
        icur = *st_rate_get_sample(rate, ibuf);

        /* interpolate */
        t = rate->opos & 0xffffffff;
        out.l = (ilast.l * (INT64_C(0x100000000) - t) + icur.l * t) >> 32;
        out.r = (ilast.r * (INT64_C(0x100000000) - t) + icur.r * t) >> 32;

        /* output sample & increment position */
        *obuf = out;
        obuf += 1;
        rate->opos += rate->opos_inc;
    }

the_end:
    *isamp = ibuf - istart;
    *osamp = obuf - ostart;
    rate->ilast = ilast;
}

void st_rate_stop (void *opaque)
{
    RTMemFree (opaque);
}
