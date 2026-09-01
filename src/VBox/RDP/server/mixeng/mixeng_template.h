/*
 * QEMU Mixing engine
 *
 * Copyright (c) 2004 Vassili Karpov (malc)
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

/*
 * Tusen tack till Mike Nordell
 * dec++'ified by Dscho
 */

#ifdef SIGNED
#define HALF IN_MAX
#define HALFT IN_MAX
#else
#define HALFT ((IN_MAX)>>1)
#define HALF HALFT
#endif

#ifdef NOVOL
#define VOL(a, b) a
#else
#define VOL(a, b) ((a) * (b)) >> 32
#endif

#ifdef OLD_WAY
static int64_t inline glue(conv_,IN_T) (IN_T v)
{
#ifdef SIGNED
    return (INT_MAX*(int64_t)v)/HALF;
#else
    return (INT_MAX*((int64_t)v-HALFT))/HALF;
#endif
}

static IN_T inline glue(clip_,IN_T) (int64_t v)
{
    if (v >= INT_MAX)
        return IN_MAX;
    else if (v < -INT_MAX)
        return IN_MIN;

#ifdef SIGNED
    return (IN_T) (v*HALF/INT_MAX);
#else
    return (IN_T) (v+INT_MAX/2)*HALF/INT_MAX;
#endif
}

#else  /* !OLD_WAY */

static inline int64_t glue(conv_,IN_T) (IN_T v)
{
#ifdef SIGNED
    return ((int64_t) v) << (31 - SHIFT);
#else
    return ((int64_t) v - HALFT) << (31 - SHIFT);
#endif
}

static inline IN_T glue(clip_,IN_T) (int64_t v)
{
    if (v >= INT_MAX) {
        return IN_MAX;
    }
    else if (v < -INT_MAX) {
        return IN_MIN;
    }

#ifdef SIGNED
    return (IN_T) (v >> (31 - SHIFT));
#else
    return (IN_T) (v >> (31 - SHIFT)) + HALFT;
#endif
}

#endif /* OLD_WAY */

static void glue(glue(conv_,IN_T),_to_stereo) (void *dst, const void *src,
                                               int samples, volume_t *vol)
{
    st_sample_t *out = (st_sample_t *) dst;
    IN_T *in = (IN_T *) src;
    if (vol->mute) {
        memset (dst, 0, sizeof (st_sample_t) * samples);
        return;
    }
    while (samples--) {
        out->l = VOL (glue(conv_,IN_T) (*in++), vol->l);
        out->r = VOL (glue(conv_,IN_T) (*in++), vol->r);
        out += 1;
    }
}

static void glue(glue(conv_,IN_T),_to_mono) (void *dst, const void *src,
                                             int samples, volume_t *vol)
{
    st_sample_t *out = (st_sample_t *) dst;
    IN_T *in = (IN_T *) src;
    if (vol->mute) {
        memset (dst, 0, sizeof (st_sample_t) * samples);
        return;
    }
    while (samples--) {
        out->l = VOL (glue(conv_,IN_T) (in[0]), vol->l);
        out->r = out->l;
        out += 1;
        in += 1;
    }
}

static void glue(glue(clip_,IN_T),_from_stereo) (void *dst, const void *src,
                                                 int samples)
{
    st_sample_t *in = (st_sample_t *) src;
    IN_T *out = (IN_T *) dst;
    while (samples--) {
        *out++ = glue(clip_,IN_T) (in->l);
        *out++ = glue(clip_,IN_T) (in->r);
        in += 1;
    }
}

static void glue(glue(clip_,IN_T),_from_mono) (void *dst, const void *src,
                                               int samples)
{
    st_sample_t *in = (st_sample_t *) src;
    IN_T *out = (IN_T *) dst;
    while (samples--) {
        *out++ = glue(clip_,IN_T) (in->l + in->r);
        in += 1;
    }
}

#undef HALF
#undef HALFT

