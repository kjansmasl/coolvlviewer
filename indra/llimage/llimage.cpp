/**
 * @file llimage.cpp
 * @brief Base class for images.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <algorithm>

#include "boost/preprocessor.hpp"

#include "llimage.h"

#include "llcolor4u.h"
#include "llmath.h"
#include "llimagebmp.h"
#include "llimagetga.h"
#include "llimagej2c.h"
#include "llimagejpeg.h"
#include "llimagepng.h"

///////////////////////////////////////////////////////////////////////////////
// Helper macros for generate cycle unwrap templates
///////////////////////////////////////////////////////////////////////////////

#define _UNROL_GEN_TPL_arg_0(arg)
#define _UNROL_GEN_TPL_arg_1(arg) arg

#define _UNROL_GEN_TPL_comma_0
#define _UNROL_GEN_TPL_comma_1 BOOST_PP_COMMA()

#define _UNROL_GEN_TPL_ARGS_macro(z,n,seq) \
	BOOST_PP_CAT(_UNROL_GEN_TPL_arg_, BOOST_PP_MOD(n, 2))(BOOST_PP_SEQ_ELEM(n, seq)) BOOST_PP_CAT(_UNROL_GEN_TPL_comma_, BOOST_PP_AND(BOOST_PP_MOD(n, 2), BOOST_PP_NOT_EQUAL(BOOST_PP_INC(n), BOOST_PP_SEQ_SIZE(seq))))

#define _UNROL_GEN_TPL_ARGS(seq) \
	BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(seq), _UNROL_GEN_TPL_ARGS_macro, seq)

#define _UNROL_GEN_TPL_TYPE_ARGS_macro(z,n,seq) \
	BOOST_PP_SEQ_ELEM(n, seq) BOOST_PP_CAT(_UNROL_GEN_TPL_comma_, BOOST_PP_AND(BOOST_PP_MOD(n, 2), BOOST_PP_NOT_EQUAL(BOOST_PP_INC(n), BOOST_PP_SEQ_SIZE(seq))))

#define _UNROL_GEN_TPL_TYPE_ARGS(seq) \
	BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(seq), _UNROL_GEN_TPL_TYPE_ARGS_macro, seq)

#define _UNROLL_GEN_TPL_foreach_ee(z, n, seq) \
	executor<n>(_UNROL_GEN_TPL_ARGS(seq));

#define _UNROLL_GEN_TPL(name, args_seq, operation, spec) \
	template<> struct name<spec> { \
	private: \
		template<S32 _idx> inline void executor(_UNROL_GEN_TPL_TYPE_ARGS(args_seq)) { \
			BOOST_PP_SEQ_ENUM(operation) ; \
		} \
	public: \
		inline void operator()(_UNROL_GEN_TPL_TYPE_ARGS(args_seq)) { \
			BOOST_PP_REPEAT(spec, _UNROLL_GEN_TPL_foreach_ee, args_seq) \
		} \
};

#define _UNROLL_GEN_TPL_foreach_seq_macro(r, data, elem) \
	_UNROLL_GEN_TPL(BOOST_PP_SEQ_ELEM(0, data), BOOST_PP_SEQ_ELEM(1, data), BOOST_PP_SEQ_ELEM(2, data), elem)

#define UNROLL_GEN_TPL(name, args_seq, operation, spec_seq) \
	/*general specialization - should not be implemented!*/ \
	template<U8> struct name { inline void operator()(_UNROL_GEN_TPL_TYPE_ARGS(args_seq)) { /*static_assert(!"Should not be instantiated.");*/  } }; \
	BOOST_PP_SEQ_FOR_EACH(_UNROLL_GEN_TPL_foreach_seq_macro, (name)(args_seq)(operation), spec_seq)


///////////////////////////////////////////////////////////////////////////////
// Generated unrolling loop templates with specializations
///////////////////////////////////////////////////////////////////////////////

// example: for (c = 0; c < ch; ++c) comp[c] = cx[0] = 0;
UNROLL_GEN_TPL(uroll_zeroze_cx_comp, (S32*)(cx)(S32*)(comp), (cx[_idx] = comp[_idx] = 0), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] >>= 4;
UNROLL_GEN_TPL(uroll_comp_rshftasgn_constval, (S32*)(comp)(S32)(cval), (comp[_idx] >>= cval), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] = (cx[c] >> 5) * yap;
UNROLL_GEN_TPL(uroll_comp_asgn_cx_rshft_cval_all_mul_val, (S32*)(comp)(S32*)(cx)(S32)(cval)(S32)(val), (comp[_idx] = (cx[_idx] >> cval) * val), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] += (cx[c] >> 5) * Cy;
UNROLL_GEN_TPL(uroll_comp_plusasgn_cx_rshft_cval_all_mul_val, (S32*)(comp)(S32*)(cx)(S32)(cval)(S32)(val), (comp[_idx] += (cx[_idx] >> cval) * val), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] += pix[c] * info.xapoints[x];
UNROLL_GEN_TPL(uroll_inp_plusasgn_pix_mul_val, (S32*)(comp)(const U8*)(pix)(S32)(val), (comp[_idx] += pix[_idx] * val), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) cx[c] = pix[c] * info.xapoints[x];
UNROLL_GEN_TPL(uroll_inp_asgn_pix_mul_val, (S32*)(comp)(const U8*)(pix)(S32)(val), (comp[_idx] = pix[_idx] * val), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] = ((cx[c] * info.yapoints[y]) + (comp[c] * (256 - info.yapoints[y]))) >> 16;
UNROLL_GEN_TPL(uroll_comp_asgn_cx_mul_apoint_plus_comp_mul_inv_apoint_allshifted_16_r, (S32*)(comp)(S32*)(cx)(S32)(apoint), (comp[_idx] = ((cx[_idx] * apoint) + (comp[_idx] * (256 - apoint))) >> 16), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] = (comp[c] + pix[c] * info.yapoints[y]) >> 8;
UNROLL_GEN_TPL(uroll_comp_asgn_comp_plus_pix_mul_apoint_allshifted_8_r, (S32*)(comp)(const U8*)(pix)(S32)(apoint), (comp[_idx] = (comp[_idx] + pix[_idx] * apoint) >> 8), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) comp[c] = ((comp[c]*(256 - info.xapoints[x])) + ((cx[c] * info.xapoints[x]))) >> 12;
UNROLL_GEN_TPL(uroll_comp_asgn_comp_mul_inv_apoint_plus_cx_mul_apoint_allshifted_12_r, (S32*)(comp)(S32)(apoint)(S32*)(cx), (comp[_idx] = ((comp[_idx] * (256 - apoint)) + (cx[_idx] * apoint)) >> 12), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) *dptr++ = comp[c] & 0xff;
UNROLL_GEN_TPL(uroll_uref_dptr_inc_asgn_comp_and_ff, (U8*&)(dptr)(S32*)(comp), (*dptr++ = comp[_idx] & 0xff), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) *dptr++ = (sptr[info.xpoints[x]*ch + c]) & 0xff;
UNROLL_GEN_TPL(uroll_uref_dptr_inc_asgn_sptr_apoint_plus_idx_alland_ff, (U8*&)(dptr)(const U8*)(sptr)(S32)(apoint), (*dptr++ = sptr[apoint + _idx] & 0xff), (1)(3)(4));
// example: for (c = 0; c < ch; ++c) *dptr++ = (comp[c]>>10) & 0xff;
UNROLL_GEN_TPL(uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff, (U8*&)(dptr)(S32*)(comp)(S32)(cval), (*dptr++ = (comp[_idx]>>cval) & 0xff), (1)(3)(4));

template<U8 ch>
class scale_info
{
public:
	std::vector<S32> xpoints;
	std::vector<const U8*> ystrides;
	std::vector<S32> xapoints;
	std::vector<S32> yapoints;
	S32 xup_yup;

public:
	// unrolling loop types declaration
	typedef uroll_zeroze_cx_comp<ch>														uroll_zeroze_cx_comp_t;
	typedef uroll_comp_rshftasgn_constval<ch>												uroll_comp_rshftasgn_constval_t;
	typedef uroll_comp_asgn_cx_rshft_cval_all_mul_val<ch>									uroll_comp_asgn_cx_rshft_cval_all_mul_val_t;
	typedef uroll_comp_plusasgn_cx_rshft_cval_all_mul_val<ch>								uroll_comp_plusasgn_cx_rshft_cval_all_mul_val_t;
	typedef uroll_inp_plusasgn_pix_mul_val<ch>												uroll_inp_plusasgn_pix_mul_val_t;
	typedef uroll_inp_asgn_pix_mul_val<ch>													uroll_inp_asgn_pix_mul_val_t;
	typedef uroll_comp_asgn_cx_mul_apoint_plus_comp_mul_inv_apoint_allshifted_16_r<ch>		uroll_comp_asgn_cx_mul_apoint_plus_comp_mul_inv_apoint_allshifted_16_r_t;
	typedef uroll_comp_asgn_comp_plus_pix_mul_apoint_allshifted_8_r<ch>						uroll_comp_asgn_comp_plus_pix_mul_apoint_allshifted_8_r_t;
	typedef uroll_comp_asgn_comp_mul_inv_apoint_plus_cx_mul_apoint_allshifted_12_r<ch>		uroll_comp_asgn_comp_mul_inv_apoint_plus_cx_mul_apoint_allshifted_12_r_t;
	typedef uroll_uref_dptr_inc_asgn_comp_and_ff<ch>										uroll_uref_dptr_inc_asgn_comp_and_ff_t;
	typedef uroll_uref_dptr_inc_asgn_sptr_apoint_plus_idx_alland_ff<ch>						uroll_uref_dptr_inc_asgn_sptr_apoint_plus_idx_alland_ff_t;
	typedef uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff<ch>								uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff_t;

public:
	scale_info(const U8* src,
			   U32 srcW, U32 srcH, U32 dstW, U32 dstH, U32 srcStride)
	:	xup_yup((dstW >= srcW) + ((dstH >= srcH) << 1))
	{
		calc_x_points(srcW, dstW);
		calc_y_strides(src, srcStride, srcH, dstH);
		calc_aa_points(srcW, dstW, xup_yup & 1, xapoints);
		calc_aa_points(srcH, dstH, xup_yup & 2, yapoints);
	}

private:
	void calc_x_points(U32 srcW, U32 dstW)
	{
		xpoints.resize(dstW + 1);

		S32 val = dstW >= srcW ? 0x8000 * srcW / dstW - 0x8000 : 0;
		S32 inc = (srcW << 16) / dstW;

		for (U32 i = 0, j = 0; i < dstW; ++i, ++j, val += inc)
		{
			xpoints[j] = llmax(0, val >> 16);
		}
	}

	void calc_y_strides(const U8* src, U32 srcStride, U32 srcH, U32 dstH)
	{
		ystrides.resize(dstH + 1);

		S32 val = dstH >= srcH ? 0x8000 * srcH / dstH - 0x8000 : 0;
		S32 inc = (srcH << 16) / dstH;

		for (U32 i = 0, j = 0; i < dstH; ++i, ++j, val += inc)
		{
			ystrides[j] = src + llmax(0, val >> 16) * srcStride;
		}
	}

	void calc_aa_points(U32 srcSz, U32 dstSz, bool scale_up, std::vector<S32>& vp)
	{
		vp.resize(dstSz);

		if (scale_up)
		{
			S32 val = 0x8000 * srcSz / dstSz - 0x8000;
			S32 inc = (srcSz << 16) / dstSz;
			U32 pos;

			for (U32 i = 0, j = 0; i < dstSz; ++i, ++j, val += inc)
			{
				pos = val >> 16;

				if (pos >= srcSz - 1)
				{
					vp[j] = 0;
				}
				else
				{
					S32 tmp = val >> 8;
					vp[j] = tmp - (tmp & 0xffffff00);
				}
			}
		}
		else
		{
			S32 inc = (srcSz << 16) / dstSz;
			S32 cp = (dstSz << 14) / srcSz + 1;
			S32 ap;

			for (U32 i = 0, j = 0, val = 0; i < dstSz; ++i, ++j, val += inc)
			{
				ap = ((0x100 - ((val >> 8) & 0xff)) * cp) >> 8;
				vp[j] = ap | (cp << 16);
			}
		}
	}
};

template<U8 ch>
LL_INLINE void bilinear_scale(const U8* src, U32 srcW, U32 srcH, U32 srcStride,
							  U8* dst, U32 dstW, U32 dstH, U32 dstStride)
{
	typedef scale_info<ch> scale_info_t;
	scale_info_t info(src, srcW, srcH, dstW, dstH, srcStride);

	const U8* sptr;
	const U8* pix;
	U8* dptr;
	U32 x, y;
	S32 cx[ch], comp[ch];

	if (info.xup_yup == 3)
	{
		// scale x/y - up
		for (y = 0; y < dstH; ++y)
		{
			dptr = dst + y * dstStride;
			sptr = info.ystrides[y];

			if (info.yapoints[y] > 0)
			{
				for (x = 0; x < dstW; ++x)
				{
					//for (c = 0; c < ch; ++c) cx[c] = comp[c] = 0;
					typename scale_info_t::uroll_zeroze_cx_comp_t()(cx, comp);

					if (info.xapoints[x] > 0)
					{
						pix = info.ystrides[y] + info.xpoints[x] * ch;

						//for (c = 0; c < ch; ++c) comp[c] = pix[c] * (256 - info.xapoints[x]);
						typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(comp, pix, 256 - info.xapoints[x]);

						pix += ch;

						//for (c = 0; c < ch; ++c) comp[c] += pix[c] * info.xapoints[x];
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(comp, pix, info.xapoints[x]);

						pix += srcStride;

						//for (c = 0; c < ch; ++c) cx[c] = pix[c] * info.xapoints[x];
						typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, info.xapoints[x]);

						pix -= ch;

						//for (c = 0; c < ch; ++c) {
						//	cx[c] += pix[c] * (256 - info.xapoints[x]);
						//	comp[c] = ((cx[c] * info.yapoints[y]) + (comp[c] * (256 - info.yapoints[y]))) >> 16;
						//	*dptr++ = comp[c] & 0xff;
						//}
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, 256 - info.xapoints[x]);
						typename scale_info_t::uroll_comp_asgn_cx_mul_apoint_plus_comp_mul_inv_apoint_allshifted_16_r_t()(comp, cx, info.yapoints[y]);
						typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_and_ff_t()(dptr, comp);
					}
					else
					{
						pix = info.ystrides[y] + info.xpoints[x] * ch;

						//for (c = 0; c < ch; ++c) comp[c] = pix[c] * (256 - info.yapoints[y]);
						typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(comp, pix, 256 - info.yapoints[y]);

						pix += srcStride;

						//for (c = 0; c < ch; ++c) {
						//	comp[c] = (comp[c] + pix[c] * info.yapoints[y]) >> 8;
						//	*dptr++ = comp[c] & 0xff;
						//}
						typename scale_info_t::uroll_comp_asgn_comp_plus_pix_mul_apoint_allshifted_8_r_t()(comp, pix, info.yapoints[y]);
						typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_and_ff_t()(dptr, comp);
					}
				}
			}
			else
			{
				for (x = 0; x < dstW; ++x)
				{
					if (info.xapoints[x] > 0)
					{
						pix = info.ystrides[y] + info.xpoints[x] * ch;

						//for (c = 0; c < ch; ++c) {
						//	comp[c] = pix[c] * (256 - info.xapoints[x]);
						//	comp[c] = (comp[c] + pix[c] * info.xapoints[x]) >> 8;
						//	*dptr++ = comp[c] & 0xff;
						//}
						typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(comp, pix, 256 - info.xapoints[x]);
						typename scale_info_t::uroll_comp_asgn_comp_plus_pix_mul_apoint_allshifted_8_r_t()(comp, pix, info.xapoints[x]);
						typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_and_ff_t()(dptr, comp);
					}
					else
					{
						//for (c = 0; c < ch; ++c) *dptr++ = (sptr[info.xpoints[x]*ch + c]) & 0xff;
						typename scale_info_t::uroll_uref_dptr_inc_asgn_sptr_apoint_plus_idx_alland_ff_t()(dptr, sptr, info.xpoints[x]*ch);
					}
				}
			}
		}
	}
	else if (info.xup_yup == 1)
	{
		// scaling down vertically
		S32 Cy, j;
		S32 yap;

		for (y = 0; y < dstH; ++y)
		{
			Cy = info.yapoints[y] >> 16;
			yap = info.yapoints[y] & 0xffff;

			dptr = dst + y * dstStride;

			for (x = 0; x < dstW; ++x)
			{
				pix = info.ystrides[y] + info.xpoints[x] * ch;

				//for (c = 0; c < ch; ++c) comp[c] = pix[c] * yap;
				typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(comp, pix, yap);

				pix += srcStride;

				for (j = (1 << 14) - yap; j > Cy; j -= Cy, pix += srcStride)
				{
					//for (c = 0; c < ch; ++c) comp[c] += pix[c] * Cy;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(comp, pix, Cy);
				}

				if (j > 0)
				{
					//for (c = 0; c < ch; ++c) comp[c] += pix[c] * j;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(comp, pix, j);
				}

				if (info.xapoints[x] > 0)
				{
					pix = info.ystrides[y] + info.xpoints[x]*ch + ch;
					//for (c = 0; c < ch; ++c) cx[c] = pix[c] * yap;
					typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, yap);

					pix += srcStride;
					for (j = (1 << 14) - yap; j > Cy; j -= Cy)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * Cy;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, Cy);
						pix += srcStride;
					}

					if (j > 0)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * j;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, j);
					}

					//for (c = 0; c < ch; ++c) comp[c] = ((comp[c]*(256 - info.xapoints[x])) + ((cx[c] * info.xapoints[x]))) >> 12;
					typename scale_info_t::uroll_comp_asgn_comp_mul_inv_apoint_plus_cx_mul_apoint_allshifted_12_r_t()(comp, info.xapoints[x], cx);
				}
				else
				{
					//for (c = 0; c < ch; ++c) comp[c] >>= 4;
					typename scale_info_t::uroll_comp_rshftasgn_constval_t()(comp, 4);
				}

				//for (c = 0; c < ch; ++c) *dptr++ = (comp[c]>>10) & 0xff;
				typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff_t()(dptr, comp, 10);
			}
		}
	}
	else if (info.xup_yup == 2)
	{
		// scaling down horizontally
		S32 Cx, j;
		S32 xap;

		for (y = 0; y < dstH; ++y)
		{
			dptr = dst + y * dstStride;

			for (x = 0; x < dstW; ++x)
			{
				Cx = info.xapoints[x] >> 16;
				xap = info.xapoints[x] & 0xffff;

				pix = info.ystrides[y] + info.xpoints[x] * ch;

				//for (c = 0; c < ch; ++c) comp[c] = pix[c] * xap;
				typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(comp, pix, xap);

				pix += ch;
				for (j = (1 << 14) - xap; j > Cx; j -= Cx)
				{
					//for (c = 0; c < ch; ++c) comp[c] += pix[c] * Cx;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(comp, pix, Cx);
					pix += ch;
				}

				if (j > 0)
				{
					//for (c = 0; c < ch; ++c) comp[c] += pix[c] * j;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(comp, pix, j);
				}

				if (info.yapoints[y] > 0)
				{
					pix = info.ystrides[y] + info.xpoints[x]*ch + srcStride;
					//for (c = 0; c < ch; ++c) cx[c] = pix[c] * xap;
					typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, xap);

					pix += ch;
					for (j = (1 << 14) - xap; j > Cx; j -= Cx)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * Cx;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, Cx);
						pix += ch;
					}

					if (j > 0)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * j;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, j);
					}

					//for (c = 0; c < ch; ++c) comp[c] = ((comp[c] * (256 - info.yapoints[y])) + ((cx[c] * info.yapoints[y]))) >> 12;
					typename scale_info_t::uroll_comp_asgn_comp_mul_inv_apoint_plus_cx_mul_apoint_allshifted_12_r_t()(comp, info.yapoints[y], cx);
				}
				else
				{
					//for (c = 0; c < ch; ++c) comp[c] >>= 4;
					typename scale_info_t::uroll_comp_rshftasgn_constval_t()(comp, 4);
				}

				//for (c = 0; c < ch; ++c) *dptr++ = (comp[c]>>10) & 0xff;
				typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff_t()(dptr, comp, 10);
			}
		}
	}
	else
	{
		// scale x/y - down
		S32 Cx, Cy, i, j;
		S32 xap, yap;

		for (y = 0; y < dstH; ++y)
		{
			Cy = info.yapoints[y] >> 16;
			yap = info.yapoints[y] & 0xffff;

			dptr = dst + y * dstStride;
			for (x = 0; x < dstW; ++x)
			{
				Cx = info.xapoints[x] >> 16;
				xap = info.xapoints[x] & 0xffff;

				sptr = info.ystrides[y] + info.xpoints[x] * ch;
				pix = sptr;
				sptr += srcStride;

				//for (c = 0; c < ch; ++c) cx[c] = pix[c] * xap;
				typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, xap);

				pix += ch;
				for (i = (1 << 14) - xap; i > Cx; i -= Cx)
				{
					//for (c = 0; c < ch; ++c) cx[c] += pix[c] * Cx;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, Cx);
					pix += ch;
				}

				if (i > 0)
				{
					//for (c = 0; c < ch; ++c) cx[c] += pix[c] * i;
					typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, i);
				}

				//for (c = 0; c < ch; ++c) comp[c] = (cx[c] >> 5) * yap;
				typename scale_info_t::uroll_comp_asgn_cx_rshft_cval_all_mul_val_t()(comp, cx, 5, yap);

				for (j = (1 << 14) - yap; j > Cy; j -= Cy)
				{
					pix = sptr;
					sptr += srcStride;

					//for (c = 0; c < ch; ++c) cx[c] = pix[c] * xap;
					typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, xap);

					pix += ch;
					for (i = (1 << 14) - xap; i > Cx; i -= Cx)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * Cx;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, Cx);
						pix += ch;
					}

					if (i > 0)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * i;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, i);
					}

					//for (c = 0; c < ch; ++c) comp[c] += (cx[c] >> 5) * Cy;
					typename scale_info_t::uroll_comp_plusasgn_cx_rshft_cval_all_mul_val_t()(comp, cx, 5, Cy);
				}

				if (j > 0)
				{
					pix = sptr;
					sptr += srcStride;

					//for (c = 0; c < ch; ++c) cx[c] = pix[c] * xap;
					typename scale_info_t::uroll_inp_asgn_pix_mul_val_t()(cx, pix, xap);

					pix += ch;
					for (i = (1 << 14) - xap; i > Cx; i -= Cx)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * Cx;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, Cx);
						pix += ch;
					}

					if (i > 0)
					{
						//for (c = 0; c < ch; ++c) cx[c] += pix[c] * i;
						typename scale_info_t::uroll_inp_plusasgn_pix_mul_val_t()(cx, pix, i);
					}

					//for (c = 0; c < ch; ++c) comp[c] += (cx[c] >> 5) * j;
					typename scale_info_t::uroll_comp_plusasgn_cx_rshft_cval_all_mul_val_t()(comp, cx, 5, j);
				}

				//for (c = 0; c < ch; ++c) *dptr++ = (comp[c]>>23) & 0xff;
				typename scale_info_t::uroll_uref_dptr_inc_asgn_comp_rshft_cval_and_ff_t()(dptr, comp, 23);
			}
		}
	}
}

// wrapper
static void bilinear_scale(const U8* src, U32 srcW, U32 srcH, U32 srcCh,
						   U32 srcStride, U8* dst, U32 dstW, U32 dstH,
						   U32 dstCh, U32 dstStride)
{
	llassert(srcCh == dstCh);

	switch (srcCh)
	{
		case 1:
			bilinear_scale<1>(src, srcW, srcH, srcStride, dst, dstW, dstH,
							  dstStride);
			break;

		case 3:
			bilinear_scale<3>(src, srcW, srcH, srcStride, dst, dstW, dstH,
							  dstStride);
			break;

		case 4:
			bilinear_scale<4>(src, srcW, srcH, srcStride, dst, dstW, dstH,
							  dstStride);
			break;

		default:
			llassert(false);
	}
}

//---------------------------------------------------------------------------
// LLImage
//---------------------------------------------------------------------------

// 5 Mb seems to be the required space to fit all requests from the main
// thread (I get 5136384 as the max requested size during full sessions)...
constexpr size_t TEMP_DATA_BUFFER_SIZE = 5 * 1024 * 1024; // 5 Mb

//static
std::string LLImage::sLastErrorMessage;
LLMutex* LLImage::sMutex = NULL;
#if LL_JEMALLOC
// Initialize with a sane value, in case our allocator gets called before the
// jemalloc arena for it is set.
U32 LLImage::sMallocxFlags = MALLOCX_TCACHE_NONE;
#endif
U8* LLImage::sTempDataBuffer = NULL;
U32 LLImage::sTempDataBufferUsageCount = 0;
U32 LLImage::sDynamicBufferAllocationsCount = 0;
S32 LLImage::sMaxMainThreadTempBufferSizeRequest = 0;

//static
void LLImage::initClass()
{
	sMutex = new LLMutex();

#if LL_JEMALLOC
	static unsigned int arena = 0;
	if (!arena)
	{
		size_t sz = sizeof(arena);
		if (mallctl("arenas.create", &arena, &sz, NULL, 0))
		{
			llwarns << "Failed to create a new jemalloc arena" << llendl;
		}
	}
	llinfos << "Using jemalloc arena " << arena << " for textures memory"
			<< llendl;

	sMallocxFlags = MALLOCX_ARENA(arena) | MALLOCX_TCACHE_NONE;
#endif

	if (!sTempDataBuffer)
	{
		// Note: use only this buffer from the main thread !
		sTempDataBuffer = (U8*)allocate_texture_mem(TEMP_DATA_BUFFER_SIZE *
													sizeof(U8));
	}
}

//static
void LLImage::cleanupClass()
{
	if (sTempDataBuffer)
	{
		dumpStats();
		free_texture_mem(sTempDataBuffer);
		sTempDataBuffer = NULL;
	}

	delete sMutex;
	sMutex = NULL;
}

//static
void LLImage::dumpStats()
{
	llinfos << "Static temp buffer usages count: "
			<< sTempDataBufferUsageCount
			<< " - Dynamic temp buffer allocations count: "
			<< sDynamicBufferAllocationsCount
			<< " - Maximum requested size for main thread temporary buffer: "
			<< sMaxMainThreadTempBufferSizeRequest
			<< " bytes - Size of static temp buffer: "
			<< TEMP_DATA_BUFFER_SIZE << " bytes." << llendl;
}

//static
const std::string& LLImage::getLastError()
{
	static const std::string noerr("No Error");
	return sLastErrorMessage.empty() ? noerr : sLastErrorMessage;
}

//static
void LLImage::setLastError(const std::string& message)
{
	if (sMutex)
	{
		sMutex->lock();
	}
	sLastErrorMessage = message;
	if (sMutex)
	{
		sMutex->unlock();
	}
}

//---------------------------------------------------------------------------
// LLImageBase
//---------------------------------------------------------------------------

LLImageBase::LLImageBase()
:	mData(NULL),
	mDataSize(0),
	mWidth(0),
	mHeight(0),
	mComponents(0)
{
	mBadBufferAllocation = false;
}

//virtual
LLImageBase::~LLImageBase()
{
	deleteData(); //virtual
}

//virtual
void LLImageBase::dump()
{
	llinfos << "LLImageBase mComponents " << mComponents << " mData " << mData
			<< " mDataSize " << mDataSize << " mWidth " << mWidth
			<< " mHeight " << mHeight << llendl;
}

//virtual
void LLImageBase::sanityCheck()
{
	if (mWidth > MAX_IMAGE_SIZE || mHeight > MAX_IMAGE_SIZE ||
		mDataSize > (S32)MAX_IMAGE_DATA_SIZE ||
		mComponents > (S8)MAX_IMAGE_COMPONENTS)
	{
		llerrs << "Failed sanity check - width: " << mWidth << " - height: "
			   << mHeight << " - datasize: " << mDataSize << " - components: "
			   << mComponents << " - data: " << mData << llendl;
	}
}

bool LLImageBase::sSizeOverride = false;

//virtual
void LLImageBase::deleteData()
{
	if (mData)
	{
		free_texture_mem(mData);
		mData = NULL;
	}
	mDataSize = 0;
}

//virtual
U8* LLImageBase::allocateData(S32 size)
{
	mBadBufferAllocation = false;
	if (size < 0)
	{
		size = mWidth * mHeight * mComponents;
		if (size <= 0)
		{
			llwarns << "Bad dimentions: " << mWidth << "x" <<  mHeight << "x"
					<< mComponents << llendl;
			mBadBufferAllocation = true;
			return NULL;
		}
	}
	else if (size <= 0 || (size > 4096 * 4096 * 16 && sSizeOverride == false))
	{
		llwarns << "Bad size: " << size << llendl;
		mBadBufferAllocation = true;
		return NULL;
	}

	if (!mData || size != mDataSize)
	{
		if (mData)
		{
			deleteData(); //virtual
		}

		mData = (U8*)allocate_texture_mem((size_t)size * sizeof(U8));
		if (!mData)
		{
			llwarns << "Could not allocate image data for requested size: "
					<< size << llendl;
			size = 0;
			mWidth = mHeight = 0;
			mBadBufferAllocation = true;
			return NULL;
		}
		mDataSize = size;
	}

	return mData;
}

//virtual
U8* LLImageBase::reallocateData(S32 size)
{
	if (mData && mDataSize == size)
	{
		return mData;
	}
	U8* new_datap = (U8*)allocate_texture_mem((size_t)size * sizeof(U8));
	if (!new_datap)
	{
		llwarns << "Could not reallocate image data for requested size: "
				<< size << llendl;
		mBadBufferAllocation = true;
		return NULL;
	}
	if (mData)
	{
		S32 bytes = llmin(mDataSize, size);
		memcpy(new_datap, mData, bytes);
		free_texture_mem(mData);
	}
	mData = new_datap;
	mDataSize = size;
	mBadBufferAllocation = false;
	return mData;
}

const U8* LLImageBase::getData() const
{
	if (mBadBufferAllocation)
	{
		llwarns << "Bad memory allocation for the image buffer !" << llendl;
		llassert(false);
		return NULL;
	}

	return mData;
}

U8* LLImageBase::getData()
{
	if (mBadBufferAllocation)
	{
		llwarns << "Bad memory allocation for the image buffer !" << llendl;
		llassert(false);
		return NULL;
	}

	return mData;
}

bool LLImageBase::isBufferInvalid()
{
	return mBadBufferAllocation || mData == NULL;
}

void LLImageBase::setSize(S32 width, S32 height, S32 ncomponents)
{
	mWidth = width;
	mHeight = height;
	mComponents = ncomponents;
}

U8* LLImageBase::allocateDataSize(S32 width, S32 height, S32 ncomp, S32 size)
{
	setSize(width, height, ncomp);
	return allocateData(size); //virtual
}

//---------------------------------------------------------------------------
// LLImageRaw
//---------------------------------------------------------------------------

LLAtomicS32 LLImageRaw::sRawImageCount(0);

LLImageRaw::LLImageRaw()
:	LLImageBase()
{
	++sRawImageCount;
}

LLImageRaw::LLImageRaw(U16 width, U16 height, S8 components)
:	LLImageBase()
{
	llassert(S32(width) * S32(height) * S32(components) <= MAX_IMAGE_DATA_SIZE);
	allocateDataSize(width, height, components);
	++sRawImageCount;
}

LLImageRaw::LLImageRaw(U8* data, U16 width, U16 height, S8 components,
					   bool no_copy)
:	LLImageBase()
{
	if (no_copy)
	{
		setDataAndSize(data, width, height, components);
	}
	else if (allocateDataSize(width, height, components) && data && getData())
	{
		memcpy(getData(), data, width * height * components);
	}
	++sRawImageCount;
}

LLImageRaw::LLImageRaw(const U8* data, U16 width, U16 height, S8 components)
:	LLImageBase()
{
	if (allocateDataSize(width, height, components) && data && getData())
	{
		memcpy(getData(), data, width * height * components);
	}
	++sRawImageCount;
}

LLImageRaw::LLImageRaw(const std::string& filename, bool j2c_lowest_mip_only)
:	LLImageBase()
{
	createFromFile(filename, j2c_lowest_mip_only);
	++sRawImageCount;
}

LLImageRaw::~LLImageRaw()
{
	--sRawImageCount;
}

void LLImageRaw::releaseData()
{
	LLImageBase::setSize(0, 0, 0);
	LLImageBase::setDataAndSize(NULL, 0);
}

void LLImageRaw::setDataAndSize(U8* data, S32 width, S32 height, S8 components)
{
	if (data == getData())
	{
		return;
	}

	deleteData();

	LLImageBase::setSize(width, height, components);
	LLImageBase::setDataAndSize(data, width * height * components);
}

bool LLImageRaw::resize(U16 width, U16 height, S8 components)
{
	if (getWidth() == width && getHeight() == height &&
		getComponents() == components && !isBufferInvalid())
	{
		return true;
	}

	// Reallocate the data buffer.
	deleteData();
	allocateDataSize(width, height, components);

	return !isBufferInvalid();
}

U8* LLImageRaw::getSubImage(U32 x_pos, U32 y_pos, U32 width, U32 height) const
{
	U8* data = new (std::nothrow) U8[width * height * getComponents()];
	// Should do some simple bounds checking
	if (!data || !getData())
	{
		llwarns << "Out of memory. Sub image not retrieved !" << llendl;
		return NULL;
	}

	U32 i;
	for (i = y_pos; i < y_pos + height; ++i)
	{
		memcpy(data + i * width * getComponents(),
			   getData() +
			   ((y_pos + i) * getWidth() + x_pos) * getComponents(),
			   getComponents() * width);
	}
	return data;
}

bool LLImageRaw::setSubImage(U32 x_pos, U32 y_pos, U32 width, U32 height,
							 const U8* data, U32 stride, bool reverse_y)
{
	if (!data || !getData())
	{
		llwarns << "Out of memory. Sub image not set !" << llendl;
		return false;
	}

	// Should do some simple bounds checking

	for (U32 i = 0; i < height; ++i)
	{
		const U32 row = reverse_y ? height - 1 - i : i;
		const U32 from_offset = row * (stride == 0 ? width * getComponents()
												   : stride);
		const U32 to_offset = (y_pos + i) * getWidth() + x_pos;
		memcpy(getData() + to_offset * getComponents(),
			   data + from_offset, getComponents() * width);
	}

	return true;
}

void LLImageRaw::clear(U8 r, U8 g, U8 b, U8 a)
{
	// This is fairly bogus, but it will do for now.
	if (isBufferInvalid()) return;

	S32 count = getWidth() * getHeight();
	S8 components = getComponents();
	llassert(components <= 4 && count * components == getDataSize());

	switch (components)
	{
		case 1:
		{
			U8* dst = getData();
			std::fill_n(dst, count, r);
			break;
		}

		case 2:
		{
			U16* dst = (U16*)getData();
#if LL_BIG_ENDIAN
			U16 val = U16(g) | (U16)b << 8;
#else
			U16 val = U16(r) | (U16)g << 8;
#endif
			std::fill_n(dst, count, val);
			break;
		}

		case 3:
		{
			U8* dst = getData();
			for (S32 i = 0; i < count; ++i)
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
			}
			break;
		}

		case 4:
		{
			U32* dst = (U32*)getData();
#if LL_BIG_ENDIAN
			//U32 val = U32(a) | U32(b) << 8 | U32(g) << 16 | U32(r) << 24;
			U32 val = U32(r) << 8 | g;
			val = val << 8 | b;
			val = val << 8 | a;
#else
			//U32 val = U32(r) | U32(g) << 8 | U32(b) << 16 | U32(a) << 24;
			U32 val = U32(a) << 8 | b;
			val = val << 8 | g;
			val = val << 8 | r;
#endif
			std::fill_n(dst, count, val);
			break;
		}

		default:
			llwarns_once << "Invalid number of components: " << components
						 << llendl;
	}
}

// Reverses the order of the rows in the image
void LLImageRaw::verticalFlip()
{
	S32 row_bytes = getWidth() * getComponents();
	U8* line_buffer = getTempBuffer(row_bytes);
	if (!line_buffer || !getData())
	{
		llwarns << "Out of memory. Flipping aborted !" << llendl;
		return;
	}
	S32 mid_row = getHeight() / 2;
	for (S32 row = 0; row < mid_row; ++row)
	{
		U8* row_a_data = getData() + row * row_bytes;
		U8* row_b_data = getData() + (getHeight() - 1 - row) * row_bytes;
		memcpy(line_buffer, row_a_data, row_bytes);
		memcpy(row_a_data, row_b_data, row_bytes);
		memcpy(row_b_data, line_buffer, row_bytes);
	}
	freeTempBuffer(line_buffer);
}

bool LLImageRaw::optimizeAwayAlpha()
{
	if (getComponents() != 4)
	{
		return false;
	}

	U8* data = getData();
	U32 width = getWidth();
	U32 height = getHeight();
	U32 pixels = width * height;

	// Check alpha channel for all 255
	for (U32 i = 0; i < pixels; ++i)
	{
		if (data[i * 4 + 3] != 255)
		{
			return false;
		}
	}

	// Alpha channel is 255 for each pixel, make a new copy of data without
	// alpha channel. *TODO: vectorize.
	U8* new_data = (U8*)allocate_texture_mem(width * height * 3);
	for (U32 i = 0; i < pixels; ++i)
	{
		U32 di = i * 3;
		U32 si = i * 4;
		new_data[di++] = data[si++];
		new_data[di++] = data[si++];
		new_data[di] = data[si];
	}

	setDataAndSize(new_data, width, height, 3);
	return true;
}

void LLImageRaw::expandToPowerOfTwo(S32 max_dim, bool scale_image)
{
	// Find new sizes
	S32 new_width = MIN_IMAGE_SIZE;
	S32 new_height = MIN_IMAGE_SIZE;

	while (new_width < getWidth() && new_width < max_dim)
	{
		new_width <<= 1;
	}

	while (new_height < getHeight() && new_height < max_dim)
	{
		new_height <<= 1;
	}

	scale(new_width, new_height, scale_image);
}

void LLImageRaw::contractToPowerOfTwo(S32 max_dim, bool scale_image)
{
	// Find new sizes
	S32 new_width = max_dim;
	S32 new_height = max_dim;

	while (new_width > getWidth() && new_width > MIN_IMAGE_SIZE)
	{
		new_width >>= 1;
	}

	while (new_height > getHeight() && new_height > MIN_IMAGE_SIZE)
	{
		new_height >>= 1;
	}

	scale(new_width, new_height, scale_image);
}

void LLImageRaw::biasedScaleToPowerOfTwo(S32 max_dim)
{
	// Strong bias towards rounding down (to save bandwidth)
	// No bias would mean THRESHOLD == 1.5f;
	constexpr F32 THRESHOLD = 1.75f;

	// Find new sizes
	S32 larger_w = max_dim;	// 2^n >= mWidth
	S32 smaller_w = max_dim;	// 2^(n-1) <= mWidth
	while (smaller_w > getWidth() && smaller_w > MIN_IMAGE_SIZE)
	{
		larger_w = smaller_w;
		smaller_w >>= 1;
	}
	S32 new_width = (F32)getWidth() / smaller_w > THRESHOLD ? larger_w
															: smaller_w;

	S32 larger_h = max_dim;	// 2^m >= mHeight
	S32 smaller_h = max_dim;	// 2^(m-1) <= mHeight
	while (smaller_h > getHeight() && smaller_h > MIN_IMAGE_SIZE)
	{
		larger_h = smaller_h;
		smaller_h >>= 1;
	}
	S32 new_height = (F32)getHeight() / smaller_h > THRESHOLD ? larger_h
															  : smaller_h;

	scale(new_width, new_height);
}

// Calculates (U8)(255*(a/255.f)*(b/255.f) + 0.5f).  Thanks, Jim Blinn!
LL_INLINE U8 LLImageRaw::fastFractionalMult(U8 a, U8 b)
{
	U32 i = a * b + 128;
	return U8((i + (i>>8)) >> 8);
}

void LLImageRaw::composite(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	if (!src || !dst || src->isBufferInvalid() || dst->isBufferInvalid())
	{
		return;
	}

	if (dst->getComponents() == 3)
	{
		if (src->getWidth() == dst->getWidth() &&
			src->getHeight() == dst->getHeight())
		{
			// No scaling needed
			if (src->getComponents() == 3)
			{
				copyUnscaled(src);  // Alpha is one so just copy the data.
			}
			else
			{
				compositeUnscaled4onto3(src);
			}
		}
		else if (src->getComponents() == 3)
		{
			copyScaled(src);  // Alpha is one so just copy the data.
		}
		else
		{
			compositeScaled4onto3(src);
		}
	}
}

// Src and dst can be any size.  Src has 4 components. Dst has 3 components.
void LLImageRaw::compositeScaled4onto3(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	llassert(src->getComponents() == 4 && dst->getComponents() == 3);

	// Vertical: scale but no composite
	S32 temp_data_size = src->getWidth() * dst->getHeight() *
						 src->getComponents();
	U8* temp_buffer = getTempBuffer(temp_data_size);
	if (!temp_buffer || !src->getData() || !dst->getData())
	{
		llwarns << "Out of memory. Scaling aborted !" << llendl;
		return;
	}
	for (S32 col = 0; col < src->getWidth(); ++col)
	{
		copyLineScaled(src->getData() + src->getComponents() * col,
					   temp_buffer + src->getComponents() * col,
					   src->getHeight(), dst->getHeight(),
					   src->getWidth(), src->getWidth());
	}

	// Horizontal: scale and composite
	for (S32 row = 0; row < dst->getHeight(); ++row)
	{
		compositeRowScaled4onto3(temp_buffer +
								 src->getComponents() * src->getWidth() * row,
								 dst->getData() +
								 dst->getComponents() * dst->getWidth() * row,
								 src->getWidth(), dst->getWidth());
	}

	// Clean up
	freeTempBuffer(temp_buffer);
}

// Src and dst are same size. Src has 4 components. Dst has 3 components.
void LLImageRaw::compositeUnscaled4onto3(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	llassert(3 == src->getComponents() || 4 == src->getComponents());
	llassert(src->getWidth() == dst->getWidth() &&
			 src->getHeight() == dst->getHeight());

	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, conversion aborted !" << llendl;
		return;
	}

	S32 pixels = getWidth() * getHeight();
	while (pixels--)
	{
		U8 alpha = src_data[3];
		if (alpha)
		{
			if (alpha == 255)
			{
				dst_data[0] = src_data[0];
				dst_data[1] = src_data[1];
				dst_data[2] = src_data[2];
			}
			else
			{
				U8 transparency = 255 - alpha;
				dst_data[0] = fastFractionalMult(dst_data[0], transparency) +
							  fastFractionalMult(src_data[0], alpha);
				dst_data[1] = fastFractionalMult(dst_data[1], transparency) +
							  fastFractionalMult(src_data[1], alpha);
				dst_data[2] = fastFractionalMult(dst_data[2], transparency) +
							  fastFractionalMult(src_data[2], alpha);
			}
		}
		src_data += 4;
		dst_data += 3;
	}
}

void LLImageRaw::copyUnscaledAlphaMask(LLImageRaw* src, const LLColor4U& fill)
{
	LLImageRaw* dst = this;  // Just for clarity.

	if (!src || !dst || src->isBufferInvalid() || dst->isBufferInvalid())
	{
		return;
	}

	llassert(src->getComponents() == 1 && dst->getComponents() == 4 &&
			 src->getWidth() == dst->getWidth() &&
			 src->getHeight() == dst->getHeight());

	S32 pixels = getWidth() * getHeight();
	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, copy aborted !" << llendl;
		return;
	}

	for (S32 i = 0; i < pixels; ++i)
	{
		dst_data[0] = fill.mV[0];
		dst_data[1] = fill.mV[1];
		dst_data[2] = fill.mV[2];
		dst_data[3] = src_data[0];
		src_data += 1;
		dst_data += 4;
	}
}

// Fill the buffer with a constant color
void LLImageRaw::fill(const LLColor4U& color)
{
	if (isBufferInvalid()) return;

	if (!getData())
	{
		llwarns << "Out of memory, filling aborted !" << llendl;
		return;
	}

	S32 pixels = getWidth() * getHeight();
	if (getComponents() == 4)
	{
		U32* data = (U32*)getData();
		U32 rgba = color.asRGBA();
		for (S32 i = 0; i < pixels; ++i)
		{
			data[i] = rgba;
		}
	}
	else if (getComponents() == 3)
	{
		U8* data = getData();
		for (S32 i = 0; i < pixels; ++i)
		{
			data[0] = color.mV[0];
			data[1] = color.mV[1];
			data[2] = color.mV[2];
			data += 3;
		}
	}
}

LLPointer<LLImageRaw> LLImageRaw::duplicate()
{
	if (getNumRefs() < 2)
	{
		// nobody else refences to this image, no need to duplicate.
		return this;
	}

	if (!getData())
	{
		llwarns << "Out of memory, image not duplicated !" << llendl;
		return this;
	}

	// make a duplicate
	LLPointer<LLImageRaw> dup = new LLImageRaw(getData(), getWidth(),
											   getHeight(), getComponents());
	if (dup->isBufferInvalid())
	{
		// There was an allocation failure: release the LLImageRaw and return
		// a NULL LLPointer<LLImageRaw>:
		dup = NULL;
	}

	return dup;
}

// Src and dst can be any size.  Src and dst can each have 3 or 4 components.
void LLImageRaw::copy(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	if (!src || !dst || src->isBufferInvalid() || dst->isBufferInvalid())
	{
		return;
	}

	llassert((3 == src->getComponents() || 4 == src->getComponents()) &&
			 (3 == dst->getComponents() || 4 == dst->getComponents()));

	if (src->getWidth() == dst->getWidth() &&
		src->getHeight() == dst->getHeight())
	{
		// No scaling needed
		if (src->getComponents() == dst->getComponents())
		{
			copyUnscaled(src);
		}
		else if (3 == src->getComponents())
		{
			copyUnscaled3onto4(src);
		}
		else
		{
			// 4 == src->getComponents()
			copyUnscaled4onto3(src);
		}
	}
	else
	{
		// Scaling needed
		// No scaling needed
		if (src->getComponents() == dst->getComponents())
		{
			copyScaled(src);
		}
		else if (3 == src->getComponents())
		{
			copyScaled3onto4(src);
		}
		else
		{
			// 4 == src->getComponents()
			copyScaled4onto3(src);
		}
	}
}

// Src and dst are same size.  Src and dst have same number of components.
void LLImageRaw::copyUnscaled(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, copy aborted !" << llendl;
		return;
	}

	llassert(1 == src->getComponents() || 3 == src->getComponents() ||
			 4 == src->getComponents());
	llassert(src->getComponents() == dst->getComponents());
	llassert(src->getWidth() == dst->getWidth() &&
			 src->getHeight() == dst->getHeight());

	memcpy(dst_data, src_data,
		   getWidth() * getHeight() * getComponents());
}

// Src and dst can be any size.  Src has 3 components.  Dst has 4 components.
void LLImageRaw::copyScaled3onto4(LLImageRaw* src)
{
	llassert(3 == src->getComponents() && 4 == getComponents());

	// Slow, but simple.  Optimize later if needed.
	LLImageRaw temp(src->getWidth(), src->getHeight(), 4);
	temp.copyUnscaled3onto4(src);
	copyScaled(&temp);
}

// Src and dst can be any size.  Src has 4 components.  Dst has 3 components.
void LLImageRaw::copyScaled4onto3(LLImageRaw* src)
{
	llassert(4 == src->getComponents() && 3 == getComponents());

	// Slow, but simple.  Optimize later if needed.
	LLImageRaw temp(src->getWidth(), src->getHeight(), 3);
	temp.copyUnscaled4onto3(src);
	copyScaled(&temp);
}

// Src and dst are same size.  Src has 4 components.  Dst has 3 components.
void LLImageRaw::copyUnscaled4onto3(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	llassert(3 == dst->getComponents() && 4 == src->getComponents() &&
			 src->getWidth() == dst->getWidth() &&
			 src->getHeight() == dst->getHeight());

	S32 pixels = getWidth() * getHeight();
	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, copy aborted !" << llendl;
		return;
	}

	for (S32 i = 0; i < pixels; ++i)
	{
		dst_data[0] = src_data[0];
		dst_data[1] = src_data[1];
		dst_data[2] = src_data[2];
		src_data += 4;
		dst_data += 3;
	}
}

// Src and dst are same size.  Src has 3 components.  Dst has 4 components.
void LLImageRaw::copyUnscaled3onto4(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.
	llassert(3 == src->getComponents() && 4 == dst->getComponents() &&
			 src->getWidth() == dst->getWidth() &&
			 src->getHeight() == dst->getHeight());

	S32 pixels = getWidth() * getHeight();
	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, copy aborted !" << llendl;
		return;
	}

	for (S32 i = 0; i < pixels; ++i)
	{
		dst_data[0] = src_data[0];
		dst_data[1] = src_data[1];
		dst_data[2] = src_data[2];
		dst_data[3] = 255;
		src_data += 3;
		dst_data += 4;
	}
}

U8* LLImageRaw::getTempBuffer(S32 size)
{
	bool from_main_thread = is_main_thread();
	if (from_main_thread &&
		size > LLImage::sMaxMainThreadTempBufferSizeRequest)
	{
		LLImage::sMaxMainThreadTempBufferSizeRequest = size;
	}
	if (from_main_thread && LLImage::sTempDataBuffer &&
		(size_t)size <= TEMP_DATA_BUFFER_SIZE)
	{
		// In order to avoid many memory reallocations resulting in virtual
		// address space fragmentation, we use, for the main thread, a static
		// buffer as a temporary storage whenever possible.
		++LLImage::sTempDataBufferUsageCount;
		return LLImage::sTempDataBuffer;
	}
	else
	{
		++LLImage::sDynamicBufferAllocationsCount;
		U8* tmp = (U8*)allocate_texture_mem((size_t)size * sizeof(U8));
		return tmp;
	}
}

void LLImageRaw::freeTempBuffer(U8* addr)
{
	if (addr != LLImage::sTempDataBuffer)
	{
		free_texture_mem((void*)addr);
	}
}

// Src and dst can be any size.  Src and dst have same number of components.
void LLImageRaw::copyScaled(LLImageRaw* src)
{
	LLImageRaw* dst = this;  // Just for clarity.

	if (!src || !dst || src->isBufferInvalid() || dst->isBufferInvalid())
	{
		return;
	}

	llassert_always(1 == src->getComponents() || 3 == src->getComponents() ||
					4 == src->getComponents());
	llassert_always(src->getComponents() == dst->getComponents());

	U8* src_data = src->getData();
	U8* dst_data = dst->getData();
	if (!src_data || !dst_data)
	{
		llwarns << "Out of memory, copy aborted !" << llendl;
		return;
	}

	if (src->getWidth() == dst->getWidth() &&
		src->getHeight() == dst->getHeight())
	{
		memcpy(dst_data, src_data, getWidth() * getHeight() * getComponents());
		return;
	}

	S32 src_width = src->getWidth();
	S32 src_components = src->getComponents();
	S32 dst_width = dst->getWidth();
	S32 dst_components = dst->getComponents();
	bilinear_scale(src->getData(), src_width, src->getHeight(), src_components,
				   src_width * src_components, dst->getData(), dst_width,
				   dst->getHeight(), dst_components, dst_width * dst_components);
}

bool LLImageRaw::scale(S32 new_width, S32 new_height, bool scale_image_data)
{
	if (isBufferInvalid()) return false;

	S32 components = getComponents();
	if (components != 1 && components != 3 && components != 4)
	{
		llwarns << "Invalid number of components: " << components
				<< ". Aborted." << llendl;
		return false;
	}

	S32 old_width = getWidth();
	S32 old_height = getHeight();
	if (old_width == new_width && old_height == new_height)
	{
		return true;  // Nothing to do.
	}

	// Reallocate the data buffer.

	if (!getData())
	{
		llwarns << "Out of memory. Scaling aborted !" << llendl;
		return false;
	}

	if (scale_image_data)
	{
		S32 new_data_size = new_width * new_height * components;
		if (new_data_size <= 0)
		{
			llwarns << "Non-positive data size: width = " << new_width
					<< " - height = " << new_height << " - components = "
					<< components << "; aborting !" << llendl;
			llassert(false);
			return false;
		}

		U8* new_data = (U8*)allocate_texture_mem(new_data_size);
		if (!new_data)
		{
			llwarns << "Out of memory while rescaling for requested size: "
					<< new_data_size << llendl;
			return false;
		}

		components = getComponents();
		bilinear_scale(getData(), old_width, old_height, components,
					   old_width * components, new_data, new_width, new_height,
					   components, new_width * components);
		setDataAndSize(new_data, new_width, new_height, components);
	}
	else
	{
		// Copy out existing image data
		S32	temp_data_size = old_width * old_height	* getComponents();
		U8* temp_buffer = getTempBuffer(temp_data_size);
		if (!temp_buffer)
		{
			llwarns << "Out of memory while rescaling: old (w, h, c) = ("
					<< old_width << ", " << old_height << ", "
					<< components << "); new (w, h, c) = ("
					<< new_width << ", " << new_height << ", "
					<< getComponents() << ")" << llendl;
			return false;
		}
		memcpy(temp_buffer,	getData(), temp_data_size);

		// Allocate new image data, will delete old data
		components = getComponents();
		U8*	new_buffer = allocateDataSize(new_width, new_height, components);
		if (!new_buffer)
		{
			llwarns << "Out of memory while rescaling: old (w, h, c) = ("
					<< old_width << ", " << old_height << ", "
					<< components << "); new (w, h, c) = ("
					<< new_width << ", " << new_height << ", "
					<< getComponents() << ")" << llendl;
			freeTempBuffer(temp_buffer);
			return false;
		}

		components = getComponents();
		for (S32 row = 0; row <	new_height;	++row)
		{
			if (row	< old_height)
			{
				memcpy(new_buffer +	new_width * row * components,
					   temp_buffer + old_width * row * components,
					   components * llmin(old_width, new_width));
				if (old_width <	new_width)
				{
					// Pad out rest of row with black
					memset(new_buffer +	components *
						   (new_width * row + old_width),
						   0, components * (new_width - old_width));
				}
			}
			else
			{
				// Pad remaining rows with black
				memset(new_buffer +	new_width * row * components, 0,
					   new_width * components);
			}
		}

		// Clean up
		freeTempBuffer(temp_buffer);
	}

	return true;
}

LLPointer<LLImageRaw> LLImageRaw::scaled(S32 new_width, S32 new_height)
{
	LLPointer<LLImageRaw> result;

	if (isBufferInvalid())
	{
		llwarns << "Invalid image buffer. Aborted." << llendl;
		return result;
	}

	S32 components = getComponents();
	if (components != 1 && components != 3 && components != 4)
	{
		llwarns << "Invalid number of components: " << components
				<< ". Aborted." << llendl;
		return result;
	}

	S32 old_width = getWidth();
	S32 old_height = getHeight();
	if (old_width == new_width && old_height == new_height)
	{
		// Note: cannot use (std::nothrow) with our custom new() allocator
		try
		{
			result = new LLImageRaw(old_width, old_height, components);
			if (result.notNull() && !result->isBufferInvalid())
			{
				memcpy(result->getData(), getData(), getDataSize());
			}
		}
		catch (std::bad_alloc&)
		{
		}
	}
	else
	{
		S32 new_data_size = new_width * new_height * components;
		if (new_data_size > 0)
		{
			// Note: cannot use (std::nothrow) with our custom new() allocator
			try
			{
				result = new LLImageRaw(new_width, new_height, components);
				if (result.notNull() && !result->isBufferInvalid())
				{
					bilinear_scale(getData(), old_width, old_height,
								   components, old_width * components,
								   result->getData(), new_width, new_height,
								   components, new_width * components);
				}
			}
			catch (std::bad_alloc&)
			{
			}
		}
	}

	if (result.isNull())
	{
		llwarns << "Failed to allocate new image for size: "
				<< new_width << "x" << new_height << ". Out of memory ?"
				<< llendl;
	}

	return result;
}

void LLImageRaw::copyLineScaled(U8* in, U8* out, S32 in_pixel_len,
								S32 out_pixel_len, S32 in_pixel_step,
								S32 out_pixel_step)
{
	const S32 components = getComponents();
	llassert(components >= 1 && components <= 4);

	const F32 ratio = F32(in_pixel_len) / out_pixel_len; // ratio of old to new
	const F32 norm_factor = 1.f / ratio;

	S32 goff = components >= 2 ? 1 : 0;
	S32 boff = components >= 3 ? 2 : 0;
	for (S32 x = 0; x < out_pixel_len; ++x)
	{
		// Sample input pixels in range from sample0 to sample1. Avoid floating
		// point accumulation error... Do not just add ratio each time. JC
		const F32 sample0 = x * ratio;
		const F32 sample1 = (x + 1) * ratio;
		// Left integer (floor)
		const S32 index0 = llfloor(sample0);
		// Right integer (floor)
		const S32 index1 = llfloor(sample1);
		// Spill over on left
		const F32 fract0 = 1.f - sample0 + (F32)index0;
		// Spill over on right
		const F32 fract1 = sample1 - (F32)index1;

		if (index0 == index1)
		{
			// Interval is embedded in one input pixel
			S32 t0 = x * out_pixel_step * components;
			S32 t1 = index0 * in_pixel_step * components;
			U8* outp = out + t0;
			U8* inp = in + t1;
			for (S32 i = 0; i < components; ++i)
			{
				*outp = *inp;
				++outp;
				++inp;
			}
		}
		else
		{
			// Left straddle
			S32 t1 = index0 * in_pixel_step * components;
			F32 r = in[t1 + 0] * fract0;
			F32 g = in[t1 + goff] * fract0;
			F32 b = in[t1 + boff] * fract0;
			F32 a = 0;
			if (components == 4)
			{
				a = in[t1 + 3] * fract0;
			}

			// Central interval
			if (components < 4)
			{
				for (S32 u = index0 + 1; u < index1; ++u)
				{
					S32 t2 = u * in_pixel_step * components;
					r += in[t2 + 0];
					g += in[t2 + goff];
					b += in[t2 + boff];
				}
			}
			else
			{
				for (S32 u = index0 + 1; u < index1; ++u)
				{
					S32 t2 = u * in_pixel_step * components;
					r += in[t2 + 0];
					g += in[t2 + 1];
					b += in[t2 + 2];
					a += in[t2 + 3];
				}
			}

			// right straddle
			// Watch out for reading off of end of input array.
			if (fract1 && index1 < in_pixel_len)
			{
				S32 t3 = index1 * in_pixel_step * components;
				if (components < 4)
				{
					U8 in0 = in[t3];
					U8 in1 = in[t3 + goff];
					U8 in2 = in[t3 + boff];
					r += in0 * fract1;
					g += in1 * fract1;
					b += in2 * fract1;
				}
				else
				{
					U8 in0 = in[t3++];
					U8 in1 = in[t3++];
					U8 in2 = in[t3++];
					U8 in3 = in[t3];
					r += in0 * fract1;
					g += in1 * fract1;
					b += in2 * fract1;
					a += in3 * fract1;
				}
			}

			U8 arr[] = {
				U8(ll_roundp(r * norm_factor)),
				U8(ll_roundp(g * norm_factor)),
				U8(ll_roundp(b * norm_factor)),
				U8(ll_roundp(a * norm_factor))
			};  // Skip conditional
			S32 t4 = x * out_pixel_step * components;
			memcpy(out + t4, arr, sizeof(U8) * components);
		}
	}
}

void LLImageRaw::compositeRowScaled4onto3(U8* in, U8* out, S32 in_pixel_len,
										  S32 out_pixel_len)
{
	llassert(getComponents() == 3);

	constexpr S32 IN_COMPONENTS = 4;
	constexpr S32 OUT_COMPONENTS = 3;

	const F32 ratio = F32(in_pixel_len) / out_pixel_len; // ratio of old to new
	const F32 norm_factor = 1.f / ratio;

	for (S32 x = 0; x < out_pixel_len; ++x)
	{
		// Sample input pixels in range from sample0 to sample1.
		// Avoid floating point accumulation error; do not just add ratio each
		// time. JC
		const F32 sample0 = x * ratio;
		const F32 sample1 = (x + 1) * ratio;
		const S32 index0 = S32(sample0); // Left integer (floor)
		const S32 index1 = S32(sample1); // Right integer (floor)
		const F32 fract0 = 1.f - (sample0 - F32(index0)); // Spill over on left
		const F32 fract1 = sample1 - F32(index1); // Spill-over on right

		U8 in_scaled_r;
		U8 in_scaled_g;
		U8 in_scaled_b;
		U8 in_scaled_a;

		if (index0 == index1)
		{
			// Interval is embedded in one input pixel
			S32 t1 = index0 * IN_COMPONENTS;
			in_scaled_r = in[t1];
			in_scaled_g = in[t1];
			in_scaled_b = in[t1];
			in_scaled_a = in[t1];
		}
		else
		{
			// Left straddle
			S32 t1 = index0 * IN_COMPONENTS;
			F32 r = in[t1] * fract0;
			F32 g = in[t1 + 1] * fract0;
			F32 b = in[t1 + 2] * fract0;
			F32 a = in[t1 + 3] * fract0;

			// Central interval
			for (S32 u = index0 + 1; u < index1; ++u)
			{
				S32 t2 = u * IN_COMPONENTS;
				r += in[t2];
				g += in[t2 + 1];
				b += in[t2 + 2];
				a += in[t2 + 3];
			}

			// right straddle
			// Watch out for reading off of end of input array.
			if (fract1 && index1 < in_pixel_len)
			{
				S32 t3 = index1 * IN_COMPONENTS;
				r += in[t3] * fract1;
				g += in[t3 + 1] * fract1;
				b += in[t3 + 2] * fract1;
				a += in[t3 + 3] * fract1;
			}

			r *= norm_factor;
			g *= norm_factor;
			b *= norm_factor;
			a *= norm_factor;

			in_scaled_r = U8(ll_roundp(r));
			in_scaled_g = U8(ll_roundp(g));
			in_scaled_b = U8(ll_roundp(b));
			in_scaled_a = U8(ll_roundp(a));
		}

		if (in_scaled_a)
		{
			if (255 == in_scaled_a)
			{
				out[0] = in_scaled_r;
				out[1] = in_scaled_g;
				out[2] = in_scaled_b;
			}
			else
			{
				U8 transparency = 255 - in_scaled_a;
				out[0] = fastFractionalMult(out[0], transparency) +
						 fastFractionalMult(in_scaled_r, in_scaled_a);
				out[1] = fastFractionalMult(out[1], transparency) +
						 fastFractionalMult(in_scaled_g, in_scaled_a);
				out[2] = fastFractionalMult(out[2], transparency) +
						 fastFractionalMult(in_scaled_b, in_scaled_a);
			}
		}
		out += OUT_COMPONENTS;
	}
}

static struct
{
	const char* exten;
	EImageCodec codec;
}

file_extensions[] =
{
	{ "bmp", IMG_CODEC_BMP },
	{ "tga", IMG_CODEC_TGA },
	{ "j2c", IMG_CODEC_J2C },
	{ "jp2", IMG_CODEC_J2C },
	{ "texture", IMG_CODEC_J2C },
	{ "jpg", IMG_CODEC_JPEG },
	{ "jpeg", IMG_CODEC_JPEG },
	{ "png", IMG_CODEC_PNG }
};
constexpr S32 num_file_extensions = LL_ARRAY_SIZE(file_extensions);

static std::string find_file(std::string& name, S8* codec)
{
	std::string tname;
	for (S32 i = 0; i < num_file_extensions; ++i)
	{
		tname = name + "." + std::string(file_extensions[i].exten);
		llifstream ifs(tname.c_str(), std::ifstream::binary);
		if (ifs.is_open())
		{
			ifs.close();
			if (codec)
			{
				*codec = file_extensions[i].codec;
			}
			return std::string(file_extensions[i].exten);
		}
	}
	return "";
}

EImageCodec LLImageBase::getCodecFromExtension(const std::string& exten)
{
	if (!exten.empty())
	{
		for (S32 i = 0; i < num_file_extensions; ++i)
		{
			if (file_extensions[i].exten == exten)
			{
				return file_extensions[i].codec;
			}
		}
	}
	return IMG_CODEC_INVALID;
}

bool LLImageRaw::createFromFile(const std::string& filename,
								bool j2c_lowest_mip_only)
{
	std::string name = filename;
	size_t dotidx = name.rfind('.');
	S8 codec = IMG_CODEC_INVALID;
	std::string exten;

	deleteData(); // Delete any existing data

	if (dotidx != std::string::npos)
	{
		exten = name.substr(dotidx + 1);
		LLStringUtil::toLower(exten);
		codec = getCodecFromExtension(exten);
	}
	else
	{
		exten = find_file(name, &codec);
		name = name + "." + exten;
	}
	if (codec == IMG_CODEC_INVALID)
	{
		return false; // Format not recognized
	}

	llifstream ifs(name.c_str(), std::ifstream::binary);
	if (!ifs.is_open())
	{
		// SJB: changed from llinfos to LL_DEBUGS("Image") to reduce spam
		LL_DEBUGS("Image") << "Unable to open image file: " << name << LL_ENDL;
		return false;
	}

	ifs.seekg (0, std::ios::end);
	int length = ifs.tellg();
	if (j2c_lowest_mip_only && length > 2048)
	{
		length = 2048;
	}
	ifs.seekg (0, std::ios::beg);

	if (!length)
	{
		llinfos << "Zero length file file: " << name << llendl;
		return false;
	}

	LLPointer<LLImageFormatted> image;
	switch (codec)
	{
		case IMG_CODEC_BMP:
			image = new LLImageBMP();
			break;

		case IMG_CODEC_TGA:
			image = new LLImageTGA();
			break;

		case IMG_CODEC_JPEG:
			image = new LLImageJPEG();
			break;

		case IMG_CODEC_J2C:
			image = new LLImageJ2C();
			break;

		default:
			return false;
	}
	llassert(image.notNull());

	U8* buffer = image->allocateData(length);
	if (!buffer) return false;
	ifs.read ((char*)buffer, length);
	ifs.close();

	bool success;

	success = image->updateData();
	if (success)
	{
		if (j2c_lowest_mip_only && codec == IMG_CODEC_J2C)
		{
			S32 width = image->getWidth();
			S32 height = image->getHeight();
			S32 discard_level = 0;
			while (width > 1 && height > 1 && discard_level < MAX_DISCARD_LEVEL)
			{
				width >>= 1;
				height >>= 1;
				++discard_level;
			}
			((LLImageJ2C *)((LLImageFormatted*)image))->setDiscardLevel(discard_level);
		}
		success = image->decode(this);
	}

	image = NULL; // deletes image
	if (!success)
	{
		deleteData();
		llwarns << "Unable to decode image" << name << llendl;
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
// LLImageFormatted
//---------------------------------------------------------------------------

LLImageFormatted::LLImageFormatted(S8 codec)
:	LLImageBase(),
	mCodec(codec),
	mDecoding(0),
	mDecoded(0),
	mDiscardLevel(-1)
{
}

//virtual
void LLImageFormatted::resetLastError()
{
	LLImage::setLastError("");
}

//virtual
void LLImageFormatted::setLastError(const std::string& message,
									const std::string& filename)
{
	std::string error = message;
	if (!filename.empty())
	{
		error += " FILE: " + filename;
	}
	LLImage::setLastError(error);
}

// static
LLImageFormatted* LLImageFormatted::createFromType(S8 codec)
{
	LLImageFormatted* image;
	switch (codec)
	{
		case IMG_CODEC_BMP:
			image = new LLImageBMP();
			break;

		case IMG_CODEC_TGA:
			image = new LLImageTGA();
			break;

		case IMG_CODEC_JPEG:
			image = new LLImageJPEG();
			break;

		case IMG_CODEC_PNG:
			image = new LLImagePNG();
			break;

		case IMG_CODEC_J2C:
			image = new LLImageJ2C();
			break;

		default:
			image = NULL;
	}
	return image;
}

// static
LLImageFormatted* LLImageFormatted::createFromExtension(const std::string& instring)
{
	std::string exten;
	size_t dotidx = instring.rfind('.');
	if (dotidx != std::string::npos)
	{
		exten = instring.substr(dotidx + 1);
	}
	else
	{
		exten = instring;
	}
	S8 codec = getCodecFromExtension(exten);
	return createFromType(codec);
}

//virtual
void LLImageFormatted::dump()
{
	LLImageBase::dump();

	llinfos << "LLImageFormatted" << " mDecoding " << mDecoding << " mCodec "
			<< S32(mCodec) << " mDecoded " << mDecoded << llendl;
}

S32 LLImageFormatted::calcDataSize(S32 discard_level)
{
	if (discard_level < 0)
	{
		discard_level = mDiscardLevel;
	}
	S32 w = getWidth() >> discard_level;
	S32 h = getHeight() >> discard_level;
	w = llmax(w, 1);
	h = llmax(h, 1);
	return w * h * getComponents();
}

S32 LLImageFormatted::calcDiscardLevelBytes(S32 bytes)
{
	llassert(bytes >= 0);
	S32 discard_level = 0;
	while (true)
	{
		S32 bytes_needed = calcDataSize(discard_level); //virtual
		if (bytes_needed <= bytes)
		{
			break;
		}
		if (++discard_level > MAX_IMAGE_MIP)
		{
			return -1;
		}
	}
	return discard_level;
}

// Subclasses that can handle more than 4 channels should override this function.
bool LLImageFormatted::decodeChannels(LLImageRaw* raw_image, S32 first_channel,
									  S32 max_channel)
{
	llassert(first_channel == 0 && max_channel == 4);
	return decode(raw_image);  // Loads first 4 channels by default.
}

//virtual
void LLImageFormatted::sanityCheck()
{
	LLImageBase::sanityCheck();

	if (mCodec >= IMG_CODEC_EOF)
	{
		llerrs << "Failed sanity check. Decoding: " << S32(mDecoding)
			   << " - decoded: " << S32(mDecoded) << " - codec: "
			   << S32(mCodec) << llendl;
	}
}

bool LLImageFormatted::copyData(U8* data, S32 size)
{
	if (data && (data != getData() || size != getDataSize()))
	{
		deleteData();
		if (allocateData(size) && getData())
		{
			memcpy(getData(), data, size);
		}
		else
		{
			return false;
		}
	}
	return true;
}

// LLImageFormatted becomes the owner of data
void LLImageFormatted::setData(U8* data, S32 size)
{
	if (data && data != getData())
	{
		deleteData();
		setDataAndSize(data, size); // Access private LLImageBase members
	}
}

void LLImageFormatted::appendData(U8* data, S32 size)
{
	if (data)
	{
		if (!getData())
		{
			setData(data, size);
		}
		else
		{
			S32 cursize = getDataSize();
			S32 newsize = cursize + size;
			if (reallocateData(newsize))
			{
				memcpy(getData() + cursize, data, size);
				free_texture_mem(data);
			}
		}
	}
}

bool LLImageFormatted::load(const std::string& filename)
{
	resetLastError();

	S64 file_size = 0;
	LLFile infile(filename, "rb", &file_size);
	if (!infile)
	{
		setLastError("Unable to open file for reading", filename);
		return false;
	}
	if (file_size == 0)
	{
		setLastError("File is empty", filename);
		return false;
	}

	U8* data = allocateData(file_size);
	if (!data)
	{
		setLastError("Out of memory", filename);
		return false;
	}

	if (infile.read(data, file_size) != file_size)
	{
		deleteData();
		setLastError("Unable to read entire file");
		return false;
	}

	return updateData();
}

bool LLImageFormatted::save(const std::string& filename)
{
	if (!getData())
	{
		llwarns << "NULL data pointer for raw image. Not saving: " << filename
				<< llendl;
		return false;
	}

	resetLastError();

	LLFILE* file = LLFile::open(filename, "wb");	
	if (!file)
	{
		setLastError("Unable to open file for writing", filename);
		return false;
	}

	fwrite((void*)getData(), 1, getDataSize(), file);
	LLFile::close(file);
	return true;
}

S8 LLImageFormatted::getCodec() const
{
	return mCodec;
}

static void avg4_colors4(const U8* a, const U8* b, const U8* c,
						 const U8* d, U8* dst)
{
	dst[0] = (U8)(((U32)(a[0]) + b[0] + c[0] + d[0])>>2);
	dst[1] = (U8)(((U32)(a[1]) + b[1] + c[1] + d[1])>>2);
	dst[2] = (U8)(((U32)(a[2]) + b[2] + c[2] + d[2])>>2);
	dst[3] = (U8)(((U32)(a[3]) + b[3] + c[3] + d[3])>>2);
}

static void avg4_colors3(const U8* a, const U8* b, const U8* c,
						 const U8* d, U8* dst)
{
	dst[0] = (U8)(((U32)(a[0]) + b[0] + c[0] + d[0])>>2);
	dst[1] = (U8)(((U32)(a[1]) + b[1] + c[1] + d[1])>>2);
	dst[2] = (U8)(((U32)(a[2]) + b[2] + c[2] + d[2])>>2);
}

static void avg4_colors2(const U8* a, const U8* b, const U8* c,
						 const U8* d, U8* dst)
{
	dst[0] = (U8)(((U32)(a[0]) + b[0] + c[0] + d[0])>>2);
	dst[1] = (U8)(((U32)(a[1]) + b[1] + c[1] + d[1])>>2);
}

void LLImageBase::setDataAndSize(U8* data, S32 size)
{
	mData = data;
	mDataSize = size;
}

//static
void LLImageBase::generateMip(const U8* indata, U8* mipdata,
							  S32 width, S32 height, S32 nchannels)
{
	llassert(width > 0 && height > 0);
	U8* data = mipdata;
	S32 in_width = width * 2;
	for (S32 h = 0; h < height; ++h)
	{
		for (S32 w = 0; w < width; ++w)
		{
			switch (nchannels)
			{
				case 4:
					avg4_colors4(indata, indata + 4, indata + 4 * in_width,
								 indata + 4 * in_width + 4, data);
				break;
				case 3:
					avg4_colors3(indata, indata + 3, indata + 3 * in_width,
								 indata + 3 * in_width + 3, data);
				break;
				case 2:
					avg4_colors2(indata, indata + 2, indata + 2 * in_width,
								 indata + 2 * in_width + 2, data);
				break;
				case 1:
					*(U8*)data = (U8)(((U32)(indata[0]) + indata[1] +
									   indata[in_width] +
									   indata[in_width + 1]) >> 2);
				break;
					default:
					llerrs << "Bad number of channels" << llendl;
			}
			indata += nchannels * 2;
			data += nchannels;
		}
		indata += nchannels * in_width; // skip odd lines
	}
}

//static
F32 LLImageBase::calc_download_priority(F32 virtual_size, F32 visible_pixels,
										S32 bytes_sent)
{
	F32 bytes_weight = 1.f;
	if (!bytes_sent)
	{
		bytes_weight = 20.f;
	}
	else if (bytes_sent < 1000)
	{
		bytes_weight = 1.f;
	}
	else if (bytes_sent < 2000)
	{
		bytes_weight = 1.f / 1.5f;
	}
	else if (bytes_sent < 4000)
	{
		bytes_weight = 1.f / 3.f;
	}
	else if (bytes_sent < 8000)
	{
		bytes_weight = 1.f / 6.f;
	}
	else if (bytes_sent < 16000)
	{
		bytes_weight = 1.f / 12.f;
	}
	else if (bytes_sent < 32000)
	{
		bytes_weight = 1.f / 20.f;
	}
	else if (bytes_sent < 64000)
	{
		bytes_weight = 1.f / 32.f;
	}
	else
	{
		bytes_weight = 1.f / 64.f;
	}
	bytes_weight *= bytes_weight;

	F32 virtual_size_factor = virtual_size * 0.01f;

	// The goal is for weighted priority is to be <= 0 when we have reached a
	// point where we have sent enough data.
	F32 w_priority = log10f(bytes_weight * virtual_size_factor);

	// We do not want to affect how MANY bytes we send based on the visible
	// pixels, but the order in which they are sent. We post-multiply so we do
	// not change the zero point.
	if (w_priority > 0.f)
	{
		F32 pixel_weight = log10f(visible_pixels + 1) * 3.f;
		w_priority *= pixel_weight;
	}

	return w_priority;
}
