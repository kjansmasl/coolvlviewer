/**
 * @file llfontregistry.cpp
 * @author Brad Payne
 * @brief Storage for fonts.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "boost/tokenizer.hpp"

#include "llfontregistry.h"

#include "lldir.h"
#include "llfontfreetype.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llwindow.h"
#include "llxmlnode.h"

#define NORM_DESC 1

#if LL_WINDOWS
# define CURRENT_OS_NAME "Windows"
#elif LL_DARWIN
# define CURRENT_OS_NAME "Mac"
#elif LL_LINUX
# define CURRENT_OS_NAME "Linux"
#else
# define CURRENT_OS_NAME ""
#endif

static const std::string s_template_string("TEMPLATE");

bool font_desc_init_from_xml(LLXMLNodePtr node, LLFontDescriptor& desc)
{
	if (node->hasName("font"))
	{
		std::string attr_name;
		if (node->getAttributeString("name", attr_name))
		{
#if LL_WINDOWS
			// File names are case-insensitive under Windows... HB
			LLStringUtil::toLower(attr_name);
#endif
			desc.setName(attr_name);
		}

		std::string attr_style;
		if (node->getAttributeString("font_style", attr_style))
		{
			desc.setStyle(LLFontGL::getStyleFromString(attr_style));
		}

		desc.setSize(s_template_string);
	}

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		std::string child_name;
		child->getAttributeString("name", child_name);
		if (child->hasName("file"))
		{
			std::string fontname = child->getTextContents();
#if LL_WINDOWS
			// File names are case-insensitive under Windows... HB
			LLStringUtil::toLower(fontname);
#endif
			desc.getFileNames().emplace_back(fontname);
		}
		else if (child->hasName("os"))
		{
			if (child_name == CURRENT_OS_NAME)
			{
				font_desc_init_from_xml(child, desc);
			}
		}
	}
	return true;
}

bool init_from_xml(LLFontRegistry* registry, LLXMLNodePtr node)
{
	LLXMLNodePtr child;

	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		std::string child_name;
		child->getAttributeString("name", child_name);
		if (child->hasName("font"))
		{
			LLFontDescriptor desc;
			bool font_succ = font_desc_init_from_xml(child, desc);
			LLFontDescriptor norm_desc = desc.normalize();
			if (font_succ)
			{
				// If this is the first time we have seen this font name,
				// create a new template map entry for it.
				const LLFontDescriptor* match_desc =
					registry->getMatchingFontDesc(desc);
				if (!match_desc)
				{
					// Create a new entry (with no corresponding font).
					registry->mFontMap[norm_desc] = NULL;
				}
				// Otherwise, find the existing entry and combine data.
				else
				{
					// Prepend files from desc. A little roundabout because the
					// map key is const, so we have to fetch it, make a new map
					// key and replace the old entry.
					string_vec_t match_file_names = match_desc->getFileNames();
					match_file_names.insert(match_file_names.begin(),
											desc.getFileNames().begin(),
											desc.getFileNames().end());
					LLFontDescriptor new_desc = *match_desc;
					new_desc.getFileNames() = match_file_names;
					registry->mFontMap.erase(*match_desc);
					registry->mFontMap[new_desc] = NULL;
				}
			}
		}
		else if (child->hasName("font_size"))
		{
			std::string size_name;
			F32 size_value;
			if (child->getAttributeString("name", size_name) &&
				child->getAttributeF32("size", size_value))
			{
				registry->mFontSizes[size_name] = size_value;
			}

		}
	}
	return true;
}

LLFontDescriptor::LLFontDescriptor()
:	mStyle(0)
{
}

LLFontDescriptor::LLFontDescriptor(const std::string& name,
								   const std::string& size, U8 style,
								   const string_vec_t& file_names)
:	mName(name),
	mSize(size),
	mStyle(style),
	mFileNames(file_names)
{
#if LL_WINDOWS
	// File names are case-insensitive under Windows... HB
	LLStringUtil::toLower(mName);
#endif
}

LLFontDescriptor::LLFontDescriptor(const std::string& name,
								   const std::string& size, U8 style)
:	mName(name),
	mSize(size),
	mStyle(style)
{
#if LL_WINDOWS
	// File names are case-insensitive under Windows... HB
	LLStringUtil::toLower(mName);
#endif
}

bool LLFontDescriptor::operator<(const LLFontDescriptor& b) const
{
	if (mName == b.mName)
	{
		if (mStyle == b.mStyle)
		{
			return mSize < b.mSize;
		}
		return mStyle < b.mStyle;
	}
	return mName < b.mName;
}

bool LLFontDescriptor::isTemplate() const
{
	return getSize() == s_template_string;
}

// Look for substring match and remove substring if matched.
bool removeSubString(std::string& str, const std::string& substr)
{
	size_t pos = str.find(substr);
	if (pos == std::string::npos)
	{
		return false;
	}
	str.erase(pos, substr.size());
	return true;
}

// Normal form is
// - raw name
// - bold, italic style info reflected in both style and font name.
// - other style info removed.
// - size info moved to mSize, defaults to Medium
// For example,
// - "SansSerifHuge" would normalize to { "SansSerif", "Huge", 0 }
// - "SansSerifBold" would normalize to { "SansSerifBold", "Medium", BOLD }
LLFontDescriptor LLFontDescriptor::normalize() const
{
#if LL_WINDOWS
	// File names are case-insensitive under Windows... HB
	static const std::string tiny_str("tiny");
	static const std::string small_str("small");
	static const std::string big_str("big");
	static const std::string medium_str("medium");
	static const std::string large_str("large");
	static const std::string huge_str("huge");
	static const std::string monospace_str("monospace");
	static const std::string bold_str("bold");
	static const std::string italic_str("italic");
#else
	static const std::string tiny_str("Tiny");
	static const std::string small_str("Small");
	static const std::string big_str("Big");
	static const std::string medium_str("Medium");
	static const std::string large_str("Marge");
	static const std::string huge_str("Huge");
	static const std::string monospace_str("Monospace");
	static const std::string bold_str("Bold");
	static const std::string italic_str("Italic");
#endif

	std::string new_name(mName);
#if LL_WINDOWS
	// File names are case-insensitive under Windows... HB
	LLStringUtil::toLower(new_name);
#endif
	std::string new_size(mSize);
	U8 new_style(mStyle);

	// Only care about style to extent it can be picked up by font.
	new_style &= LLFontGL::BOLD | LLFontGL::ITALIC;

	// All these transformations are to support old-style font specifications.
	if (removeSubString(new_name, tiny_str))
	{
		new_size = "Tiny";
	}
	if (removeSubString(new_name, small_str))
	{
		new_size = "Small";
	}
	if (removeSubString(new_name, medium_str))
	{
		new_size = "Medium";
	}
	if (removeSubString(new_name, big_str))
	{
		new_size = "Large";
	}
	if (removeSubString(new_name, large_str))
	{
		new_size = "Large";
	}
	if (removeSubString(new_name, huge_str))
	{
		new_size = "Huge";
	}

	// *HACK: Monospace is the only one we do not remove, so name "Monospace"
	// does not get taken down to "". For other fonts, there is no ambiguity
	// between font name and size specifier.
	if (new_size != s_template_string && new_size.empty() &&
		new_name.find(monospace_str) != std::string::npos)
	{
		new_size = "Monospace";
	}
	if (new_size.empty())
	{
		new_size = "Medium";
	}

	if (removeSubString(new_name, bold_str))
	{
		new_style |= LLFontGL::BOLD;
	}

	if (removeSubString(new_name, italic_str))
	{
		new_style |= LLFontGL::ITALIC;
	}

	return LLFontDescriptor(new_name, new_size, new_style, mFileNames);
}

LLFontRegistry::LLFontRegistry(const string_vec_t& xui_paths,
							   bool create_gl_textures)
:	mCreateGLTextures(create_gl_textures)
{
	// Propagate this down from LLUICtrlFactory so LLRender does not need an
	// upstream dependency on LLUI.
	mXUIPaths = xui_paths;

	// This is potentially a slow directory traversal, so we want to cache the
	// result.
	mUltimateFallbackList = LLWindow::getDynamicFallbackFontList();
}

LLFontRegistry::~LLFontRegistry()
{
	clear();
}

bool LLFontRegistry::parseFontInfo(const std::string& xml_filename)
{
	bool success = false;  // Succeed if we find at least one XUI file
	LLXMLNodePtr root;
	std::string full_filename, root_name;
	for (S32 i = 0, count = mXUIPaths.size(); i < count; ++i)
	{
		full_filename = gDirUtilp->findSkinnedFilename(mXUIPaths[i],
													   xml_filename);
		if (!LLXMLNode::parseFile(full_filename, root, NULL))
		{
			continue;
		}

		if (root.isNull() || !root->hasName("fonts"))
		{
			llwarns << "Bad font info file: " << full_filename << llendl;
			continue;
		}

		root->getAttributeString("name", root_name);
		if (root->hasName("fonts"))
		{
			// Expect a collection of children consisting of "font" or
			// "font_size" entries
			success |= init_from_xml(this, root);
		}
	}
	if (success)
	{
		dump();
	}

	return success;
}

bool LLFontRegistry::nameToSize(const std::string& size_name, F32& size)
{
	font_size_map_t::iterator it = mFontSizes.find(size_name);
	if (it != mFontSizes.end())
	{
		size = it->second;
		return true;
	}
	return false;
}

LLFontGL* LLFontRegistry::createFont(const LLFontDescriptor& desc)
{
	// Name should hold a font name recognized as a setting; the value of the
	// setting should be a list of font files. Size should be a recognized
	// string value. Style should be a set of flags including any implied by
	// the font name.

	// First decipher the requested size.
	LLFontDescriptor norm_desc = desc.normalize();
	F32 point_size;
	bool found_size = nameToSize(norm_desc.getSize(), point_size);
	if (!found_size)
	{
		llwarns << "Unrecognized size " << norm_desc.getSize() << llendl;
		return NULL;
	}
	llinfos << norm_desc.getName() << " size " << norm_desc.getSize()
			<< " style " << (S32)norm_desc.getStyle() << llendl;

	// Find corresponding font template (based on same descriptor with no size
	// specified)
	LLFontDescriptor template_desc(norm_desc);
	template_desc.setSize(s_template_string);
	const LLFontDescriptor* match_desc = getClosestFontTemplate(template_desc);
	if (match_desc)
	{
		// See whether this best-match font has already been instantiated in
		// the requested size.
		LLFontDescriptor nearest_exact_desc = *match_desc;
		nearest_exact_desc.setSize(norm_desc.getSize());
		font_reg_map_t::iterator it = mFontMap.find(nearest_exact_desc);
		if (it != mFontMap.end() && it->second != NULL)
		{
			llinfos << "Matching font exists: " << nearest_exact_desc.getName()
					<< " - size: " << nearest_exact_desc.getSize()
					<< " - style: " << (S32)nearest_exact_desc.getStyle()
					<< llendl;
			// Copying underlying Freetype font, and storing in LLFontGL with
			// requested font descriptor
			LLFontGL* font = new LLFontGL;
			font->mFontDescriptor = desc;
			font->mFontFreetype = it->second->mFontFreetype;
			mFontMap[desc] = font;

			return font;
		}
	}
	else
	{
		// No template found in our custom fonts.xml file, which does not mean
		// we cannot find a matching font file name on the system, so do not
		// bail out just yet at this point !  HB
		llinfos << "No template font found in fonts.xml for "
				<< norm_desc.getName() << " - style = "
				<< (S32)norm_desc.getStyle() << llendl;
	}

	// Build list of font names to look for.
	string_vec_t file_names;
	if (match_desc)
	{
		// Files specified for this font come first.
		file_names = match_desc->getFileNames();

		// Add the default font as a fallback.
		string_vec_t default_file_names;
		LLFontDescriptor default_desc("default", s_template_string);
		const LLFontDescriptor* match_default_desc =
			getMatchingFontDesc(default_desc);
		if (match_default_desc)
		{
			file_names.insert(file_names.end(),
							  match_default_desc->getFileNames().begin(),
							  match_default_desc->getFileNames().end());
		}

		// Add ultimate fallback list, generated dynamically on Linux, null
		// elsewhere.
		file_names.insert(file_names.end(), mUltimateFallbackList.begin(),
						  mUltimateFallbackList.end());
	}
	else
	{
		// Try to find a matching True Type font file name on the system. HB
		const std::string& fname = desc.getName();
		file_names.emplace_back(fname + ".ttf");
		file_names.emplace_back(fname + ".otf");
		file_names.emplace_back(fname + ".ttc");
		file_names.emplace_back(fname + ".otc");
#if !LL_WINDOWS
		// Linux and macOS file systems are case-sensitive...
		file_names.emplace_back(fname + ".TTF");
		file_names.emplace_back(fname + ".OTF");
		file_names.emplace_back(fname + ".TTC");
		file_names.emplace_back(fname + ".OTC");
#endif
		const std::string& nfname = norm_desc.getName();
		if (nfname != fname)
		{
			file_names.emplace_back(nfname + ".ttf");
			file_names.emplace_back(nfname + ".otf");
			file_names.emplace_back(nfname + ".ttc");
			file_names.emplace_back(nfname + ".otc");
#if !LL_WINDOWS
			// Linux and macOS file systems are case-sensitive...
			file_names.emplace_back(nfname + ".TTF");
			file_names.emplace_back(nfname + ".OTF");
			file_names.emplace_back(nfname + ".TTC");
			file_names.emplace_back(nfname + ".OTC");
#endif
		}
	}

	// Load fonts based on names.
	if (file_names.empty())
	{
		llwarns << "Failure: no file name specified." << llendl;
		return NULL;
	}

	LLFontFreetype::font_vector_t fontlist;
	LLFontGL* result = NULL;

	// Snarf all fonts we can into fontlist. First will get pulled off the list
	// and become the "head" font, set to non-fallback. The rest will
	// constitute the fallback list.
	bool is_first_found = true;

	// Directories to search for fonts
	std::vector<std::string> font_paths;

	// First, our viewer installation path
	font_paths.emplace_back(gDirUtilp->getAppRODataDir() + "/fonts/");

	// Then OS-specific pathes
#if LL_DARWIN
	font_paths.emplace_back("/System/Library/Fonts/");
	font_paths.emplace_back("/Library/Fonts/");
	font_paths.emplace_back("/Library/Fonts/Supplemental/");
	font_paths.emplace_back("/System/Library/Fonts/Supplemental/");
#elif LL_LINUX
	if (match_desc)
	{
		// Under Linux, file_names already contain absolute paths of fallback
		// fonts, so add an empty path so we can find them...
		font_paths.emplace_back("");
	}
	else	// Try and find a matching font file name among system fonts... HB
	{
		// Make a list of unique and valid font paths.
		std::set<std::string> linux_paths;
		for (U32 i = 0, count = mUltimateFallbackList.size(); i < count; ++i)
		{
			const std::string& path = mUltimateFallbackList[i];
			size_t j = path.rfind('/');
			if (j != std::string::npos)
			{
				linux_paths.emplace(path.substr(0, j + 1));
			}
		}
		// Add to the list of possible paths to scan for.
		for (std::set<std::string>::iterator it = linux_paths.begin(),
											 end = linux_paths.end();
			 it != end; ++it)
		{
			font_paths.emplace_back(*it);
		}
	}
#elif LL_WINDOWS
	// Try to figure out where the system's font files are stored.
	char* system_root = getenv("SystemRoot");
	if (system_root)
	{
		font_paths.emplace_back(llformat("%s/fonts/", system_root));
	}
	else
	{
		llwarns << "SystemRoot not found, attempting to load fonts from default path."
				<< llendl;
		font_paths.emplace_back("/WINDOWS/FONTS/");
	}
#endif

	// The fontname string may contain multiple font file names separated by
	// semicolons. Break it apart and try loading each one, in order.
	std::string font_path;
	for (S32 i = 0, count = file_names.size(); i < count; ++i)
	{
		LLFontGL* fontp = new LLFontGL;
		const std::string& file_name = file_names[i];
		bool is_fallback = !is_first_found || !mCreateGLTextures;
		bool found = false;
		for (S32 j = 0, npaths = font_paths.size(); j < npaths; ++j)
		{
			font_path = font_paths[j] + file_name;
			LL_DEBUGS("FontRegistry") << "Trying: " << font_path << LL_ENDL;
			found = fontp->loadFace(font_path, point_size,
									LLFontGL::sVertDPI, LLFontGL::sHorizDPI, 2,
									is_fallback);
			if (found)
			{
				break;
			}
		}
		if (!found)
		{
			if (match_desc)
			{
				llwarns_once << "Could not load font: " << file_name << llendl;
			}
			else
			{
				LL_DEBUGS("FontRegistry") << "Could not find font: "
										  << file_name << LL_ENDL;
			}
			delete fontp;
			fontp = NULL;
		}
		if (fontp)
		{
			if (is_first_found)
			{
				result = fontp;
				is_first_found = false;
				llinfos << "Found matching font, filename: " << font_path
						<< llendl;
			}
			else
			{
				LL_DEBUGS("FontRegistry") << "Adding: " << font_path
										  << LL_ENDL;
				fontlist.emplace_back(fontp->mFontFreetype);
				delete fontp;
				fontp = NULL;
			}
		}
	}

	if (result && !fontlist.empty())
	{
		result->mFontFreetype->setFallbackFonts(fontlist);
	}

	if (!result && !match_desc)
	{
		llwarns << "Failure: no matching font found for "
				<< norm_desc.getName() << " - style = "
				<< (S32)norm_desc.getStyle() << llendl;
		return NULL;
	}

#if NORM_DESC
	if (match_desc)
	{
		norm_desc.setStyle(match_desc->getStyle());
	}
	if (result)
	{
		llinfos << "Created font " << desc.getName() << " (normalized desc: "
				<< norm_desc.getName() << ")" << llendl;
		result->mFontDescriptor = norm_desc;
	}
	else
	{
		llwarns << "Failure to create font " << desc.getName()
				<< ": unknown reason." << llendl;
	}
	mFontMap[norm_desc] = result;
#else
	if (result)
	{
		llinfos << "Created font " << desc.getName() << llendl;
		result->mFontDescriptor = desc;
	}
	else
	{
		llwarns << "Failure to create font " << desc.getName()
				<< ": unknown reason." << llendl;
	}
	mFontMap[desc] = result;
#endif

	return result;
}

void LLFontRegistry::reset()
{
	for (font_reg_map_t::iterator it = mFontMap.begin(), end = mFontMap.end();
		 it != end; ++it)
	{
		// Reset the corresponding font but preserve the entry.
		if (it->second)
		{
			it->second->reset();
		}
	}
}

void LLFontRegistry::clear()
{
	for (font_reg_map_t::iterator it = mFontMap.begin(), end = mFontMap.end();
		 it != end; ++it)
	{
		LLFontGL* fontp = it->second;
		delete fontp;
	}
	mFontMap.clear();
}

void LLFontRegistry::destroyGL()
{
	for (font_reg_map_t::iterator it = mFontMap.begin(), end = mFontMap.end();
		 it != end; ++it)
	{
		// Reset the corresponding font but preserve the entry.
		if (it->second)
		{
			it->second->destroyGL();
		}
	}
}

LLFontGL* LLFontRegistry::getFont(const LLFontDescriptor& desc, bool normalize)
{
	font_reg_map_t::iterator it;
#if NORM_DESC
	if (normalize)
	{
		it = mFontMap.find(desc.normalize());
	}
	else
#endif
	{
		it = mFontMap.find(desc);
	}

	if (it != mFontMap.end())
	{
		return it->second;
	}

	LLFontGL* fontp = createFont(desc);
	if (fontp)
	{
		// Generate glyphs for ASCII chars to avoid stalls later
		fontp->generateASCIIglyphs();
	}
	else
	{
		llwarns << "Failure with name = " << desc.getName()
				<< " - style = " << ((S32) desc.getStyle())
				<< " - size = " << desc.getSize() << llendl;
	}
	return fontp;
}

const LLFontDescriptor* LLFontRegistry::getMatchingFontDesc(const LLFontDescriptor& desc)
{
	LLFontDescriptor norm_desc = desc.normalize();
	font_reg_map_t::iterator it = mFontMap.find(norm_desc);
	return it != mFontMap.end() ? &(it->first) : NULL;
}

static U32 bitCount(U8 c)
{
	U32 count = 0;
	if (c & 1)		++count;
	if (c & 2)		++count;
	if (c & 4)		++count;
	if (c & 8)		++count;
	if (c & 16)		++count;
	if (c & 32)		++count;
	if (c & 64)		++count;
	if (c & 128)	++count;
	return count;
}

// Find nearest match for the requested descriptor.
const LLFontDescriptor* LLFontRegistry::getClosestFontTemplate(const LLFontDescriptor& desc)
{
	const LLFontDescriptor* exact_match_desc = getMatchingFontDesc(desc);
	if (exact_match_desc)
	{
		return exact_match_desc;
	}

	LLFontDescriptor norm_desc = desc.normalize();

	const LLFontDescriptor* best_match_desc = NULL;
	for (font_reg_map_t::iterator it = mFontMap.begin(), end = mFontMap.end();
		 it != end; ++it)
	{
		const LLFontDescriptor* curr_desc = &(it->first);

		if (!curr_desc->isTemplate() ||		// Ignore if not a template.
			// Ignore if font name is wrong:
			curr_desc->getName() != norm_desc.getName() ||
			// Reject font if it matches any bits we don't want:
			(curr_desc->getStyle() & ~norm_desc.getStyle()))
		{
			continue;
		}

		// Take if it is the first plausible candidate we have found.
		if (!best_match_desc)
		{
			best_match_desc = curr_desc;
			continue;
		}

		// Take if it matches more bits than anything before.
		U8 best_style_match_bits = norm_desc.getStyle() &
								   best_match_desc->getStyle();
		U8 curr_style_match_bits = norm_desc.getStyle() &
								   curr_desc->getStyle();
		if (bitCount(curr_style_match_bits) >
				bitCount(best_style_match_bits) ||
			// Take if Bold is requested and this descriptor matches it.
			(curr_style_match_bits & LLFontGL::BOLD))
		{
			best_match_desc = curr_desc;
		}
	}

	// Nothing matched.
	return best_match_desc;
}

void LLFontRegistry::dump()
{
	llinfos << "LLFontRegistry dump: " << llendl;
	for (font_size_map_t::iterator size_it = mFontSizes.begin(),
								   end = mFontSizes.end();
		 size_it != end; ++size_it)
	{
		llinfos << "Size: " << size_it->first << " => " << size_it->second
				<< llendl;
	}
	for (font_reg_map_t::iterator font_it = mFontMap.begin(),
								  end = mFontMap.end();
		 font_it != end; ++font_it)
	{
		const LLFontDescriptor& desc = font_it->first;
		llinfos << "Font: name = " << desc.getName() << " - style = "
				<< ((S32)desc.getStyle()) << " - size = " << desc.getSize()
				<< " - file names listed below:" << llendl;
		const string_vec_t& filenames = desc.getFileNames();
		for (S32 i = 0, count = filenames.size(); i < count; ++i)
		{
			llinfos << "  file: " << filenames[i] <<llendl;
		}
	}
}
