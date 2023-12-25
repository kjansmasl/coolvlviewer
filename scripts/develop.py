#!/usr/bin/env python
#
# @file develop.py
# @authors Bryan O'Sullivan, Mark Palange, Aaron Brashears
# @brief Fire and forget script to appropriately configure cmake for SL.
#
# $LicenseInfo:firstyear=2007&license=viewergpl$
#
# Copyright (c) 2007-2009, Linden Research, Inc.
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

import errno
import getopt
import os
import random
import re
import shutil
import socket
import sys
if sys.version_info[0] == 3:
	import subprocess as cmds
else:
	import commands as cmds

# Force English on cmake and compiler messages (easier to search on the Web for
# the error messages that could arise).
os.environ['LANG'] = 'C'

if os.path.isdir('indra'):
	os.chdir('indra')
elif os.path.isdir(os.path.join('..', 'indra')):
	os.chdir(os.path.join('..', 'indra'))
elif not os.path.isdir('newview'):
	print('Error: cannot find the "indra" sub-directory', file=sys.stderr)
	sys.exit(1)

class CommandError(Exception):
	pass

def mkdir(path):
	try:
		os.mkdir(path)
		return path
	except OSError as err:
		if err.errno != errno.EEXIST or not os.path.isdir(path):
			raise

def getcwd():
	cwd = os.getcwd()
	if 'a' <= cwd[0] <= 'z' and cwd[1] == ':':
		# CMake wants DOS drive letters to be in uppercase.  The above
		# condition never asserts on platforms whose full path names
		# always begin with a slash, so we do not need to test whether
		# we are running on Windows.
		cwd = cwd[0].upper() + cwd[1:]
	return cwd

def quote(opts):
	return '"' + '" "'.join([ opt.replace('"', '') for opt in opts ]) + '"'

class PlatformSetup(object):
	generator = None
	build_types = {}
	for t in ('Debug', 'Release', 'RelWithDebInfo'):
		build_types[t.lower()] = t

	build_type = build_types['release']
	systemlibs = 'OFF'
	project_name = 'CoolVLViewer'
	distcc = True
	cmake_opts = []

	def __init__(self):
		self.script_dir = os.path.realpath(
			os.path.dirname(__import__(__name__).__file__))

	def os(self):
		'''Return the name of the OS.'''

		raise NotImplemented('os')

	def arch(self):
		'''Return the CPU architecture.'''

		return None

	def platform(self):
		'''Return a stringified two-tuple of the OS name and CPU
		architecture.'''

		ret = self.os()
		if self.arch():
			ret += '-' + self.arch()
		return ret

	def build_dirs(self):
		'''Return the top-level directories in which builds occur.'''
		return [os.path.join('..', 'build-' + self.platform())]

	def cmake_commandline(self, src_dir, build_dir, opts):
		'''Return the command line to run cmake with.'''

		args = dict(
			dir=src_dir,
			generator=self.generator,
			opts=quote(opts) if opts else '',
			systemlibs=self.systemlibs,
			type=self.build_type.upper(),
			)
		return ('cmake -DCMAKE_BUILD_TYPE:STRING=%(type)s '
				'-DUSESYSTEMLIBS:BOOL=%(systemlibs)s '
				'-G %(generator)r %(opts)s %(dir)r' % args)

	def run_cmake(self, args=[]):
		'''Run cmake.'''

		# do a sanity check to make sure we have a generator
		if not hasattr(self, 'generator'):
			raise "No generator available for '%s'" % (self.__name__,)
		cwd = getcwd()
		created = []
		try:
			for d in self.build_dirs():
				if mkdir(d):
					created.append(d)
				try:
					os.chdir(d)
					cmd = self.cmake_commandline(cwd, d, args)
					print('Running %r in %r' % (cmd, d))
					self.run(cmd, 'cmake')
				finally:
					os.chdir(cwd)
		except:
			# If we created a directory in which to run cmake and
			# something went wrong, the directory probably just
			# contains garbage, so delete it.
			os.chdir(cwd)
			for d in created:
				print('Cleaning %r' % d)
				shutil.rmtree(d)
			raise

	def parse_build_opts(self, arguments):
		opts, targets = getopt.getopt(arguments, 'o:', ['option='])
		build_opts = []
		for o, a in opts:
			if o in ('-o', '--option'):
				build_opts.append(a)
		return build_opts, targets

	def run_build(self, opts, targets):
		'''Build the default targets for this platform.'''

		raise NotImplemented('run_build')

	def cleanup(self):
		'''Delete all build directories.'''

		cleaned = 0
		for d in self.build_dirs():
			if os.path.isdir(d):
				print('Cleaning %r' % d)
				shutil.rmtree(d)
				cleaned += 1
		if not cleaned:
			print('Nothing to clean up!')

	def find_in_path(self, name, defval=None, basename=False):
		for ext in self.exe_suffixes:
			name_ext = name + ext
			if os.sep in name_ext:
				path = os.path.abspath(name_ext)
				if os.access(path, os.X_OK):
					return [basename and os.path.basename(path) or path]
			for p in os.getenv('PATH', self.search_path).split(os.pathsep):
				path = os.path.join(p, name_ext)
				if os.access(path, os.X_OK):
					return [basename and os.path.basename(path) or path]
		if defval:
			return [defval]
		return []


class UnixSetup(PlatformSetup):
	'''Generic Unixy build instructions.'''

	search_path = '/usr/bin:/usr/local/bin'
	exe_suffixes = ('',)

	def __init__(self):
		super(UnixSetup, self).__init__()
		self.generator = 'Unix Makefiles'

	def os(self):
		return 'unix'

	def arch(self):
		cpu = os.uname()[-1]
		return cpu

	def run(self, command, name=None):
		'''Run a program. If the program fails, raise an exception.'''
		ret = os.system(command)
		if ret:
			if name is None:
				name = command.split(None, 1)[0]
			if os.WIFEXITED(ret):
				st = os.WEXITSTATUS(ret)
				if st == 127:
					event = 'was not found'
				else:
					event = 'exited with status %d' % st
			elif os.WIFSIGNALED(ret):
				event = 'was killed by signal %d' % os.WTERMSIG(ret)
			else:
				event = 'died unexpectedly (!?) with 16-bit status %d' % ret
			raise CommandError('the command %r %s' %
							   (name, event))


class LinuxSetup(UnixSetup):
	def __init__(self):
		super(LinuxSetup, self).__init__()

	def os(self):
		return 'linux'

	def build_dirs(self):
		platform_build = '%s-%s' % (self.platform(), self.build_type.lower())
		return [os.path.join('..', 'viewer-' + platform_build)]

	def cmake_commandline(self, src_dir, build_dir, opts):
		args = dict(
			dir=src_dir,
			generator=self.generator,
			opts=quote(opts) if opts else '',
			systemlibs=self.systemlibs,
			type=self.build_type.upper(),
			project_name=self.project_name,
			cxx="g++"
			)
		cmd = (('cmake -DCMAKE_BUILD_TYPE:STRING=%(type)s '
				'-G %(generator)r '
				'-DUSESYSTEMLIBS:BOOL=%(systemlibs)s '
				'-DROOT_PROJECT_NAME:STRING=%(project_name)s '
				'%(opts)s %(dir)r')
			   % args)
		if 'CXX' not in os.environ:
			args.update({'cmd':cmd})
			cmd = ('CXX=%(cxx)r %(cmd)s' % args)
		return cmd

	def run_build(self, opts, targets):
		job_count = None

		for i in range(len(opts)):
			if opts[i].startswith('-j'):
				try:
					job_count = int(opts[i][2:])
				except ValueError:
					try:
						job_count = int(opts[i+1])
					except ValueError:
						job_count = True

		def get_cpu_count():
			count = 0
			for line in open('/proc/cpuinfo'):
				if re.match(r'processor\s*:', line):
					count += 1
			return count

		def localhost():
			count = get_cpu_count()
			return 'localhost/' + str(count), count

		def get_distcc_hosts():
			try:
				hosts = []
				name = os.getenv('DISTCC_DIR', '/etc/distcc') + '/hosts'
				for l in open(name):
					l = l[l.find('#')+1:].strip()
					if l: hosts.append(l)
				return hosts
			except IOError:
				return (os.getenv('DISTCC_HOSTS', '').split() or
						[localhost()[0]])

		def count_distcc_hosts():
			cpus = 0
			hosts = 0
			for host in get_distcc_hosts():
				m = re.match(r'.*/(\d+)', host)
				hosts += 1
				cpus += m and int(m.group(1)) or 1
			return hosts, cpus

		if job_count is None:
			hosts, job_count = count_distcc_hosts()
			'''Saturate the cores (including during I/O mutex waits by'''
			''' increasing the jobs count at 50% above the cores count.'''
			job_count = int((job_count * 3) / 2)
		opts.extend(['-j', str(job_count)])

		if targets:
			targets = ' '.join(targets)
		else:
			targets = 'all'

		for d in self.build_dirs():
			if self.generator == 'Ninja':
				cmd = 'ninja -C %r %s %s' % (d, ' '.join(opts), targets)
			else:
				cmd = 'make -C %r %s %s' % (d, ' '.join(opts), targets)
			print('Running %r' % cmd)
			self.run(cmd)


class DarwinSetup(UnixSetup):
	def __init__(self):
		super(DarwinSetup, self).__init__()
		self.generator = 'Xcode'

	def os(self):
		return 'darwin'

	def arch(self):
		return UnixSetup.arch(self)

	def cmake_commandline(self, src_dir, build_dir, opts):
		args = dict(
			dir=src_dir,
			generator=self.generator,
			opts=quote(opts) if opts else '',
			systemlibs=self.systemlibs,
			project_name=self.project_name,
			type=self.build_type.upper(),
			)
		return ('cmake -G %(generator)r '
				'-DCMAKE_BUILD_TYPE:STRING=%(type)s '
				'-DUSESYSTEMLIBS:BOOL=%(systemlibs)s '
				'-DROOT_PROJECT_NAME:STRING=%(project_name)s '
				'%(opts)s %(dir)r' % args)


class WindowsSetup(PlatformSetup):
	gens = {
		'vs2022' : {
			'gen' : r'Visual Studio 17 2022',
			'toolset' : r'v143'
			},
		'vs2022-clang' : {
			'gen' : r'Visual Studio 17 2022',
			'toolset' : r'ClangCL'
			}
		}

	search_path = r'C:\windows'
	exe_suffixes = ('.exe', '.bat', '.com')

	def __init__(self):
		super(WindowsSetup, self).__init__()
		self._generator = None

	def _get_generator(self):
		return self._generator

	def _set_generator(self, gen):
		self._generator = gen

	generator = property(_get_generator, _set_generator)

	def os(self):
		osname = 'win64'
		return osname

	def arch(self):
		cpu ='x86_64'
		return cpu

	def build_dirs(self):
		return [os.path.join('..', 'build-' + self.generator)]

	def cmake_commandline(self, src_dir, build_dir, opts):
		genstring = self.gens[self.generator.lower()]['gen']
		toolsetstr = self.gens[self.generator.lower()]['toolset']
		args = dict(
			dir=src_dir,
			generator=genstring,
			toolset=toolsetstr,
			opts=quote(opts) if opts else '',
			systemlibs=self.systemlibs,
			project_name=self.project_name,
			)
		return ('cmake -G "%(generator)s" -T %(toolset)s '
				'-DUSESYSTEMLIBS:BOOL=%(systemlibs)s '
				'-DROOT_PROJECT_NAME:STRING=%(project_name)s '
				'%(opts)s "%(dir)s"' % args)

	def get_HKLM_registry_value(self, key_str, value_str):
		if sys.version_info[0] == 3:
			import winreg as wreg
		else:
			import _winreg as wreg
		reg = wreg.ConnectRegistry(None, wreg.HKEY_LOCAL_MACHINE)
		key = wreg.OpenKey(reg, key_str)
		value = wreg.QueryValueEx(key, value_str)[0]
		print('Found: %s' % value)
		return value

	def run(self, command, name=None):
		'''Run a program.  If the program fails, raise an exception.'''
		ret = os.system(command)
		if ret:
			if name is None:
				name = command.split(None, 1)[0]
			path = self.find_in_path(name)
			if not path:
				ret = 'was not found'
			else:
				ret = 'exited with status %d' % ret
			raise CommandError('the command %r %s' %
							   (name, ret))

	def run_cmake(self, args=[]):
		PlatformSetup.run_cmake(self, args)

class CygwinSetup(WindowsSetup):
	def __init__(self):
		super(CygwinSetup, self).__init__()
		self.generator = 'vc143'

	def cmake_commandline(self, src_dir, build_dir, opts):
		dos_dir = cmds.getoutput("cygpath -w %s" % src_dir)
		args = dict(
			dir=dos_dir,
			generator=self.gens[self.generator.lower()]['gen'],
			opts=quote(opts) if opts else '',
			systemlibs=self.systemlibs,
			project_name=self.project_name,
			)
		return ('cmake -G "%(generator)s" '
				'-DUSESYSTEMLIBS:BOOL=%(systemlibs)s '
				'-DROOT_PROJECT_NAME:STRING=%(project_name)s '
				'%(opts)s "%(dir)s"' % args)

setup_platform = {
	'darwin' : DarwinSetup,
	'linux'  : LinuxSetup,
	'linux2' : LinuxSetup,
	'win32'  : WindowsSetup,
	'cygwin' : CygwinSetup
	}


usage_msg = '''
Usage: develop.py [options] [command [command-options]]

Options:
  -h | --help           print this help message
	   --systemlibs     build against available system libs instead of prebuilt libs
  -t | --type=NAME      build type ("Release", "RelWithDebInfo" or "Debug")
  -N | --no-distcc      disable use of distcc
  -G | --generator=NAME generator name
                        Windows: "vs2022" or "vs2022-clang"
                        Mac OS X: "Xcode" (default) or "Unix Makefiles"
                        Linux: "Unix Makefiles" (default) or "Ninja"
  -p | --project=NAME   overrides the root project name (does not affect makefiles)

Commands:
  configure       configure project by running cmake (default if none given)
  build           configure and build default target (for Linux only !)
  clean           delete all build directories, does not affect sources
  printbuilddirs  print the build directory that will be used

Command-options for "configure":
  We use cmake variables to change the build configuration. E.g., to set up
  the project for the "Release" target and ignore fatal warnings:
	develop.py -t Release configure -DNO_FATAL_WARNINGS:BOOL=TRUE
'''

def main(arguments):
	setup = setup_platform[sys.platform]()
	try:
		opts, args = getopt.getopt(
			arguments,
			'?hNt:p:G:',
			['help', 'systemlibs', 'no-distcc', 'type=', 'generator=', 'project='])
	except getopt.GetoptError as err:
		print('Error:', err, file=sys.stderr)
		print("""
Note: You must pass -D options to cmake after the "configure" command
For example: develop.py configure -DCMAKE_BUILD_TYPE=Release""", file=sys.stderr)
		print(usage_msg.strip(), file=sys.stderr)
		sys.exit(1)

	for o, a in opts:
		if o in ('-?', '-h', '--help'):
			print(usage_msg.strip())
			sys.exit(0)
		elif o in ('--systemlibs',):
			setup.systemlibs = 'ON'
		elif o in ('-t', '--type'):
			try:
				setup.build_type = setup.build_types[a.lower()]
			except KeyError:
				print('Error: unknown build type', repr(a), file=sys.stderr)
				print('Supported build types:', file=sys.stderr)
				types = list(setup.build_types.values())
				types.sort()
				for t in types:
					print(' ', t)
				sys.exit(1)
		elif o in ('-G', '--generator'):
			setup.generator = a
		elif o in ('-N', '--no-distcc'):
			setup.distcc = False
		elif o in ('-p', '--project'):
			setup.project_name = a
		else:
			print('INTERNAL ERROR: unhandled option', repr(o), file=sys.stderr)
			sys.exit(1)
	if not args:
		setup.run_cmake()
		return
	try:
		cmd = args.pop(0)
		if cmd in ('cmake', 'configure'):
			setup.run_cmake(args)
		elif cmd == 'build':
			if os.getenv('DISTCC_DIR') is None:
				distcc_dir = os.path.join(getcwd(), '.distcc')
				if not os.path.exists(distcc_dir):
					os.mkdir(distcc_dir)
				print("setting DISTCC_DIR to %s" % distcc_dir)
				os.environ['DISTCC_DIR'] = distcc_dir
			else:
				print("DISTCC_DIR is set to %s" % os.getenv('DISTCC_DIR'))
			for d in setup.build_dirs():
				if not os.path.exists(d):
					raise CommandError('run "develop.py cmake" first')
			setup.run_cmake()
			opts, targets = setup.parse_build_opts(args)
			setup.run_build(opts, targets)
		elif cmd == 'clean':
			if args:
				raise CommandError('clean takes no arguments')
			setup.cleanup()
		elif cmd == 'printbuilddirs':
			for d in setup.build_dirs():
				print(d, file=sys.stdout)
		else:
			print('Error: unknown subcommand', repr(cmd), file=sys.stderr)
			print("(run 'develop.py --help' for help)", file=sys.stderr)
			sys.exit(1)
	except getopt.GetoptError as err:
		print('Error with %r subcommand: %s' % (cmd, err), file=sys.stderr)
		sys.exit(1)


if __name__ == '__main__':
	try:
		main(sys.argv[1:])
	except CommandError as err:
		print('Error:', err, file=sys.stderr)
		sys.exit(1)
