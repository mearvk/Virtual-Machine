/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
void NAME (void *opaque, st_sample_t *ibuf, st_sample_t *obuf,
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
            OP (obuf[i].l, ibuf[i].l);
            OP (obuf[i].r, ibuf[i].r);
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
            ilast = *ibuf++;
            rate->ipos++;
            /* See if we finished the input buffer yet */
            if (ibuf >= iend) goto the_end;
        }

        icur = *ibuf;

        /* interpolate */
        t = rate->opos & 0xffffffff;
        out.l = (ilast.l * (INT64_C(0x100000000) - t) + icur.l * t) >> 32;
        out.r = (ilast.r * (INT64_C(0x100000000) - t) + icur.r * t) >> 32;

        /* output sample & increment position */
        OP (obuf->l, out.l);
        OP (obuf->r, out.r);
        obuf += 1;
        rate->opos += rate->opos_inc;
    }

the_end:
    *isamp = ibuf - istart;
    *osamp = obuf - ostart;
    rate->ilast = ilast;
}

#undef NAME
#undef OP
