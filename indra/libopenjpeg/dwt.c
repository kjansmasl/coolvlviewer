/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux and Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2007, Jonathan Ballard <dzonatas@dzonux.net>
 * Copyright (c) 2007, Callum Lerwick <seg@haxxed.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if SSE2NEON
# include <stdlib.h>
# include "sse2neon.h"
/* Since we emulate SSE with NEON, let's make sure __SSE__ is defined */
# if !defined(__SSE__)
#  define __SSE__ 1
# endif
#else
# include <immintrin.h>
#endif

/* MSVC does not define __SSE__... */
#if !defined(__SSE__) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
# define __SSE__ 1
#endif

#include "opj_includes.h"

/** @defgroup DWT DWT - Implementation of a discrete wavelet transform */
/*@{*/

/** @name Local data structures */
/*@{*/

typedef struct dwt_local {
	int* mem;
	int dn;
	int sn;
	int cas;
} dwt_t;

typedef union {
	float	f[4];
} v4;

typedef struct v4dwt_local {
	v4*	wavelet ;
	int		dn ;
	int		sn ;
	int		cas ;
} v4dwt_t ;

static const float dwt_alpha =  1.586134342f; //  12994
static const float dwt_beta  =  0.052980118f; //    434
static const float dwt_gamma = -0.882911075f; //  -7233
static const float dwt_delta = -0.443506852f; //  -3633

static const float K      = 1.230174105f; //  10078
/* FIXME: What is this constant? */
static const float c13318 = 1.625732422f;

/*@}*/

/**
Virtual function type for wavelet transform in 1-D
*/
typedef void (*DWT1DFN)(dwt_t* v);

/** @name Local static functions */
/*@{*/

/**
Forward lazy transform (horizontal)
*/
static void dwt_deinterleave_h(int* a, int* b, int dn, int sn, int cas);
/**
Forward lazy transform (vertical)
*/
static void dwt_deinterleave_v(int* a, int* b, int dn, int sn, int x, int cas);
/**
Inverse lazy transform (horizontal)
*/
static void dwt_interleave_h(dwt_t* h, int* a);
/**
Inverse lazy transform (vertical)
*/
static void dwt_interleave_v(dwt_t* v, int* a, int x);
/**
Forward 5-3 wavelet transform in 1-D
*/
static void dwt_encode_1(int* a, int dn, int sn, int cas);
/**
Inverse 5-3 wavelet transform in 1-D
*/
static void dwt_decode_1(dwt_t* v);
/**
Forward 9-7 wavelet transform in 1-D
*/
static void dwt_encode_1_real(int* a, int dn, int sn, int cas);
/**
Explicit calculation of the Quantization Stepsizes
*/
static void dwt_encode_stepsize(int stepsize, int numbps, opj_stepsize_t* bandno_stepsize);
/**
Inverse wavelet transform in 2-D.
*/
static void dwt_decode_tile(opj_tcd_tilecomp_t* tilec, int i, DWT1DFN fn);

/*@}*/

/*@}*/

#define S(i) a[(i)*2]
#define D(i) a[(1+(i)*2)]
#define S_(i) ((i)<0?S(0):((i)>=sn?S(sn-1):S(i)))
#define D_(i) ((i)<0?D(0):((i)>=dn?D(dn-1):D(i)))
/* new */
#define SS_(i) ((i)<0?S(0):((i)>=dn?S(dn-1):S(i)))
#define DD_(i) ((i)<0?D(0):((i)>=sn?D(sn-1):D(i)))

/* <summary>                                                              */
/* This table contains the norms of the 5-3 wavelets for different bands. */
/* </summary>                                                             */
static const double dwt_norms[4][10] = {
	{1.000, 1.500, 2.750, 5.375, 10.68, 21.34, 42.67, 85.33, 170.7, 341.3},
	{1.038, 1.592, 2.919, 5.703, 11.33, 22.64, 45.25, 90.48, 180.9},
	{1.038, 1.592, 2.919, 5.703, 11.33, 22.64, 45.25, 90.48, 180.9},
	{.7186, .9218, 1.586, 3.043, 6.019, 12.01, 24.00, 47.97, 95.93}
};

/* <summary>                                                              */
/* This table contains the norms of the 9-7 wavelets for different bands. */
/* </summary>                                                             */
static const double dwt_norms_real[4][10] = {
	{1.000, 1.965, 4.177, 8.403, 16.90, 33.84, 67.69, 135.3, 270.6, 540.9},
	{2.022, 3.989, 8.355, 17.04, 34.27, 68.63, 137.3, 274.6, 549.0},
	{2.022, 3.989, 8.355, 17.04, 34.27, 68.63, 137.3, 274.6, 549.0},
	{2.080, 3.865, 8.307, 17.18, 34.71, 69.59, 139.3, 278.6, 557.2}
};

/*
==========================================================
   local functions
==========================================================
*/

/* <summary>			                 */
/* Forward lazy transform (horizontal).  */
/* </summary>                            */
static void dwt_deinterleave_h(int* a, int* b, int dn, int sn, int cas) {
	int i;
    for (i=0; i<sn; i++) b[i]=a[2*i+cas];
    for (i=0; i<dn; i++) b[sn+i]=a[(2*i+1-cas)];
}

/* <summary>                             */
/* Forward lazy transform (vertical).    */
/* </summary>                            */
static void dwt_deinterleave_v(int* a, int* b, int dn, int sn, int x, int cas) {
    int i;
    for (i=0; i<sn; i++) b[i*x]=a[2*i+cas];
    for (i=0; i<dn; i++) b[(sn+i)*x]=a[(2*i+1-cas)];
}

/* <summary>                             */
/* Inverse lazy transform (horizontal).  */
/* </summary>                            */
static void dwt_interleave_h(dwt_t* h, int* a) {
    int* ai = a;
    int* bi = h->mem + h->cas;
    int  i	= h->sn;
    while( i-- ) {
      *bi = *(ai++);
	  bi += 2;
    }
    ai	= a + h->sn;
    bi	= h->mem + 1 - h->cas;
    i	= h->dn ;
    while( i-- ) {
      *bi = *(ai++);
	  bi += 2;
    }
}

/* <summary>                             */
/* Inverse lazy transform (vertical).    */
/* </summary>                            */
static void dwt_interleave_v(dwt_t* v, int* a, int x) {
    int* ai = a;
    int* bi = v->mem + v->cas;
    int i = v->sn;
    while( i-- ) {
      *bi = *ai;
	  bi += 2;
	  ai += x;
    }
    ai = a + (v->sn * x);
    bi = v->mem + 1 - v->cas;
    i = v->dn ;
    while( i-- ) {
      *bi = *ai;
	  bi += 2;
	  ai += x;
    }
}


/* <summary>                             */
/* Forward 5-3 wavelet transform in 1-D. */
/* </summary>                            */
static void dwt_encode_1(int* a, int dn, int sn, int cas) {
	if (!cas) {
		if (dn > 0 || sn > 1) {				/* NEW: CASE ONE ELEMENT */
			int i;
			for (i = 0; i < dn; i++) {
				D(i) -= (S_(i) + S_(i + 1)) >> 1;
			}
			for (i = 0; i < sn; i++) {
				S(i) += (D_(i - 1) + D_(i) + 2) >> 2;
			}
		}
	} else if (!sn && dn == 1) {	    	/* NEW: CASE ONE ELEMENT */
		S(0) *= 2;
	} else {
		int i;
		for (i = 0; i < dn; i++) {
			S(i) -= (DD_(i) + DD_(i - 1)) >> 1;
		}
		for (i = 0; i < sn; i++) {
			D(i) += (SS_(i) + SS_(i + 1) + 2) >> 2;
		}
	}
}

/* <summary>                             */
/* Inverse 5-3 wavelet transform in 1-D. */
/* </summary>                            */
static void dwt_decode_1_(int* a, int dn, int sn, int cas) {
	int i;

	if (!cas) {
		if (dn > 0 || sn > 1) {				/* NEW: CASE ONE ELEMENT */
			for (i = 0; i < sn; i++) {
				S(i) -= (D_(i - 1) + D_(i) + 2) >> 2;
			}
			for (i = 0; i < dn; i++) {
				D(i) += (S_(i) + S_(i + 1)) >> 1;
			}
		}
	} else if (!sn  && dn == 1) {			/* NEW: CASE ONE ELEMENT */
			S(0) /= 2;
	} else {
		for (i = 0; i < sn; i++) {
			D(i) -= (SS_(i) + SS_(i + 1) + 2) >> 2;
		}
		for (i = 0; i < dn; i++) {
			S(i) += (DD_(i) + DD_(i - 1)) >> 1;
		}
	}
}

/* <summary>                             */
/* Inverse 5-3 wavelet transform in 1-D. */
/* </summary>                            */
static void dwt_decode_1(dwt_t* v) {
	dwt_decode_1_(v->mem, v->dn, v->sn, v->cas);
}

/* <summary>                             */
/* Forward 9-7 wavelet transform in 1-D. */
/* </summary>                            */
static void dwt_encode_1_real(int* a, int dn, int sn, int cas) {
	int i;
	if (!cas) {
		if (dn > 0 || sn > 1) {		/* NEW :  CASE ONE ELEMENT */
			for (i = 0; i < dn; ++i) {
				D(i) -= fix_mul(S_(i) + S_(i + 1), 12993);
			}
			for (i = 0; i < sn; ++i) {
				S(i) -= fix_mul(D_(i - 1) + D_(i), 434);
			}
			for (i = 0; i < dn; ++i) {
				D(i) += fix_mul(S_(i) + S_(i + 1), 7233);
			}
			for (i = 0; i < sn; ++i) {
				S(i) += fix_mul(D_(i - 1) + D_(i), 3633);
			}
			for (i = 0; i < dn; ++i) {
				D(i) = fix_mul(D(i), 5038);	/*5038 */
			}
			for (i = 0; i < sn; ++i) {
				S(i) = fix_mul(S(i), 6659);	/*6660 */
			}
		}
	} else if (sn > 0 || dn > 1) {	/* NEW :  CASE ONE ELEMENT */
		for (i = 0; i < dn; ++i) {
			S(i) -= fix_mul(DD_(i) + DD_(i - 1), 12993);
		}
		for (i = 0; i < sn; ++i) {
			D(i) -= fix_mul(SS_(i) + SS_(i + 1), 434);
		}
		for (i = 0; i < dn; ++i) {
			S(i) += fix_mul(DD_(i) + DD_(i - 1), 7233);
		}
		for (i = 0; i < sn; ++i) {
			D(i) += fix_mul(SS_(i) + SS_(i + 1), 3633);
		}
		for (i = 0; i < dn; ++i) {
			S(i) = fix_mul(S(i), 5038);	/*5038 */
		}
		for (i = 0; i < sn; ++i) {
			D(i) = fix_mul(D(i), 6659);	/*6660 */
		}
	}
}

static void dwt_encode_stepsize(int stepsize, int numbps, opj_stepsize_t* bandno_stepsize) {
	int p = int_floorlog2(stepsize) - 13;
	int n = 11 - int_floorlog2(stepsize);
	bandno_stepsize->mant = (n < 0 ? stepsize >> -n : stepsize << n) & 0x7ff;
	bandno_stepsize->expn = numbps - p;
}

/*
==========================================================
   DWT interface
==========================================================
*/

/* <summary>                             */
/* Forward 5-3 wavelet transform in 2-D. */
/* </summary>                            */
void dwt_encode(opj_tcd_tilecomp_t* tilec) {
	int w = tilec->x1-tilec->x0;
	int l = tilec->numresolutions-1;
	int* a = tilec->data;
	int i, j, k;
	for (i = 0; i < l; ++i) {
		/* width of the resolution level computed */
		int rw = tilec->resolutions[l - i].x1 - tilec->resolutions[l - i].x0;
		/* height of the resolution level computed */
		int rh = tilec->resolutions[l - i].y1 - tilec->resolutions[l - i].y0;
		/* width of the resolution level once lower than computed one */
		int rw1= tilec->resolutions[l - i - 1].x1 - tilec->resolutions[l - i - 1].x0;
		/* height of the resolution level once lower than computed one */
		int rh1= tilec->resolutions[l - i - 1].y1 - tilec->resolutions[l - i - 1].y0;

		/* 0 = non inversion on horizontal filtering 1 = inversion between low-pass and high-pass filtering */
		int cas_row = tilec->resolutions[l - i].x0 % 2;
		/* 0 = non inversion on vertical filtering 1 = inversion between low-pass and high-pass filtering */
		int cas_col = tilec->resolutions[l - i].y0 % 2;

		int sn = rh1;
		int dn = rh - rh1;
		int* bj = (int*)opj_malloc(rh * sizeof(int));
		if (!bj) {
			/* Memory allocation failure... */
			return;
		}
		for (j = 0; j < rw; j++) {
			int* aj = a + j;
			for (k = 0; k < rh; k++)  bj[k] = aj[k*w];
			dwt_encode_1(bj, dn, sn, cas_col);
			dwt_deinterleave_v(bj, aj, dn, sn, w, cas_col);
		}
		opj_free(bj);

		sn = rw1;
		dn = rw - rw1;
		bj = (int*)opj_malloc(rw * sizeof(int));
		if (!bj) {
			/* Memory allocation failure... */
			return;
		}
		for (j = 0; j < rh; j++) {
			int* aj = a + j * w;
			for (k = 0; k < rw; k++)  bj[k] = aj[k];
			dwt_encode_1(bj, dn, sn, cas_row);
			dwt_deinterleave_h(bj, aj, dn, sn, cas_row);
		}
		opj_free(bj);
	}
}


/* <summary>                             */
/* Inverse 5-3 wavelet transform in 2-D. */
/* </summary>                            */
void dwt_decode(opj_tcd_tilecomp_t* tilec, int numres) {
	dwt_decode_tile(tilec, numres, &dwt_decode_1);
}


/* <summary>                          */
/* Get gain of 5-3 wavelet transform. */
/* </summary>                         */
int dwt_getgain(int orient) {
	if (orient == 0)
		return 0;
	if (orient == 1 || orient == 2)
		return 1;
	return 2;
}

/* <summary>                */
/* Get norm of 5-3 wavelet. */
/* </summary>               */
double dwt_getnorm(int level, int orient) {
	return dwt_norms[orient][level];
}

/* <summary>                             */
/* Forward 9-7 wavelet transform in 2-D. */
/* </summary>                            */

void dwt_encode_real(opj_tcd_tilecomp_t* tilec) {
	int w = tilec->x1-tilec->x0;
	int l = tilec->numresolutions-1;
	int* a = tilec->data;
	int i, j, k;
	for (i = 0; i < l; i++) {
		/* width of the resolution level computed */
		int rw = tilec->resolutions[l - i].x1 - tilec->resolutions[l - i].x0;
		/* height of the resolution level computed */
		int rh = tilec->resolutions[l - i].y1 - tilec->resolutions[l - i].y0;
		/* width of the resolution level once lower than computed one */
		int rw1= tilec->resolutions[l - i - 1].x1 - tilec->resolutions[l - i - 1].x0;
		/* height of the resolution level once lower than computed one */
		int rh1= tilec->resolutions[l - i - 1].y1 - tilec->resolutions[l - i - 1].y0;

		/* 0 = non inversion on horizontal filtering 1 = inversion between low-pass and high-pass filtering */
		int cas_row = tilec->resolutions[l - i].x0 % 2;
		/* 0 = non inversion on vertical filtering 1 = inversion between low-pass and high-pass filtering */
		int cas_col = tilec->resolutions[l - i].y0 % 2;

		int sn = rh1;
		int dn = rh - rh1;
		int* bj = (int*)opj_malloc(rh * sizeof(int));
		if (!bj) {
			/* Memory allocation failure... */
			return;
		}
		for (j = 0; j < rw; j++) {
			int* aj = a + j;
			for (k = 0; k < rh; k++)  bj[k] = aj[k*w];
			dwt_encode_1_real(bj, dn, sn, cas_col);
			dwt_deinterleave_v(bj, aj, dn, sn, w, cas_col);
		}
		opj_free(bj);

		sn = rw1;
		dn = rw - rw1;
		bj = (int*)opj_malloc(rw * sizeof(int));
		if (!bj) {
			/* Memory allocation failure... */
			return;
		}
		for (j = 0; j < rh; j++) {
			int* aj = a + j * w;
			for (k = 0; k < rw; k++)  bj[k] = aj[k];
			dwt_encode_1_real(bj, dn, sn, cas_row);
			dwt_deinterleave_h(bj, aj, dn, sn, cas_row);
		}
		opj_free(bj);
	}
}


/* <summary>                          */
/* Get gain of 9-7 wavelet transform. */
/* </summary>                         */
int dwt_getgain_real(int orient) {
	(void)orient;
	return 0;
}

/* <summary>                */
/* Get norm of 9-7 wavelet. */
/* </summary>               */
double dwt_getnorm_real(int level, int orient) {
	return dwt_norms_real[orient][level];
}

void dwt_calc_explicit_stepsizes(opj_tccp_t * tccp, int prec) {
	int bandno;
	int numbands = 3 * tccp->numresolutions - 2;
	for (bandno = 0; bandno < numbands; bandno++) {
		int resno = bandno == 0 ? 0 : (bandno - 1) / 3 + 1;
		int orient = bandno == 0 ? 0 : (bandno - 1) % 3 + 1;
		int level = tccp->numresolutions - 1 - resno;
		int gain = tccp->qmfbid == 0 ? 0 : (orient == 0 ? 0 : (orient == 1 || orient == 2 ? 1 : 2));
		double stepsize;
		if (tccp->qntsty == J2K_CCP_QNTSTY_NOQNT) {
			stepsize = 1.0;
		} else {
			double norm = dwt_norms_real[orient][level];
			stepsize = (1 << (gain)) / norm;
		}
		dwt_encode_stepsize((int)floor(stepsize * 8192.0), prec + gain, &tccp->stepsizes[bandno]);
	}
}

/* <summary>                             */
/* Determine maximum computed resolution level for inverse wavelet transform */
/* </summary>                            */
static int dwt_decode_max_resolution(opj_tcd_resolution_t* restrict r, int i) {
	int mr = 1;
	int w;
	while (--i) {
		++r;
		if (mr < (w = r->x1 - r->x0)) {
			mr = w;
		}
		if (mr < (w = r->y1 - r->y0)) {
			mr = w;
		}
	}
	return mr;
}

/* <summary>                             */
/* Inverse wavelet transform in 2-D.     */
/* </summary>                            */
static void dwt_decode_tile(opj_tcd_tilecomp_t* tilec, int numres, DWT1DFN dwt_1D) {
	opj_tcd_resolution_t* tr = tilec->resolutions;

	int rw = tr->x1 - tr->x0;	/* width of the resolution level computed */
	int rh = tr->y1 - tr->y0;	/* height of the resolution level computed */

	int w = tilec->x1 - tilec->x0;

	dwt_t h;
	h.mem = (int*)opj_aligned_malloc(dwt_decode_max_resolution(tr, numres) * sizeof(int));
	if (!h.mem) {
		/* Memory allocation failure... */
		return;
	}
	dwt_t v;
	v.mem = h.mem;

	int* restrict tiledp = tilec->data;
	while (--numres) {
		h.sn = rw;
		v.sn = rh;

		++tr;
		rw = tr->x1 - tr->x0;
		rh = tr->y1 - tr->y0;

		h.dn = rw - h.sn;
		h.cas = tr->x0 % 2;

		int j = 0;
		int jw = 0;	/* j * w */
		while (1) {
			dwt_interleave_h(&h, &tiledp[jw]);
			(dwt_1D)(&h);
			memcpy(&tiledp[jw], h.mem, rw * sizeof(int));
			if (++j >= rh) {
				break;
			}
			jw += w;
		}

		v.dn = rh - v.sn;
		v.cas = tr->y0 % 2;

		for (j = 0; j < rw; ++j) {
			dwt_interleave_v(&v, &tiledp[j], w);
			(dwt_1D)(&v);
			int k = 0;
			int kwj = j; /* k * w + j */
			while (1) {
				tiledp[kwj] = v.mem[k];
				if (++k >= rh) {
					break;
				}
				kwj += w;
			}
		}
	}
	opj_aligned_free(h.mem);
}

static void v4dwt_interleave_h(v4dwt_t* restrict w, float* restrict a, int x, int size) {
	float* restrict bi = (float*) (w->wavelet + w->cas);
	int count = w->sn;
	int i, k;
	if ((x & 0x0f) == 0) {
		for (k = 0; k < 2; ++k) {
			if (((size_t)a & 0x0f) == 0 &&
				((size_t)bi & 0x0f) == 0 &&
				count + 3 * x < size) {
				/* Fast code path */
				for (i = 0; i < count; ++i) {
					float* restrict bk = &bi[i << 3];
					bk[0] = a[i];
					int j = i + x;
					bk[1] = a[j];
					j += x;
					bk[2] = a[j];
					j += x;
					bk[3] = a[j];
				}
			} else {
				/* Slow code path */
				for (i = 0; i < count; ++i) {
					float* restrict bk = &bi[i << 3];
					bk[0] = a[i];
					int j = i + x;
					if (j > size) continue;
					bk[1] = a[j];
					j += x;
					if (j > size) continue;
					bk[2] = a[j];
					j += x;
					if (j > size) continue;
					bk[3] = a[j];
				}
			}
			bi = (float*)(w->wavelet + 1 - w->cas);
			a += w->sn;
			size -= w->sn;
			count = w->dn;
		}
	} else {
		/* Slow code path */
		for (k = 0; k < 2; ++k) {
			for (i = 0; i < count; ++i) {
				float* restrict bk = &bi[i << 3];
				bk[0] = a[i];
				int j = i + x;
				if (j > size) continue;
				bk[1] = a[j];
				j += x;
				if (j > size) continue;
				bk[2] = a[j];
				j += x;
				if (j > size) continue;
				bk[3] = a[j];
			}
			bi = (float*)(w->wavelet + 1 - w->cas);
			a += w->sn;
			size -= w->sn;
			count = w->dn;
		}
	}
}

static void v4dwt_interleave_v(v4dwt_t* restrict v , float* restrict a , int x) {
	v4* restrict bi = v->wavelet + v->cas;
	int i;
	int ix = 0; /* i * x */
	for (i = 0; i < v->sn; ++i) {
		memcpy(&bi[i + i], &a[ix], 4 * sizeof(float));
		ix += x;
	}
	a += v->sn * x;
	bi = v->wavelet + 1 - v->cas;
	ix = 0; /* i * x */
	for (i = 0; i < v->dn; ++i) {
		memcpy(&bi[i + i], &a[ix], 4 * sizeof(float));
		ix += x;
	}
}

#ifdef __SSE__

static void v4dwt_decode_step1_sse(v4* w, int count, const __m128 c) {
	__m128* restrict vw = (__m128*)w;
	int i;
	int end = count >> 2;
	/* 4x unrolled loop */
	for (i = 0; i < end; ++i) {
		*vw = _mm_mul_ps(*vw, c);
		vw += 2;
		*vw = _mm_mul_ps(*vw, c);
		vw += 2;
		*vw = _mm_mul_ps(*vw, c);
		vw += 2;
		*vw = _mm_mul_ps(*vw, c);
		vw += 2;
	}
	count &= 3;
	for (i = 0; i < count; ++i) {
		*vw = _mm_mul_ps(*vw, c);
		vw += 2;
	}
}

static void v4dwt_decode_step2_sse(v4* l, v4* w, int k, int m, __m128 c) {
	__m128* restrict vl = (__m128*)l;
	__m128* restrict vw = (__m128*)w;
	__m128 tmp1, tmp2, tmp3;
	tmp1 = vl[0];
	int i;
	/* 4x loop unrolling */
	for (i = 0; i < m - 3; i += 4) {
		__m128 tmp4, tmp5, tmp6, tmp7, tmp8, tmp9;
		tmp2 = vw[-1];
		tmp3 = vw[0];
		tmp4 = vw[1];
		tmp5 = vw[2];
		tmp6 = vw[3];
		tmp7 = vw[4];
		tmp8 = vw[5];
		tmp9 = vw[6];
		vw[-1] = _mm_add_ps(tmp2, _mm_mul_ps(_mm_add_ps(tmp1, tmp3), c));
		vw[1] = _mm_add_ps(tmp4, _mm_mul_ps(_mm_add_ps(tmp3, tmp5), c));
		vw[3] = _mm_add_ps(tmp6, _mm_mul_ps(_mm_add_ps(tmp5, tmp7), c));
		vw[5] = _mm_add_ps(tmp8, _mm_mul_ps(_mm_add_ps(tmp7, tmp9), c));
		tmp1 = tmp9;
		vw += 8;
	}
	for ( ; i < m; ++i) {
		tmp2 = vw[-1];
		tmp3 = vw[0];
		vw[-1] = _mm_add_ps(tmp2, _mm_mul_ps(_mm_add_ps(tmp1, tmp3), c));
		tmp1 = tmp3;
		vw += 2;
	}
	if (m >= k) {
		return;
	}
	vl = vw - 2;
	c = _mm_add_ps(c, c);
	c = _mm_mul_ps(c, vl[0]);
	for ( ; m < k; ++m) {
		__m128 tmp = vw[-1];
		vw[-1] = _mm_add_ps(tmp, c);
		vw += 2;
	}
}

#else

static void v4dwt_decode_step1(v4* w, int count, const float c) {
	float* restrict fw = (float*) w;
	int i;
	for (i = 0; i < count; ++i) {
		float tmp1 = fw[i * 8 ];
		float tmp2 = fw[i * 8 + 1];
		float tmp3 = fw[i * 8 + 2];
		float tmp4 = fw[i * 8 + 3];
		fw[i * 8] = tmp1 * c;
		fw[i * 8 + 1] = tmp2 * c;
		fw[i * 8 + 2] = tmp3 * c;
		fw[i * 8 + 3] = tmp4 * c;
	}
}

static void v4dwt_decode_step2(v4* l, v4* w, int k, int m, float c) {
	float* restrict fl = (float*) l;
	float* restrict fw = (float*) w;
	int i;
	for (i = 0; i < m; ++i) {
		float tmp1_1 = fl[0];
		float tmp1_2 = fl[1];
		float tmp1_3 = fl[2];
		float tmp1_4 = fl[3];
		float tmp2_1 = fw[-4];
		float tmp2_2 = fw[-3];
		float tmp2_3 = fw[-2];
		float tmp2_4 = fw[-1];
		float tmp3_1 = fw[0];
		float tmp3_2 = fw[1];
		float tmp3_3 = fw[2];
		float tmp3_4 = fw[3];
		fw[-4] = tmp2_1 + ((tmp1_1 + tmp3_1) * c);
		fw[-3] = tmp2_2 + ((tmp1_2 + tmp3_2) * c);
		fw[-2] = tmp2_3 + ((tmp1_3 + tmp3_3) * c);
		fw[-1] = tmp2_4 + ((tmp1_4 + tmp3_4) * c);
		fl = fw;
		fw += 8;
	}
	if (m < k) {
		float c1;
		float c2;
		float c3;
		float c4;
		c += c;
		c1 = fl[0] * c;
		c2 = fl[1] * c;
		c3 = fl[2] * c;
		c4 = fl[3] * c;
		for (; m < k; ++m) {
			float tmp1 = fw[-4];
			float tmp2 = fw[-3];
			float tmp3 = fw[-2];
			float tmp4 = fw[-1];
			fw[-4] = tmp1 + c1;
			fw[-3] = tmp2 + c2;
			fw[-2] = tmp3 + c3;
			fw[-1] = tmp4 + c4;
			fw += 8;
		}
	}
}

#endif

/* <summary>                             */
/* Inverse 9-7 wavelet transform in 1-D. */
/* </summary>                            */
static void v4dwt_decode(v4dwt_t* restrict dwt) {
	int a, b;
	if (dwt->cas == 0) {
		if (dwt->dn <= 0 && dwt->sn <= 1) {
			return;
		}
		a = 0;
		b = 1;
	} else {
		if (dwt->sn <= 0 && dwt->dn <= 1) {
			return;
		}
		a = 1;
		b = 0;
	}
	v4* restrict waveleta = dwt->wavelet + a;
	v4* restrict waveletb = dwt->wavelet + b;
#ifdef __SSE__
	v4dwt_decode_step1_sse(waveleta, dwt->sn, _mm_set1_ps(K));
	v4dwt_decode_step1_sse(waveletb, dwt->dn, _mm_set1_ps(c13318));
	v4dwt_decode_step2_sse(waveletb, waveleta + 1, dwt->sn, int_min(dwt->sn, dwt->dn-a), _mm_set1_ps(dwt_delta));
	v4dwt_decode_step2_sse(waveleta, waveletb + 1, dwt->dn, int_min(dwt->dn, dwt->sn-b), _mm_set1_ps(dwt_gamma));
	v4dwt_decode_step2_sse(waveletb, waveleta + 1, dwt->sn, int_min(dwt->sn, dwt->dn-a), _mm_set1_ps(dwt_beta));
	v4dwt_decode_step2_sse(waveleta, waveletb + 1, dwt->dn, int_min(dwt->dn, dwt->sn-b), _mm_set1_ps(dwt_alpha));
#else
	v4dwt_decode_step1(waveleta, dwt->sn, K);
	v4dwt_decode_step1(waveletb, dwt->dn, c13318);
	v4dwt_decode_step2(waveletb, waveleta + 1, dwt->sn, int_min(dwt->sn, dwt->dn-a), dwt_delta);
	v4dwt_decode_step2(waveleta, waveletb + 1, dwt->dn, int_min(dwt->dn, dwt->sn-b), dwt_gamma);
	v4dwt_decode_step2(waveletb, waveleta + 1, dwt->sn, int_min(dwt->sn, dwt->dn-a), dwt_beta);
	v4dwt_decode_step2(waveleta, waveletb + 1, dwt->dn, int_min(dwt->dn, dwt->sn-b), dwt_alpha);
#endif
}

/* <summary>                             */
/* Inverse 9-7 wavelet transform in 2-D. */
/* </summary>                            */
void dwt_decode_real(opj_tcd_tilecomp_t* restrict tilec, int numres) {
	v4dwt_t h;
	v4dwt_t v;

	opj_tcd_resolution_t* res = tilec->resolutions;

	int rw = res->x1 - res->x0;	/* width of the resolution level computed */
	int rh = res->y1 - res->y0;	/* height of the resolution level computed */

	h.wavelet = (v4*)opj_aligned_malloc((dwt_decode_max_resolution(res, numres) + 5) * sizeof(v4));
	if (!h.wavelet) {
		/* Memory allocation failure... */
		return;
	}
	v.wavelet = h.wavelet;

	const int w = tilec->x1 - tilec->x0;
	const int four_w = w << 2;
	const int bufsize = w * (tilec->y1 - tilec->y0);

	while (--numres) {
		float* restrict aj = (float*)tilec->data;
		int bufleft = bufsize;
		h.sn = rw;
		v.sn = rh;

		++res;

		rw = res->x1 - res->x0;	/* width of the resolution level computed */
		rh = res->y1 - res->y0;	/* height of the resolution level computed */

		h.dn = rw - h.sn;
		h.cas = res->x0 % 2;

		int j;
		for (j = rh; j > 3; j -= 4) {
			v4dwt_interleave_h(&h, aj, w, bufleft);
			v4dwt_decode(&h);
			int k;
			int kw; /* k + w * 1,2,3 */
			for (k = rw; --k >= 0; ) {
				aj[k] = h.wavelet[k].f[0];
				kw = k + w;
				aj[kw] = h.wavelet[k].f[1];
				kw += w;
				aj[kw] = h.wavelet[k].f[2];
				kw += w;
				aj[kw] = h.wavelet[k].f[3];
			}
			aj += four_w;
			bufleft -= four_w;
		}
		j = rh & 0x03;
		if (j) {
			v4dwt_interleave_h(&h, aj, w, bufleft);
			v4dwt_decode(&h);
			int k;
			switch (j) {
				case 3:
					for (k = rw; --k >= 0; ) {
						aj[k + (w + w)] = h.wavelet[k].f[2];
						aj[k + w] = h.wavelet[k].f[1];
						aj[k] = h.wavelet[k].f[0];
					}
					break;

				case 2:
					for (k = rw; --k >= 0; ) {
						aj[k + w] = h.wavelet[k].f[1];
						aj[k] = h.wavelet[k].f[0];
					}
					break;

				case 1:
					for (k = rw; --k >= 0; ) {
						aj[k] = h.wavelet[k].f[0];
					}
			}
		}

		v.dn = rh - v.sn;
		v.cas = res->y0 % 2;

		aj = (float*)tilec->data;
		for (j = rw; j > 3; j -= 4) {
			v4dwt_interleave_v(&v, aj, w);
			v4dwt_decode(&v);
			int k;
			int kw = 0; /* k * w */
			for (k = 0; k < rh; ++k) {
				memcpy(&aj[kw], &v.wavelet[k], 4 * sizeof(float));
				kw += w;
			}
			aj += 4;
		}
		j = rw & 0x03;
		if (j) {
			v4dwt_interleave_v(&v, aj, w);
			v4dwt_decode(&v);
			int k;
			int kw = 0; /* k * w */
			const int size = j * sizeof(float);
			for (k = 0; k < rh; ++k) {
				memcpy(&aj[kw], &v.wavelet[k], size);
				kw += w;
			}
		}
	}

	opj_aligned_free(h.wavelet);
}
