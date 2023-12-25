/**
 * @file llimagej2c.cpp
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

#include "openjpeg.h"

#include "llimagej2c.h"

#include "lldir.h"

// Helper function
static LL_INLINE int ceildivpow2(int a, int b)
{
	// Divide a by b to the power of 2 and round upwards.
	return (a + (1 << b) - 1) >> b;
}

LLImageJ2C::LLImageJ2C()
:	LLImageFormatted(IMG_CODEC_J2C),
	mMaxBytes(0),
	mRawDiscardLevel(-1),
	mRate(0.f),
	mReversible(false)
{
}

//static
std::string LLImageJ2C::getEngineInfo()
{
	static std::string version_string = std::string("OpenJPEG: ") +
										opj_version();
	return version_string.c_str();
}

//virtual
void LLImageJ2C::setLastError(const std::string& message,
							  const std::string& filename)
{
	mLastError = message;
	if (!filename.empty())
	{
		mLastError += std::string(" FILE: ") + filename;
	}
}

bool LLImageJ2C::updateData()
{
	bool res = true;
	resetLastError();

	// Check to make sure that this instance has been initialized with data
	if (!getData() || getDataSize() < 16)
	{
		setLastError("LLImageJ2C uninitialized");
		res = false;
	}
	else
	{
		res = getMetadata();
	}

	if (res)
	{
		// SJB: override discard based on mMaxBytes elsewhere
		S32 max_bytes = getDataSize(); // mMaxBytes ? mMaxBytes : getDataSize();
		S32 discard = calcDiscardLevelBytes(max_bytes);
		setDiscardLevel(discard);
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}

//static
S32 LLImageJ2C::calcDataSizeJ2C(S32 w, S32 h, S32 comp, S32 discard_level,
								F32 rate)
{
	if (rate <= 0.f)
	{
		rate = .125f;
	}

	while (discard_level > 0)
	{
		if (w < 1 || h < 1)
		{
			break;
		}
		w >>= 1;
		h >>= 1;
		--discard_level;
	}

	S32 bytes = (S32)((F32)(w * h * comp) * rate);
	bytes = llmax(bytes, FIRST_PACKET_SIZE);
	return bytes;
}

S32 LLImageJ2C::calcDataSize(S32 discard_level)
{
	return calcDataSizeJ2C(getWidth(), getHeight(), getComponents(),
						   discard_level, mRate);
}

S32 LLImageJ2C::calcDiscardLevelBytes(S32 bytes)
{
	if (bytes < 0)
	{
		llwarns << "Negative bytes amount passed !" << llendl;
		llassert(false);
		return MAX_DISCARD_LEVEL;
	}
	else if (bytes == 0)
	{
		return MAX_DISCARD_LEVEL;
	}

	S32 discard_level = 0;
	while (true)
	{
		S32 bytes_needed = calcDataSize(discard_level); // virtual
		// For J2C, up the res at 75% of the optimal number of bytes:
		if (bytes >= bytes_needed - (bytes_needed >> 2))
		{
			break;
		}
		if (++discard_level >= MAX_DISCARD_LEVEL)
		{
			break;
		}
	}
	return discard_level;
}

bool LLImageJ2C::loadAndValidate(const std::string& filename)
{
	bool res = true;

	resetLastError();

	S64 file_size = 0;
	LLFile infile(filename, "rb", &file_size);
	if (!infile)
	{
		setLastError("Unable to open file for reading", filename);
		res = false;
	}
	else if (file_size == 0)
	{
		setLastError("File is empty", filename);
		res = false;
	}
	else
	{
		U8* data = (U8*)allocate_texture_mem(file_size);
		if (!data)
		{
			setLastError("Out of memory", filename);
			return false;
		}

		S64 bytes_read = infile.read(data, file_size);
		if (bytes_read != file_size)
		{
			free_texture_mem(data);
			setLastError("Unable to read entire file");
			res = false;
		}
		else
		{
			res = validate(data, file_size);
		}
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}

	return res;
}

bool LLImageJ2C::validate(U8* data, U32 file_size)
{
	if (!data) return false;

	resetLastError();

	setData(data, file_size);

	bool res = updateData();
	if (res)
	{
		// Check to make sure that this instance has been initialized with data
		if (!getData() || getDataSize() == 0)
		{
			setLastError("LLImageJ2C uninitialized");
			res = false;
		}
		else
		{
			res = getMetadata();
		}
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}

void LLImageJ2C::updateRawDiscardLevel()
{
	mRawDiscardLevel = mMaxBytes ? calcDiscardLevelBytes(mMaxBytes)
								 : mDiscardLevel;
}

// Returns true to mean done, whether successful or not.
bool LLImageJ2C::decodeChannels(LLImageRaw* raw_imagep, S32 first_channel,
								S32 max_channel_count)
{
	if (!raw_imagep) return false;

	resetLastError();

	bool res = true;
	// Check to make sure that this instance has been initialized with data
	if (!getData() || getDataSize() < 16)
	{
		setLastError("LLImageJ2C uninitialized");
	}
#if 1 
	else if (!LLMemory::hasFailedAllocation())
#else
	else
#endif
	{
		// Update the raw discard level
		updateRawDiscardLevel();
		mDecoding = true;
		res = decodeImpl(*raw_imagep, first_channel, max_channel_count);
	}

	if (res)
	{
		if (!mDecoding)
		{
			// Failed
			raw_imagep->deleteData();
		}
		else
		{
			mDecoding = false;
		}
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}

	return res;
}

bool LLImageJ2C::encode(const LLImageRaw* raw_imagep, const char* comment)
{
	if (!raw_imagep) return false;

	resetLastError();
	bool res = encodeImpl(*raw_imagep, comment);
	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}

bool LLImageJ2C::getMetadataFast(S32& width, S32& height, S32& comps)
{
	constexpr S32 J2K_HEADER_LENGTH = 42;
	if (getDataSize() < J2K_HEADER_LENGTH)
	{
		return false;
	}

	const U8* rawp = getData();
	if (!rawp || *rawp != 0xff || rawp[1] != 0x4f || rawp[2] != 0xff ||
		rawp[3] != 0x51)
	{
		return false;
	}

	width = (rawp[8] << 24) + (rawp[9] << 16) + (rawp[10] << 8) + rawp[11] -
			(rawp[16] << 24) - (rawp[17] << 16) - (rawp[18] << 8) - rawp[19];
	height = (rawp[12] << 24) + (rawp[13] << 16) + (rawp[14] << 8) + rawp[15] -
			 (rawp[20] << 24) - (rawp[21] << 16) - (rawp[22] << 8) - rawp[23];
	comps = (rawp[40] << 8) + rawp[41];

	return true;
}

// Callback method for OpenJPEG warnings and errors.
//static
void LLImageJ2C::eventMgrCallback(const char* msg, void*)
{
	if (msg && *msg)
	{
		std::string message = msg;
		if (message.back() == '\n')
		{
			message.pop_back();
		}
		llwarns << message << llendl;
	}
}

// Implementation details specific methods

static opj_event_mgr_t sEventMgr; // OpenJPEG event manager

// Event manager initialization.
//static
void LLImageJ2C::initEventManager()
{
	static bool event_mgr_initialized = false;
	if (!event_mgr_initialized)
	{
		event_mgr_initialized = true;
		// Configure the event callbacks (not required); setting of each
		// callback is optional
		memset(&sEventMgr, 0, sizeof(opj_event_mgr_t));
		sEventMgr.error_handler = LLImageJ2C::eventMgrCallback;
		sEventMgr.warning_handler = LLImageJ2C::eventMgrCallback;
#if 0	// INFO messages are not interesting to us
		sEventMgr.info_handler = LLImageJ2C::eventMgrCallback;
#endif
	}
}

bool LLImageJ2C::getMetadata()
{
	// Update the raw discard level
	updateRawDiscardLevel();

	S32 width = 0;
	S32 height = 0;
	S32 img_components = 0;

	// Try it the fast way first...
	if (getMetadataFast(width, height, img_components))
	{
		setSize(width, height, img_components);
		return true;
	}

	initEventManager();

	// *FIXME: We get metadata by decoding the ENTIRE image.
	opj_dparameters_t parameters;	// decompression parameters
	opj_image_t* image = NULL;
	opj_dinfo_t* dinfo = NULL;		// handle to a decompressor
	opj_cio_t* cio = NULL;

	// Set decoding parameters to default values
	opj_set_default_decoder_parameters(&parameters);

	// Only decode what is required to get the size data.
	parameters.cp_limit_decoding = LIMIT_TO_MAIN_HEADER;
#if 0
	parameters.cp_reduce = mRawDiscardLevel;
#endif

	// Get a decoder handle
	dinfo = opj_create_decompress(CODEC_J2K);

	// Catch events using our callbacks and give a local context
	opj_set_event_mgr((opj_common_ptr)dinfo, &sEventMgr, NULL);

	// Setup the decoder decoding parameters using user parameters
	opj_setup_decoder(dinfo, &parameters);

	// Open a byte stream
	cio = opj_cio_open((opj_common_ptr)dinfo, getData(), getDataSize());

	// Decode the stream and fill the image structure
	image = opj_decode(dinfo, cio);

	// Close the byte stream
	opj_cio_close(cio);

	// Free remaining structures
	if (dinfo)
	{
		opj_destroy_decompress(dinfo);
	}

	if (!image)
	{
		llwarns << "Failed to decode image !" << llendl;
		return false;
	}

	// Copy image data into our raw image format (instead of the separate
	// channel format
	img_components = image->numcomps;
	width = image->x1 - image->x0;
	height = image->y1 - image->y0;
	setSize(width, height, img_components);

	// Free image data structure
	opj_image_destroy(image);

	return true;
}

// Decodes the JPEG-2000 code-stream
bool LLImageJ2C::decodeImpl(LLImageRaw& raw_image, S32 first_channel,
							S32 max_channel_count)
{
	initEventManager();

	opj_dparameters_t parameters;	// Decompression parameters
	opj_image_t* image = NULL;
	opj_dinfo_t* dinfo = NULL;		// Handle to a decompressor
	opj_cio_t* cio = NULL;

	// Set decoding parameters to default values
	opj_set_default_decoder_parameters(&parameters);

	parameters.cp_reduce = getRawDiscardLevel();

	// Get a decoder handle
	dinfo = opj_create_decompress(CODEC_J2K);

	// Catch events using our callbacks and give a local context
	opj_set_event_mgr((opj_common_ptr)dinfo, &sEventMgr, NULL);

	// Setup the decoder decoding parameters using user parameters
	opj_setup_decoder(dinfo, &parameters);

	// Open a byte stream
	cio = opj_cio_open((opj_common_ptr)dinfo, getData(), getDataSize());

	// Decode the stream and fill the image structure
	image = opj_decode(dinfo, cio);

	// Close the byte stream
	opj_cio_close(cio);

	// Free remaining structures
	if (dinfo)
	{
		opj_destroy_decompress(dinfo);
	}

	// The image decode failed if the return was NULL or the component count
	// was zero. The latter is just a sanity check before we dereference the
	// array.
	if (!image || !image->numcomps)
	{
		llwarns << "Failed to decode image !" << llendl;
		if (image)
		{
			opj_image_destroy(image);
		}
		mDecoding = false;
		return true; // Done
	}

	// Sometimes we get bad data out of the cache - check to see if the decode
	// succeeded
	for (S32 i = 0; i < image->numcomps; ++i)
	{
		if (image->comps[i].factor != getRawDiscardLevel())
		{
			llwarns << "Expected discard level not reached" << llendl;
			// If we did not get the discard level we are expecting, fail
			opj_image_destroy(image);
			mDecoding = false;
			return true;
		}
	}

	if (image->numcomps <= first_channel)
	{
		llwarns << "Trying to decode more channels than are present in image: numcomps = "
				<< image->numcomps << " - first_channel = " << first_channel
				<< llendl;
		opj_image_destroy(image);
		mDecoding = false;
		return true;
	}

	// Copy image data into our raw image format (instead of the separate
	// channel format

	S32 img_components = image->numcomps;
	S32 channels = img_components - first_channel;
	if (channels > max_channel_count)
	{
		channels = max_channel_count;
	}

	// Component buffers are allocated in an image width by height buffer.
	// The image placed in that buffer is ceil(width/2^factor) by
	// ceil(height/2^factor) and if the factor isn't zero it will be at the
	// top left of the buffer with black filled in the rest of the pixels.
	// It is integer math so the formula is written in ceildivpo2.
	// (Assuming all the components have the same width, height and
	// factor.)
	S32 comp_width = image->comps[0].w;
	S32 f = image->comps[0].factor;
	S32 width = ceildivpow2(image->x1 - image->x0, f);
	S32 height = ceildivpow2(image->y1 - image->y0, f);
	raw_image.resize(width, height, channels);
	U8* rawp = raw_image.getData();
	if (!rawp)
	{
		setLastError("Could not create raw image");
		opj_image_destroy(image);
		return true;
	}

	// First_channel is what channel to start copying from dest is what channel
	// to copy to. first_channel comes from the argument, dest always starts
	// writing at channel zero.
	for (S32 comp = first_channel, dest = 0; comp < first_channel + channels;
		 ++comp, ++dest)
	{
		if (image->comps[comp].data)
		{
			S32 offset = dest;
			for (S32 y = height - 1; y >= 0; --y)
			{
				for (S32 x = 0; x < width; ++x)
				{
					rawp[offset] = image->comps[comp].data[y * comp_width + x];
					offset += channels;
				}
			}
		}
		else	// Some rare OpenJPEG versions have this bug.
		{
			llwarns << "Failed to decode image ! (NULL comp data - OpenJPEG bug)"
					<< llendl;
			opj_image_destroy(image);
			mDecoding = false;
			return true; // Done
		}
	}

	// Free image data structure
	opj_image_destroy(image);
	return true; // Done
}

// Decodes the JPEG-2000 code-stream
bool LLImageJ2C::encodeImpl(const LLImageRaw& raw_image, const char* comment)
{
	initEventManager();

	constexpr S32 MAX_COMPS = 5;
	opj_cparameters_t parameters;	// compression parameters

	// Set encoding parameters to default values
	opj_set_default_encoder_parameters(&parameters);
	parameters.cod_format = 0;
	parameters.cp_disto_alloc = 1;

	if (mReversible)
	{
		parameters.tcp_numlayers = 1;
		parameters.tcp_rates[0] = 0.f;
	}
	else
	{
		parameters.tcp_numlayers = 5;
		parameters.tcp_rates[0] = 1920.f;
		parameters.tcp_rates[1] = 480.f;
		parameters.tcp_rates[2] = 120.f;
		parameters.tcp_rates[3] = 30.f;
		parameters.tcp_rates[4] = 10.f;
		parameters.irreversible = 1;
		if (raw_image.getComponents() >= 3)
		{
			parameters.tcp_mct = 1;
		}
	}

	if (!comment)
	{
		parameters.cp_comment = (char*)"";
	}
	else
	{
		// *HACK: awful cast, too lazy to copy right now.
		parameters.cp_comment = (char*)comment;
	}

	// Fill in the source image from our raw image
	OPJ_COLOR_SPACE color_space = CLRSPC_SRGB;
	opj_image_cmptparm_t cmptparm[MAX_COMPS];
	opj_image_t* image = NULL;
	S32 numcomps = llmin((S32)raw_image.getComponents(), MAX_COMPS);
	S32 width = raw_image.getWidth();
	S32 height = raw_image.getHeight();
	memset(&cmptparm[0], 0, MAX_COMPS * sizeof(opj_image_cmptparm_t));
	for (S32 c = 0; c < numcomps; ++c)
	{
		cmptparm[c].prec = 8;
		cmptparm[c].bpp = 8;
		cmptparm[c].sgnd = 0;
		cmptparm[c].dx = parameters.subsampling_dx;
		cmptparm[c].dy = parameters.subsampling_dy;
		cmptparm[c].w = width;
		cmptparm[c].h = height;
	}

	// Create the image
	image = opj_image_create(numcomps, &cmptparm[0], color_space);
	if (!image)
	{
		llwarns << "Could not create image: out of memory ?" << llendl;
		// Free user parameters structure
		if (parameters.cp_matrice)
		{
			free(parameters.cp_matrice);
		}
		return false;
	}

	image->x1 = width;
	image->y1 = height;

	S32 i = 0;
	const U8* src_datap = raw_image.getData();
	for (S32 y = height - 1; y >= 0; --y)
	{
		for (S32 x = 0; x < width; ++x)
		{
			const U8* pixel = src_datap + (y * width + x) * numcomps;
			for (S32 c = 0; c < numcomps; ++c)
			{
				image->comps[c].data[i] = *pixel++;
			}
			++i;
		}
	}

	// Encode the destination image

	int codestream_length;
	opj_cio_t* cio = NULL;

	// Get a J2K compressor handle
	opj_cinfo_t* cinfo = opj_create_compress(CODEC_J2K);

	// Catch events using our callbacks and give a local context
	opj_set_event_mgr((opj_common_ptr)cinfo, &sEventMgr, NULL);

	// Setup the encoder parameters using the current image and using user
	// parameters
	opj_setup_encoder(cinfo, &parameters, image);

	// Allocate memory for all tiles
	cio = opj_cio_open((opj_common_ptr)cinfo, NULL, 0);

	// Encode the image
	if (!opj_encode(cinfo, cio, image, NULL))
	{
		opj_cio_close(cio);
		llwarns << "Failed to encode image." << llendl;
		return false;
	}

	codestream_length = cio_tell(cio);
	copyData(cio->buffer, codestream_length);
	updateData(); // Set width, height

	// Close and free the byte stream
	opj_cio_close(cio);

	// Free remaining compression structures
	opj_destroy_compress(cinfo);

	// Free user parameters structure
	if (parameters.cp_matrice)
	{
		free(parameters.cp_matrice);
	}

	// Free image data
	opj_image_destroy(image);

	return true;
}
