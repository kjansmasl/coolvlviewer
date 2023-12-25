#!/usr/bin/env python
# @file viewer_manifest.py
# @author Ryan Williams - Lot's of modifications by Henri Beauchamp.
# @brief Description of all installer viewer files, and methods for packaging
#		them into installers for all supported platforms.
#
# $LicenseInfo:firstyear=2006&license=viewergpl$
#
# Copyright (c) 2006-2009, Linden Research, Inc.
# Copyright (c) 2009-2023, Henri Beauchamp.
#
# Second Life Viewer Source Code
# The source code in this file ("Source Code") is provided by Linden Lab
# to you under the terms of the GNU General Public License, version 2.0
# ("GPL"), unless you have obtained a separate licensing agreement
# ("Other License"), formally executed by you and Linden Lab.  Terms of
# the GPL can be found in doc/GPL-license.txt in this distribution, or
# online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
#
# There are special exceptions to the terms and conditions of the GPL as
# it is applied to this Source Code. View the full text of the exception
# in the file doc/FLOSS-exception.txt in this software distribution, or
# online at
# http://secondlifegrid.net/programs/open_source/licensing/flossexception
#
# By copying, modifying or distributing this software, you acknowledge
# that you have read and understood your obligations described above,
# and agree to abide by those obligations.
#
# ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
# WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
# COMPLETENESS OR PERFORMANCE.
# $/LicenseInfo$

from __future__ import print_function

import sys
import os.path
import errno
import re
import tarfile

# Look for scripts/lib/python in all possible parent directories (which
# will result in rootdir pointing at the root of the viewer sources, since
# scripts/ is a subdirectory of that root directory.
# Add the full path for scripts/lib/python to sys.path, so that our custom
# modules are found.
rootdir = os.path.realpath(__file__)
while rootdir != os.path.sep:
	rootdir = os.path.dirname(rootdir)
	dir = os.path.join(rootdir, 'scripts', 'lib', 'python')
	if os.path.isdir(dir):
		if dir not in sys.path:
			sys.path.insert(0, dir)
		break
else:
	print("This script is not inside a valid source tree.", file=sys.stderr)
	sys.exit(1)

if sys.version_info[0] == 3:
	from indra.llmanifest3 import LLManifest, main, proper_windows_path, path_ancestors
else:
	from indra.llmanifest import LLManifest, main, proper_windows_path, path_ancestors

# Where the pre-built libraries and binaries reside:
binreleasedir = os.path.join(rootdir, 'bin', 'release')
libreleasedir = os.path.join(rootdir, 'lib', 'release')
libdebugdir = os.path.join(rootdir, 'lib', 'debug')

class ViewerManifest(LLManifest):
	def construct(self):
		super(ViewerManifest, self).construct()
		self.exclude("*.svn*")
		self.path(src=os.path.join(rootdir, 'doc', 'CoolVLViewerReadme.txt'), dst="CoolVLViewerReadme.txt")
		self.path(src=os.path.join(rootdir, 'doc', 'RestrainedLoveReadme.txt'), dst="RestrainedLoveReadme.txt")

		if self.prefix(src="app_settings"):
			self.path("*.crt")
			self.path("*.ini")
			self.path("*.xml")
			self.path("*.msg")
			self.path("gpu_table.txt")

			# Include the entire shaders directory recursively
			self.path("shaders")
			# ... and the entire windlight directory
			self.path("windlight")
			# ... and the entire meshes directory
			self.path("meshes")
			# ... and the entire dictionaries directory
			self.path("dictionaries")
			self.end_prefix("app_settings")

		if self.prefix(src="character"):
			option = self.custom_option()
			self.path("avatar_lad.xml")
			self.path("attentions.xml")
			if option.find('skeleton2') != -1:
				self.path("avatar_skeleton2.xml", "avatar_skeleton.xml")
			else:
				self.path("avatar_skeleton.xml")
			self.path("avatar_constraint.llsd")
			self.path("*.llm")
			self.path("*.tga")
			# Include the entire anims directory
			self.path("anims")
			self.end_prefix("character")

		# Include our fonts
		if self.prefix(src="fonts"):
			self.path("MtB*.ttf")
			self.path("profont*")
			self.path("Roboto*")
			self.path("MtBdLfRg.ttf")
			self.path("MtBdLfRg.ttf")
			self.path("Readme.txt")
			self.end_prefix("fonts")

		# Skins
		if self.prefix(src="skins"):
				self.path("paths.xml")
				# include pre-decoded sounds if any
				if self.prefix(src="default/sounds"):
					self.path("*.dsf")
					self.end_prefix("default/sounds")
				# include the entire textures directory recursively
				if self.prefix(src="*/textures"):
						self.path("*.tga")
						self.path("*.j2c")
						self.path("*.jpg")
						self.path("*.png")
						self.path("textures.xml")
						self.end_prefix("*/textures")
				self.path("*/xui/*/*.xml")
				self.path("*/*.xml")

				# Local HTML files (e.g. loading screen)
				if self.prefix(src="*/html"):
						self.path("*/*/*.html")
						self.end_prefix("*/html")
				self.end_prefix("skins")

	def buildtype(self):
		return self.args['buildtype']

	def viewer_branding_id(self):
		return self.args['branding_id']

	def installer_prefix(self):
		return "CoolVLViewer_"

	def custom_option(self):
		return self.args['custom']


class WindowsManifest(ViewerManifest):
	def construct(self):
		super(WindowsManifest, self).construct()

		config = self.args['configuration']
		build_dir = self.args['build']
		plugins = os.path.join(build_dir, "..", "media_plugins")

		if self.prefix(src="app_settings"):
			self.path("featuretable_windows.txt", dst="featuretable.txt")
			self.end_prefix("app_settings")

		# Include fallback fonts needed by Windows for special UTF-8 characters
		if self.prefix(src="fonts"):
			self.path("DejaVu*")
			self.end_prefix("fonts")

		if self.prefix(src=config, dst=""):
			self.path("CoolVLViewer.exe")
			# The config file name needs to match the exe's name.
			#self.path("CoolVLViewer.exe.config")
			self.end_prefix()

		if self.prefix(src=libreleasedir, dst=""):
			# OpenSSL libraries (either v1.0 or v1.1, which are named differently)
			self.path("libeay32.dll")
			self.path("ssleay32.dll")
			self.path("libssl-1_1-x64.dll")
			self.path("libcrypto-1_1-x64.dll")

			# HTTP/2
			self.path("nghttp2.dll")

			# Mesh 3rd party lib
			self.path("glod.dll")

			# Lua
			self.path("lua54.dll")

			# For spellchecking
			self.path("libhunspell.dll")

			self.end_prefix()

		# Media plugins
		has_cef = 0
		if self.prefix(src=os.path.join(plugins, "cef", config), dst="llplugin"):
			if (self.path("media_plugin_cef.dll")):
				has_cef = 1
			self.end_prefix()
		if self.prefix(src=os.path.join(plugins, "gstreamer", config), dst="llplugin"):
			self.path("media_plugin_gstreamer.dll")
			self.end_prefix()
		# Media plugins launcher
		if self.prefix(src=os.path.join(plugins, "slplugin", config), dst=""):
			self.path("SLPlugin.exe")
			self.end_prefix()

		# For CEF plugin runtimes
		if has_cef == 1:
			if self.prefix(src=binreleasedir, dst="llplugin"):
				self.path("dullahan_host.exe")
				self.path("chrome_elf.dll")
				self.path("libcef.dll")
				self.path("libEGL.dll")
				self.path("libGLESv2.dll")
				self.path("d3dcompiler*.dll")
				self.path("*snapshot.bin")
				self.path("*_blob.bin")
				self.end_prefix()
			if self.prefix(src=os.path.join(binreleasedir, 'swiftshader'), dst=os.path.join('llplugin', 'swiftshader')):
				self.path("libEGL.dll")
				self.path("libGLESv2.dll")
				self.end_prefix()
			# CEF plugin resources
			cefresources = os.path.join(rootdir, 'Resources')
			if self.prefix(src=cefresources, dst="llplugin"):
				# CEF 89 and older: copy all but devtools_resources.pak
				self.path("cef*.pak")
				# CEF 90 and newer:
				self.path("resources.pak")
				self.path("chrome_*.pak")
				# Common to all versions
				self.path("icudtl.dat")
				self.end_prefix()
			# CEF plugin locales
			if self.prefix(src=os.path.join(cefresources, 'locales'), dst=os.path.join('llplugin', 'locales')):
				self.path("*.pak")
				self.end_prefix()

		# SLVoice client
		if self.prefix(src=binreleasedir, dst=""):
			self.path("SLVoice.exe")
			self.end_prefix()
		# Vivox libraries
		if self.prefix(src=libreleasedir, dst=""):
			self.path("ortp_x64.dll")
			self.path("vivoxsdk_x64.dll")
			self.end_prefix()

		# Tracy, if in use
		has_tracy = 0
		if self.prefix(src=binreleasedir, dst=""):
			if (self.path("Tracy.exe")):
				# Change for this if using Tracy v0.9.1 or newer. HB
				#has_tracy = 1
				has_tracy = 0
			self.end_prefix()

		self.path(src=os.path.join(rootdir, 'doc', 'licenses-windows.txt'), dst='licenses.txt')
		self.path(src=os.path.join(rootdir, 'doc', 'VivoxAUP.txt'), dst='VivoxAUP.txt')

		# For using FMOD for sound...
		if (not self.path("fmod.dll")):
			if (not self.path("fmod64.dll")):
				print("FMOD library not found: the viewer will lack FMOD support.")

		# mimalloc overrides, if available
		has_mimalloc = 0
		if self.prefix(src=libreleasedir, dst=""):
			if (self.path("mimalloc-override.dll")):
				has_mimalloc = 1
				self.path("mimalloc-redirect.dll")
			else:
				print("Skipping mimalloc - not found (normal if built without it).")
			self.end_prefix()
		if has_mimalloc == 1 and self.prefix(src=libreleasedir, dst="llplugin"):
			self.path("mimalloc-override.dll")
			self.path("mimalloc-redirect.dll")
			self.end_prefix()

		runtime_libs = os.path.join(rootdir, 'lib', 'vc143_x64')
		if self.prefix(src=runtime_libs, dst=""):
			# For use in crash reporting (generates minidumps).
			# Actually, including dbghelp.dll is a bad idea, since it should be
			# present in any modern Windows system, and including a newer
			# version causes issues with missing DLL dependencies (such as
			# "api-ms-win-core-com-l1-1-0.dll", but not only). HB
			#self.path("dbghelp.dll")
			#self.path("api-ms-win-core-com-l1-1-0.dll")
			# VC141 runtimes
			#self.path("concrt140.dll")
			#self.path("msvcp140*.dll")
			#self.path("vccorlib140.dll")
			self.path("msvcp140.dll")
			self.path("vcruntime140*.dll")
			option = self.custom_option()
			if option.find('openmp') != -1:
				self.path("vcomp140.dll")
			if (has_tracy == 1):
				# Needed, starting with Tracy v0.9.1. HB
				self.path("msvcp140_atomic_wait.dll")
			self.end_prefix()
		# Needed by the CEF plugin wrappers
		if self.prefix(src=runtime_libs, dst="llplugin"):
			# See above about dbghelp.dll and api-ms-win-core-com-l1-1-0.dll
			#self.path("api-ms-win-core-com-l1-1-0.dll")
			#self.path("concrt140.dll")
			#self.path("msvcp140*.dll")
			#self.path("vccorlib140.dll")
			self.path("msvcp140.dll")
			self.path("vcruntime140*.dll")
			self.end_prefix()

	def package_finish(self):
		self.package_file = 'touched.bat'
		# removed: we use InstallJammer to make packages
		return


class DarwinManifest(ViewerManifest):
	def construct(self):

		# copy over the build result (this is a no-op if run within the xcode script)
		self.path(self.args['configuration'] + "/" + self.app_name() + ".app", dst="")

		# jemalloc presence flag
		has_jemalloc = 0
		if self.prefix(src="", dst="Contents"):  # everything goes in Contents
			self.path(self.info_plist_name(), dst="Info.plist")

			# Copy jemalloc, if in use
			if (self.path(os.path.join(libreleasedir, 'libjemalloc.2.dylib'), dst='Resources/libjemalloc.2.dylib')):
				has_jemalloc = 1
			else:
				print("Skipping libjemalloc.dylib - not found (normal if built without it or statically linked).")

			# Copy additional libs in <bundle>/Contents/MacOS/ and <bundle>/Contents/Resources/
			self.path(os.path.join(libreleasedir, 'libhunspell-1.3.0.dylib'), dst='Resources/libhunspell-1.3.0.dylib')
			self.path(os.path.join(libreleasedir, 'libndofdev.dylib'), dst='Resources/libndofdev.dylib')

			# Copy macOS feature table into Resources/app_settings
			self.path(os.path.join(rootdir, 'indra', 'newview', 'app_settings', 'featuretable_macos.txt'), dst='Resources/app_settings/featuretable.txt')

			# Most everything goes in the Resources directory
			if self.prefix(src="", dst="Resources"):
				super(DarwinManifest, self).construct()

				if self.prefix("cursors_mac"):
					self.path("*.tif")
					self.end_prefix("cursors_mac")

				self.path(src=os.path.join(rootdir, 'doc', 'licenses-mac.txt'), dst='licenses.txt')
				self.path(src=os.path.join(rootdir, 'doc', 'VivoxAUP.txt'), dst='VivoxAUP.txt')
				self.path("CoolVLViewer.nib")
				self.path("cool_vl_viewer.icns")

				# Translations
				self.path("English.lproj")

				# SLVoice and Vivox libs
				if self.prefix(src=binreleasedir, dst=""):
					self.path("SLVoice")
					self.end_prefix()
				if self.prefix(src=libreleasedir, dst=""):
					self.path("libortp.dylib")
					self.path("libvivoxsdk.dylib")
					self.end_prefix()

				# Tracy, if in use
				if self.prefix(src=binreleasedir, dst=""):
					self.path("tracy")
					self.end_prefix()

				for libfile in ("libapr-1.0.dylib",
								"libexpat.1*.dylib",
								"libglod.dylib",
								"libnghttp2.*dylib",
								"libfmod*.dylib"):
					self.path(os.path.join(libreleasedir, libfile), libfile)

				# Plugin launcher
				self.path("../media_plugins/slplugin/" + self.args['configuration'] + "/SLPlugin.app", "SLPlugin.app")

				self.end_prefix("Resources")

			self.end_prefix("Contents")

		# Media plugins presence flags
		has_gstreamer = 0
		has_cef = 0
		if self.prefix(src="", dst="Contents"):  # everything goes in Contents
			# most everything goes in the Resources directory
			if self.prefix(src="", dst="Resources"):

				# Media plugins
				if self.prefix(src="", dst="llplugin"):
					if self.path("../media_plugins/gstreamer/" + self.args['configuration'] + "/media_plugin_gstreamer.dylib", "media_plugin_gstreamer.dylib"):
						has_gstreamer = 1
					if self.path("../media_plugins/cef/" + self.args['configuration'] + "/media_plugin_cef.dylib", "media_plugin_cef.dylib"):
						has_cef = 1
					self.end_prefix("llplugin")

				# Dullahan helper app go inside SLPlugin.app
				if has_cef == 1 and self.prefix(src="", dst="SLPlugin.app/Contents/Frameworks"):
					self.path2basename(libreleasedir, 'DullahanHelper.app')
					pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

					if self.prefix(dst=os.path.join(
						'DullahanHelper.app', 'Contents', "Frameworks")):
						self.cmakedirs(self.get_dst_prefix());
						dullahanframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

						self.end_prefix()

					self.end_prefix()

                # Dullahan helper (GPU) app go inside SLPlugin.app
				if has_cef == 1 and self.prefix(src="", dst="SLPlugin.app/Contents/Frameworks"):
					self.path2basename(libreleasedir, 'DullahanHelper (GPU).app')
					pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

					if self.prefix(dst=os.path.join(
						'DullahanHelper (GPU).app', 'Contents', "Frameworks")):
						self.cmakedirs(self.get_dst_prefix());
						dullahanframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

						self.end_prefix()

					self.end_prefix()

                # Dullahan helper (Plugin) app go inside SLPlugin.app
				if has_cef == 1 and self.prefix(src="", dst="SLPlugin.app/Contents/Frameworks"):
					self.path2basename(libreleasedir, 'DullahanHelper (Plugin).app')
					pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

					if self.prefix(dst=os.path.join(
						'DullahanHelper (Plugin).app', 'Contents', "Frameworks")):
						self.cmakedirs(self.get_dst_prefix());
						dullahanframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

						self.end_prefix()

					self.end_prefix()

                # Dullahan helper (Renderer) app go inside SLPlugin.app
				if has_cef == 1 and self.prefix(src="", dst="SLPlugin.app/Contents/Frameworks"):
					self.path2basename(libreleasedir, 'DullahanHelper (Renderer).app')
					pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

					if self.prefix(dst=os.path.join(
						'DullahanHelper (Renderer).app', 'Contents', "Frameworks")):
						self.cmakedirs(self.get_dst_prefix());
						dullahanframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');

						self.end_prefix()

					self.end_prefix()

				self.end_prefix("Resources")

				# CEF framework goes inside Cool VL Viewer.app/Contents/Frameworks
				if has_cef == 1:
					if self.prefix(src="", dst="Frameworks"):
						frameworkfile="Chromium Embedded Framework.framework"
						self.path2basename(libreleasedir, frameworkfile)
						self.end_prefix("Frameworks")

					# This code constructs a relative path from the
					# target framework folder back to the location of the symlink.
					# It needs to be relative so that the symlink still works when
					# (as is normal) the user moves the app bunlde out of the DMG
					# and into the /Applications folder. Note we also call 'raise'
					# to terminate the process if we get an error since without
					# this symlink, Second Life web media can't possibly work.
					# Real Framework folder:
					#   Cool VL Viewer.app/Contents/Frameworks/Chromium Embedded Framework.framework/
					# Location of symlink and why it is relative
					#   Cool VL Viewer.app/Contents/Resources/SLPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework/
					frameworkpath = os.path.join(os.pardir, os.pardir, os.pardir, os.pardir, "Frameworks", "Chromium Embedded Framework.framework")
					try:
						symlinkf(frameworkpath, pluginframeworkpath)
					except OSError as err:
						print("Cannot symlink %s -> %s: %s" % (frameworkpath, pluginframeworkpath, err))
						raise

					try:
						symlinkf(frameworkpath, dullahanframeworkpath)
					except OSError as err:
						print("Cannot symlink %s -> %s: %s" % (frameworkpath, dullahanframeworkpath, err))
						raise

			self.end_prefix("Contents")

		# Fix library paths in plugins:
		self.run_command('install_name_tool -change @executable_path/../Resources/libexpat.1.dylib @executable_path/../../../libexpat.1.dylib "%(slplugin)s"' %
			{ 'slplugin': self.dst_path_of('Contents/Resources/SLPlugin.app/Contents/MacOS/SLPlugin')})
		self.run_command('install_name_tool -change @executable_path/../Resources/libnghttp2.14.14.0.dylib @executable_path/../../../libnghttp2.14.14.0.dylib "%(slplugin)s"' %
			{ 'slplugin': self.dst_path_of('Contents/Resources/SLPlugin.app/Contents/MacOS/SLPlugin')})
		if has_gstreamer == 1:
			self.run_command('install_name_tool -change @executable_path/../Resources/libexpat.1.dylib @executable_path/../../../libexpat.1.dylib "%(plugin)s"' %
				{ 'plugin': self.dst_path_of('Contents/Resources/llplugin/media_plugin_gstreamer.dylib')})
		if has_cef == 1:
			# fix up media_plugin.dylib so it knows where to look for CEF files it needs
			self.run_command('install_name_tool -change "@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "../Cool VL Viewer.app/Contents/Resources/llplugin/media_plugin_cef.dylib"'  %
				{ 'config' : self.args['configuration'] })
			self.run_command('install_name_tool -change @executable_path/../Resources/libexpat.1.dylib @executable_path/../../../libexpat.1.dylib "%(plugin)s"' %
				{ 'plugin': self.dst_path_of('Contents/Resources/llplugin/media_plugin_cef.dylib')})

		# Patch libjemalloc.dylib path in image
		if has_jemalloc == 1:
			self.run_command('install_name_tool -change libjemalloc.2.dylib @executable_path/../Resources/libjemalloc.2.dylib "%(viewer_binary)s"' %
				{ 'viewer_binary': self.dst_path_of('Contents/MacOS/'+self.app_name())})
			self.run_command('install_name_tool -change libjemalloc.2.dylib @executable_path/../../../libjemalloc.2.dylib "%(slplugin)s"' %
				{ 'slplugin': self.dst_path_of('Contents/Resources/SLPlugin.app/Contents/MacOS/SLPlugin')})
			if has_gstreamer == 1:
				self.run_command('install_name_tool -change libjemalloc.2.dylib @executable_path/../../../libjemalloc.2.dylib "%(plugin)s"' %
					{ 'plugin': self.dst_path_of('Contents/Resources/llplugin/media_plugin_gstreamer.dylib')})
			if has_cef == 1:
				self.run_command('install_name_tool -change libjemalloc.2.dylib @executable_path/../../../libjemalloc.2.dylib "%(plugin)s"' %
					{ 'plugin': self.dst_path_of('Contents/Resources/llplugin/media_plugin_cef.dylib')})

		# NOTE: the -S argument to strip causes it to keep enough info for
		# annotated backtraces (i.e. function names in the crash log).  'strip' with no
		# arguments yields a slightly smaller binary but makes crash logs mostly useless.
		# This may be desirable for the final release.  Or not.
		if self.buildtype().lower() == 'release':
			if ("package" in self.args['actions'] or
				"unpacked" in self.args['actions']):
				self.run_command('strip -S "%(viewer_binary)s"' %
								 { 'viewer_binary' : self.dst_path_of('Contents/MacOS/'+self.app_name())})

		# patch libfmod.dylib path in image
		self.run_command('install_name_tool -change @rpath/libfmod.dylib @executable_path/../Resources/libfmod.dylib "%(viewer_binary)s"' %
				{ 'viewer_binary': self.dst_path_of('Contents/MacOS/'+self.app_name())})

	def app_name(self):
		return "Cool VL Viewer"

	def info_plist_name(self):
		return "Info-CoolVLViewer.plist"

	def package_finish(self):
		# removed, not using this anymore
		pass


class LinuxManifest(ViewerManifest):
	def construct(self):
		super(LinuxManifest, self).construct()

		notdebug = "rel" in self.buildtype().lower()
		release = "release" in self.buildtype().lower()

		# For the Release build type (which lacks the symbols in the viewer
		# binary), save the build directory path into .buildir, so that our
		# wrapper script can find the path of the CoolVLViewer.debug file, when
		# it is still around...
		if release:
			# Save the build directory path into .buildir, so that our wrapper
			# script can find the path of the CoolVLViewer.debug file, when it
			# is still around...
			builddir_file = os.path.join(self.get_build_prefix(), '.buildir')
			ofile = open(builddir_file, "w")
			ofile.write(self.get_build_prefix())
			ofile.close()
			self.path(builddir_file, '.buildir')

		# Features table
		if self.prefix(src="app_settings"):
			self.path("featuretable_linux.txt", dst="featuretable.txt")
			self.end_prefix("app_settings")

		# Licenses and README files
		self.path(os.path.join(rootdir, 'doc', 'licenses-linux.txt'), "licenses.txt")
		self.path(os.path.join(rootdir, 'doc', 'README-linux.txt'), "README-linux.txt")
		self.path(os.path.join(rootdir, 'doc', 'VivoxAUP.txt'), 'VivoxAUP.txt')

		# Icons
		if self.prefix("res-sdl"):
			self.path("*")
			self.end_prefix("res-sdl")
			# Create a relative symlink for icon_name(); sadly, os.symlink()
			# cannot create relative links, so we call 'ln' instead.
			os.system("cd " + self.get_dst_prefix() + ";ln -s ./res-sdl/" + self.icon_name("48") + " " + self.icon_name())
		else:
			self.path("res/" + self.icon_name(), self.icon_name())

		# Various wrapper and utility scripts
		if self.prefix("linux_tools", dst=""):
			self.path("wrapper.sh", self.wrapper_name())
			self.path("viewer.conf", self.conf_name())
			self.path("handle_secondlifeprotocol.sh", "bin/handle_secondlifeprotocol.sh")
			self.path("register_secondlifeprotocol.sh", "bin/register_secondlifeprotocol.sh")
			self.path("messagebox.sh", "bin/messagebox.sh")
			self.path("launch_url.sh", "bin/launch_url.sh")
			self.path("dbuslua.tk")
			self.path("install-wine-SLVoice.sh")
			self.end_prefix("linux_tools")

		# Viewer binary, stripped or not, depending on build type
		# Note: for both the Debug and RelWithDebInfo types, we want the
		# unstripped binary.
		if release:
			self.path("CoolVLViewer-stripped", "bin/" + self.binary_name())
		else:
			self.path("CoolVLViewer", "bin/" + self.binary_name())

		# Other libraries
		if self.prefix(libreleasedir, dst="lib"):
			self.path("libapr-1.so.0.*.*", "libapr-1.so.0")
			# IMPORTANT: *never* bundle libfontconfig.so.1 with the viewer (and
			# never link the latter with a static libfontconfig.a library): the
			# version in use on the user's system might not correspond to the
			# one we use to build the viewer and the /etc/fonts/ config files
			# might not be understood by the latter. Instead, let's use the
			# system library, which matches the /etc/fonts/ config files.
			#self.path("libfontconfig.so.1.12.0", "libfontconfig.so.1")
			# This should exist on all Linux distros (part of util-linux).
			#self.path("libuuid.so.1.3.0", "libuuid.so.1")
			self.path("libalut.so.0.0.0", "libalut.so.0")
			# SDL2
			self.path("libSDL2-2.0.so.0.*.*", "libSDL2-2.0.so.0")
			# This too seemed like a good idea, but it causes issues on some
			# systems...
			#self.path("libopenal.so.1.12.854", "libopenal.so.1")
			self.path("libglod.so") # Mesh support
			self.end_prefix("lib")

		# Boost libraries, when dynamically linked. Only the Debug build uses
		# their debug version.
		if (notdebug and self.prefix(libreleasedir, dst="lib")):
			if (self.path("libboost_context-mt*.so.1.*.0")):
				self.path("libboost_fiber-mt*.so.1.*.0")
				self.path("libboost_filesystem-mt*.so.1.*.0")
				self.path("libboost_program_options-mt*.so.1.*.0")
				self.path("libboost_thread-mt*.so.1.*.0")
				self.path("libboost_atomic-mt*.so.1.*.0")
			else:
				print("Skipping boost libraries: assumed statically linked.")
			self.end_prefix("lib")
		if (not notdebug and self.prefix(libdebugdir, dst="lib")):
			if (self.path("libboost_context*.so.1.*.0")):
				self.path("libboost_fiber-mt*.so.1.*.0")
				self.path("libboost_filesystem-mt*.so.1.*.0")
				self.path("libboost_program_options-mt*.so.1.*.0")
				self.path("libboost_thread-mt*.so.1.*.0")
				self.path("libboost_atomic-mt*.so.1.*.0")
			else:
				print("Skipping boost libraries: assumed statically linked.")
			self.end_prefix("lib")

		# Shared jemalloc library, maybe...
		no_jemalloc = 1
		if (not notdebug and self.prefix(libdebugdir, dst="lib")):
			if (self.path("libjemalloc.so.2")):
				no_jemalloc = 0
			self.end_prefix("lib")
		if (no_jemalloc and self.prefix(libreleasedir, dst="lib")):
			self.path("libjemalloc.so.2")
			self.end_prefix("lib")

		# Media plugins
		has_cef = 0
		if self.prefix(src="", dst="bin/llplugin"):
			if (self.path("../media_plugins/cef/media_plugin_cef.so", "media_plugin_cef.so")):
				has_cef = 1
			self.path("../media_plugins/gstreamer/media_plugin_gstreamer.so", "media_plugin_gstreamer.so")
			# SLPlugin
			self.path("../media_plugins/slplugin/SLPlugin", "SLPlugin")
			self.end_prefix("bin/llplugin")

		# For CEF3 plugin runtimes (starting with CEF 73, everything goes in
		# lib/, dullahan_host excepted).
		if has_cef == 1:
			if self.prefix(src=libreleasedir, dst="lib"):
				self.path("libcef.so")
				self.path("libEGL.so")
				self.path("libGLESv2.so")
				self.path("libvk_swiftshader.so")
				self.path("*blob.bin")
				self.path("*snapshot.bin")
				self.path("*.pak")
				self.path("icudtl.dat")
				self.end_prefix("lib")
			if self.prefix(src=os.path.join(libreleasedir, 'swiftshader'), dst="lib/swiftshader"):
				self.path("*.so")
				self.end_prefix("lib/swiftshader")
			# The locales/ directory now goes along with the libcef library,
			if self.prefix(src=os.path.join(libreleasedir, 'locales'), dst="lib/locales"):
				self.path("*.pak")
				self.end_prefix("lib/locales")
			if self.prefix(src=binreleasedir, dst="bin/llplugin"):
				self.path("dullahan_host")
				self.end_prefix("bin/llplugin")

		# 64 bits FMOD library:
		if (self.prefix(libreleasedir, dst="lib")):
			if (not self.path("libfmod.so.13.*", "libfmod.so.13")):
				print("FMOD library not found: the viewer will lack FMOD support.")
			self.end_prefix("lib")

		# Vivox runtime (they are 32 bits program and libraries)
		if self.prefix(src=binreleasedir, dst="bin"):
			self.path("SLVoice")
			self.end_prefix("bin")
		if self.prefix(src=libreleasedir, dst="lib"):
			self.path("libortp.so")
			self.path("libsndfile.so.1")
			self.path("libvivoxoal.so.1")
			self.path("libvivoxplatform.so")
			self.path("libvivoxsdk.so")
			self.end_prefix("lib")

		# Tracy, if in use
		if self.prefix(src=binreleasedir, dst="bin"):
			self.path("tracy")
			self.end_prefix()

	def wrapper_name(self):
		return "cool_vl_viewer"

	def conf_name(self):
		return "cool_vl_viewer.conf"

	def binary_name(self):
		return "cool_vl_viewer-bin"

	def icon_name(self, size = ""):
		return "cvlv_icon" + size + ".png"

	def package_finish(self):
		if 'installer_name' in self.args:
			installer_name = self.args['installer_name']
		else:
			installer_name_components = [self.installer_prefix(), 'x86_64']
			installer_name_components.extend(self.args['version'])
			installer_name = "_".join(installer_name_components)

		# Fix access permissions
		self.run_command("""
			find '%(dst)s' -type d -print0 | xargs -0 --no-run-if-empty chmod 755;
			find '%(dst)s' -type f -perm 0700 -print0 | xargs -0 --no-run-if-empty chmod 0755;
			find '%(dst)s' -type f -perm 0500 -print0 | xargs -0 --no-run-if-empty chmod 0555;
			find '%(dst)s' -type f -perm 0600 -print0 | xargs -0 --no-run-if-empty chmod 0644;
			find '%(dst)s' -type f -perm 0400 -print0 | xargs -0 --no-run-if-empty chmod 0444;
			true""" %  {'dst':self.get_dst_prefix() })

		self.package_file = installer_name + '.tar.bz2'

		# Temporarily move directory tree so that it has the right name in the
		# tarfile
		self.run_command("rm -rf '%(inst)s/'" % {
			'inst': self.build_path_of(installer_name)})
		self.run_command("mv '%(dst)s' '%(inst)s'" % {
			'dst': self.get_dst_prefix(),
			'inst': self.build_path_of(installer_name)})

		if "release" in self.buildtype().lower():
			# Strip the shared libraries from their debug symbols to save space
			# and speed up loading
			self.run_command("strip -s %(libs)s" % {
				'libs': self.build_path_of(installer_name + "/lib/*.so*")})

		# --numeric-owner hides the username of the builder for security etc.
		if (self.run_command("tar -C '%(dir)s' --numeric-owner -cjf "
							 "'%(inst_path)s.tar.bz2' %(inst_name)s" % {
				'dir': self.get_build_prefix(),
				'inst_name': installer_name,
				'inst_path':self.build_path_of(installer_name)})):
			self.run_command("mv '%(inst)s' '%(dst)s'" % {
				'dst': self.get_dst_prefix(),
				'inst': self.build_path_of(installer_name)})

################################################################

def symlinkf(src, dst):
	"""
	Like ln -sf, but uses os.symlink() instead of running ln.
	"""
	try:
		os.symlink(src, dst)
	except OSError as err:
		if err.errno != errno.EEXIST:
			raise
		# We could just blithely attempt to remove and recreate the target
		# file, but that strategy doesn't work so well if we do not have
		# permissions to remove it. Check to see if it's already the
		# symlink we want, which is the usual reason for EEXIST.
		if not (os.path.islink(dst) and os.readlink(dst) == src):
			# Here either dst is not a symlink or it is the wrong symlink.
			# Remove and recreate. Caller will just have to deal with any
			# exceptions at this stage.
			os.remove(dst)
			os.symlink(src, dst)

if __name__ == "__main__":
	main()
