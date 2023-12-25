/**
 * @file vorbisencode.cpp
 * @brief Vorbis encoding routine routine for Indra.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "vorbis/vorbisenc.h"

#include "llvorbisencode.h"

#include "llmath.h"
#include "llrand.h"

constexpr S64 HEADER_SIZE = 44;

S32 check_for_invalid_wav_formats(const std::string& in_fname,
								  std::string& error_msg, F32 max_duration)
{
	error_msg.clear();

	S64 physical_file_size = 0;
	LLFile infile(in_fname, "rb", &physical_file_size);	
	if (!infile)
	{
		llwarns << "Could not open for read: " << in_fname << llendl;
		error_msg = "CannotUploadSoundFile";
		return LLVORBISENC_SOURCE_OPEN_ERR;
	}

	U8 wav_header[HEADER_SIZE];
	if (infile.read(wav_header, HEADER_SIZE) != HEADER_SIZE)
	{
		llwarns << "Could not open read wav header of file: " << in_fname
				<< llendl;
		error_msg = "CannotUploadSoundFile";
		return LLVORBISENC_SOURCE_OPEN_ERR;
	}

	if (strncmp((char*)&(wav_header[0]), "RIFF", 4))
	{
		error_msg = "SoundFileNotRIFF";
		return LLVORBISENC_WAV_FORMAT_ERR;
	}

	if (strncmp((char*)&(wav_header[8]), "WAVE", 4))
	{
		error_msg = "SoundFileNotRIFF";
		return LLVORBISENC_WAV_FORMAT_ERR;
	}

	// Parse the chunks

	S64 chunk_length = 0;
	S64 raw_data_length = 0;
	U32 bytes_per_sec = 0;
	U32 sample_rate = 0;
	U32 bits_per_sample = 0;
	U16 num_channels = 0;
	bool uncompressed_pcm = false;
	// Start at the first chunk (usually fmt but not always)
	S64 file_pos = 12;

	while (file_pos + 8 < physical_file_size)
	{
		infile.seek(file_pos);
		infile.read(wav_header, HEADER_SIZE);

		chunk_length = ((U32)wav_header[7] << 24) +
					   ((U32)wav_header[6] << 16) +
					   ((U32)wav_header[5] << 8) + wav_header[4];

		if (chunk_length > physical_file_size - file_pos - 4)
		{
			error_msg = "SoundFileInvalidChunkSize";
			return LLVORBISENC_CHUNK_SIZE_ERR;
		}

		LL_DEBUGS("VorbisEncode") << "Chunk found: '" << wav_header[0]
								  << wav_header[1] << wav_header[2]
								  << wav_header[3] << "'" << LL_ENDL;

		if (!strncmp((char*)&(wav_header[0]), "fmt ", 4))
		{
			if (wav_header[8] == 0x01 && wav_header[9] == 0x00)
			{
				uncompressed_pcm = true;
			}
			num_channels = ((U16)wav_header[11] << 8) + wav_header[10];
			sample_rate = ((U32)wav_header[15] << 24) +
						  ((U32)wav_header[14] << 16) +
						  ((U32)wav_header[13] << 8) + wav_header[12];
			bits_per_sample = ((U16)wav_header[23] << 8) + wav_header[22];
			bytes_per_sec = ((U32)wav_header[19] << 24) +
							((U32) wav_header[18] << 16) +
							((U32) wav_header[17] << 8) + wav_header[16];
		}
		else if (!strncmp((char*)&(wav_header[0]), "data", 4))
		{
			raw_data_length = chunk_length;
		}
		file_pos += chunk_length + 8;
		chunk_length = 0;
	}

	if (!uncompressed_pcm)
	{
		error_msg = "SoundFileNotPCM";
		return LLVORBISENC_PCM_FORMAT_ERR;
	}

	if (num_channels < 1 || num_channels > LLVORBIS_CLIP_MAX_CHANNELS)
	{
		error_msg = "SoundFileInvalidChannelCount";
		return LLVORBISENC_MULTICHANNEL_ERR;
	}

	if (sample_rate != LLVORBIS_CLIP_SAMPLE_RATE)
	{
		error_msg = "SoundFileInvalidSampleRate";
		return LLVORBISENC_UNSUPPORTED_SAMPLE_RATE;
	}

	if (bits_per_sample != 16 && bits_per_sample != 8)
	{
		error_msg = "SoundFileInvalidWordSize";
		return LLVORBISENC_UNSUPPORTED_WORD_SIZE;
	}

	if (!raw_data_length)
	{
		error_msg = "SoundFileInvalidHeader";
		return LLVORBISENC_CLIP_TOO_LONG;
	}

	if (max_duration <= LLVORBIS_CLIP_MAX_TIME)
	{
		max_duration = LLVORBIS_CLIP_MAX_TIME;
	}
	F32 clip_length = (F32)raw_data_length / (F32)bytes_per_sec;
	if (clip_length > max_duration)
	{
		error_msg = "SoundFileInvalidTooLong";
		return LLVORBISENC_CLIP_TOO_LONG;
	}

    return LLVORBISENC_NOERR;
}

#define READ_BUFFER 1024
S32 encode_vorbis_file(const std::string& in_fname,
					   const std::string& out_fname, F32 max_duration)
{
	S32 format_error = 0;
	std::string error_msg;
	if ((format_error = check_for_invalid_wav_formats(in_fname, error_msg,
													  max_duration)))
	{
		llwarns << error_msg << ": " << in_fname << llendl;
		return format_error;
	}

	LLFile infile(in_fname, "rb");	
	if (!infile)
	{
		llwarns << "Could not open sound file for reading and upload: "
				<< in_fname << llendl;
		return LLVORBISENC_SOURCE_OPEN_ERR;
	}

	LLFile outfile(out_fname, "w+b");	
	if (!outfile)
	{
		llwarns << "Could not open temporary ogg file for writing: "
				<< in_fname << llendl;
		return LLVORBISENC_DEST_OPEN_ERR;
	}

	// Out of the data segment, not the stack
	U8 readbuffer[READ_BUFFER * 4 + HEADER_SIZE];

	// Take physcal pages, weld into a logical stream of packets
	ogg_stream_state os;
	// One Ogg bitstream page. Vorbis packets are inside
	ogg_page og;
	// One raw packet of data for decode
	ogg_packet op;
	// Structure storing all the static vorbis bitstream settings
	vorbis_info vi;
	// Structure storing all the user comments
	vorbis_comment vc;
	// Central working state for the packet->PCM decoder
	vorbis_dsp_state vd;
	// Local working space for packet->PCM decode
	vorbis_block vb;

	S32 eos = 0;
	S32 result;

	U16 num_channels = 0;
	U32 sample_rate = 0;
	U32 bits_per_sample = 0;

	U8 wav_header[HEADER_SIZE];

	S64 data_left = 0;

	// Parse the chunks
	S64 chunk_length = 0;
	// Start at the first chunk (usually fmt but not always)
	S64 file_pos = 12;

	while (!infile.eof() && infile.seek(file_pos) == file_pos &&
		   infile.read(wav_header, HEADER_SIZE) == HEADER_SIZE)
	{
		chunk_length = ((U32)wav_header[7] << 24) +
					   ((U32)wav_header[6] << 16) +
					   ((U32)wav_header[5] << 8) + wav_header[4];

		LL_DEBUGS("VorbisEncode") << "Chunk found: '" << wav_header[0]
								  << wav_header[1] << wav_header[2]
								  << wav_header[3] << "'" << LL_ENDL;

		if (!strncmp((char*)&(wav_header[0]), "fmt ", 4))
		{
			num_channels = ((U16) wav_header[11] << 8) + wav_header[10];
			sample_rate = ((U32) wav_header[15] << 24) +
						  ((U32) wav_header[14] << 16) +
						  ((U32) wav_header[13] << 8) + wav_header[12];
			bits_per_sample = ((U16) wav_header[23] << 8) + wav_header[22];
		}
		else if (!strncmp((char*)&(wav_header[0]), "data", 4))
		{
			infile.seek(file_pos + 8);
			// Leave the file pointer at the beginning of the data chunk data
			data_left = chunk_length;
			break;
		}
		file_pos += chunk_length + 8;
		chunk_length = 0;
	}

	//********** Encode setup ************//

	// Choose an encoding mode:
	// (mode 0: 44kHz stereo uncoupled, roughly 128kbps VBR)
	vorbis_info_init(&vi);

	// Always encode to mono

#if 1
	// SL-52913 & SL-53779 determined this quality level to be our 'good
	// enough' general-purpose quality level with a nice low bitrate.
	// Equivalent to oggenc -q0.5
	F32 quality = 0.05f;
#else
	F32 quality = (bitrate == 128000 ? 0.4f : 0.1f);
#endif

	if (vorbis_encode_init_vbr(&vi, /*num_channels*/ 1, sample_rate, quality))
	{
		llwarns << "Unable to initialize vorbis CODEC at quality "
				<< quality << llendl;
		return LLVORBISENC_DEST_OPEN_ERR;
	}

	// Add a comment
	vorbis_comment_init(&vc);
#if 0
	vorbis_comment_add(&vc, "Linden");
#endif

	// Set up the analysis state and auxiliary encoding storage
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);

	// Set up our packet->stream encoder. Pick a random serial number; that way
	// we can more likely build chained streams just by concatenation.
	 ogg_stream_init(&os, ll_rand());

	// Vorbis streams begin with three headers; the initial header (with most
	// of the CODEC setup parameters) which is mandated by the Ogg bitstream
	// spec. The second header holds any comment fields. The third header holds
	// the bitstream codebook. We merely need to make the headers, then pass
	// them to libvorbis one at a time; libvorbis handles the additional Ogg
	// bitstream constraints.
	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout(&vd, &vc, &header, &header_comm,
								  &header_code);
		// automatically placed in its own page:
		ogg_stream_packetin(&os, &header);
		ogg_stream_packetin(&os, &header_comm);
		ogg_stream_packetin(&os, &header_code);

		// We do not have to write out here, but doing so makes streaming much
		// easier, so we do, flushing ALL pages. This ensures the actual audio
		// data will start on a new page
		while (!eos)
		{
			S32 result = ogg_stream_flush(&os, &og);
			if (result == 0) break;
			outfile.write((U8*)og.header, og.header_len);
			outfile.write((U8*)og.body, og.body_len);
		}
	}

	while (!eos)
	{
		S64 bytes_per_sample = bits_per_sample / 8;

		// stereo hardwired here
		S64 n = llclamp((S64)(READ_BUFFER * num_channels * bytes_per_sample),
						S64(0), data_left);
		S64 bytes = infile.read(readbuffer, n);
		if (bytes != n)
		{
			// End of file. This can be done implicitly in the mainline, but
			// it is easier to see here in non-clever fashion. Tell the library
			// we are at end of stream so that it can handle the last frame and
			// mark end of stream in the output properly
			vorbis_analysis_wrote(&vd, 0);
		 }
		 else
		 {
			data_left -= bytes;

			// Expose the buffer to submit data
			F32** buffer = vorbis_analysis_buffer(&vd, READ_BUFFER);

			S64 i = 0;
			S64 samples = bytes / (num_channels * bytes_per_sample);
			S32 temp;

			if (num_channels == 2)
			{
				if (bytes_per_sample == 2)
				{
					// Uninterleave samples
					for (i = 0; i < samples; ++i)
					{
						temp = ((char*)readbuffer)[i * 4 + 1];
						temp += ((char*)readbuffer)[i * 4 + 3];
						temp <<= 8;
						temp += readbuffer[i * 4];
						temp += readbuffer[i * 4 + 2];

						buffer[0][i] = F32(temp) * (1.f / 65536.f);
					}
				}
				else
				{
					// Presume it is 1 byte per which is unsigned (F#@%ing wav
					// "standard")

					// Uninterleave samples
					for (i = 0; i < samples; ++i)
					{
					 	temp = readbuffer[i * 2];
						temp += readbuffer[i * 2 + 1];
						temp -= 256;
						buffer[0][i] = F32(temp) * (1.f / 256.f);
					}
				}
			}
			else if (num_channels == 1)
			{
				if (bytes_per_sample == 2)
				{
					for (i = 0; i < samples; ++i)
					{
					 	temp = ((char*)readbuffer)[i * 2 + 1];
						temp <<= 8;
						temp += readbuffer[i * 2];
						buffer[0][i] = F32(temp) * (1.f / 32768.f);
					}
				}
				else
				{
					// Presume it is 1 byte per which is unsigned (F#@%ing wav
					// "standard")
					for (i = 0; i < samples; ++i)
					{
						temp = readbuffer[i];
						temp -= 128;
						buffer[0][i] = F32(temp) * (1.f / 128.f);
					}
				}
			}

			// Tell the library how much we actually submitted
			vorbis_analysis_wrote(&vd,i);
		}

		// Vorbis does some data preanalysis, then divvies up blocks for more
		// involved (potentially parallel) processing. Get a single block for
		// encoding now.
		while (vorbis_analysis_blockout(&vd, &vb) == 1)
		{
			// Analysis. Do the main analysis, creating a packet.
			vorbis_analysis(&vb, NULL);
			vorbis_bitrate_addblock(&vb);

			while (vorbis_bitrate_flushpacket(&vd, &op))
			{
				// Weld the packet into the bitstream
				ogg_stream_packetin(&os, &op);

				// Write out pages (if any)
				while (!eos)
				{
					result = ogg_stream_pageout(&os,&og);
					if (result == 0) break;

					outfile.write((U8*)og.header, og.header_len);
					outfile.write((U8*)og.body, og.body_len);

					// This could be set above, but for illustrative purposes,
					// I do it here (to show that vorbis does know where the
					// stream ends).
					if (ogg_page_eos(&og))
					{
						eos = 1;
					}
				}
			}
		}
	}

	// Clean up and exit. vorbis_info_clear() must be called last. ogg_page and
	// ogg_packet structs always point to storage in libvorbis. They are never
	// freed or manipulated directly.
	ogg_stream_clear(&os);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);

	llinfos << "Vorbis encoding done." << llendl;

	return LLVORBISENC_NOERR;
}
