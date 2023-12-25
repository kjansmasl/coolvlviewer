NOTE: by lack of time, this file is not accuratelty maintained and rarely ever
updated; as a result it got some outdated and no more relevant parts. If you do
not find an answer to your problem in it, do search the Cool VL Viewer forum
for one: http://sldev.free.fr/forum/
The "FAQ and general help" sub-forum, especially, is a mine of information, and
a handy search feature is available for the whole forum:
http://sldev.free.fr/forum/search.php


1.- Cool VL Viewer - Linux README - INTRODUCTION
    -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

This document contains information about the Cool VL Viewer Linux client, and
is not meant to serve as an introduction to Second Life itself (for that
purpose, please see: https://support.secondlife.com/ ).

1. Introduction (this chapter)
2. System Requirements
3. Installing & Running
4. Troubleshooting
   4.1. Error creating window.
   4.2. Failure to detect the (i)GPU (V)RAM amount.
   4.3. Blank window after minimizing it.
   4.4. Audio issues.
   4.5. 'ALT' key for camera controls does not work.
   4.6. In-world streaming movie/music playback issues.
5. Advanced Troubleshooting
   5.1. Audio
   5.2. OpenGL
6. Getting more help and reporting problems

Appendix A: Voice issues
Appendix B: Joystick issues.


2. SYSTEM REQUIREMENTS
-=-=-=-=-=-=-=-=-=-=-=

Minimum requirements:
    * Internet connection: DSL, cable, or optical fiber.
    * Computer processor: 64 bits x86 CPU, or 64 bits ARM CPU for custom ARM
      builds. A quad-core is the recommended minimum.
    * Computer memory: 4GB minimum (recommended: 8GB and more).
    * Linux operating system: a reasonably modern 64 bits Linux environment is
      required. To use voice, you will also need your distribution's 32 bits
      compatibility environment installed, or Wine (the latter is recommended
      since it allows to run an up to date voice client, while the 32 bits
      Linux voice client is outdated).
    * Screen Resolution: 1280x1024 pixels or more.
    * Video/Graphics card: NVIDIA GeForce 8800 GT or better (with proprietary
      drivers; NVIDIA Open Source drivers are too slow and incomplete), or an
      AMD Radeon R5 or better, or Intel UHD Graphics or better, both AMD and
      Intel GPUs with latest Mesa OpenGL drivers. A GTX 460 or better graphics
      card is highly recommended.
      For ARM custom builds, the GPU part of the SoC must have proper OpenGL
      v2.1 or better support under your Linux distribution.

      **NOTE**: Second Life viewers absolutely require you to have recent,
      correctly configured OpenGL 3D drivers for your hardware; the graphics
      drivers that came with your operating system are likely not good enough !
      See the TROUBLESHOOTING section if you encounter problems starting the
      Cool VL Viewer.


3. INSTALLING & RUNNING
-=-=-=-=-=-=-=-=-=-=-=-

The Cool VL Viewer Linux client entirely runs out of the directory you have
installed it into.

Run ./cool_vl_viewer from the installation directory to start the Cool VL
Viewer, or use the menu launcher that got installed (if you so chose at
installation time).

You need to have the compatibility 32 bits support system libraries installed
if you want to be able to use the Voice feature.

You may easily check for any missing library by pointing a terminal window into
the Cool VL Viewer installation directory and typing:
./cool_vl_viewer --check-libs

For in-world MOVIE and MUSIC PLAYBACK, you will need gstreamer v1.0 installed
on your system. This is optional (i.e. it is not required for general client
functionality). If you have gstreamer installed, the selection of in-world
movies you can successfully play will depend on the gstreamer plugins and
CODECs you have installed; if you cannot play a certain in-world movie then
you are probably missing the appropriate gstreamer plugin/CODEC on your system;
you may be able to install it (see TROUBLESHOOTING).

User data is stored in the hidden directory ~/.secondlife by default; you may
override this location with the SECONDLIFE_USER_DIR environment variable if
you wish.


4. TROUBLESHOOTING
-=-=-=-=-=-=-=-=-=

The client prints a lot of diagnostic information to the console it was run
from. Most of this (plugins are a notable exception and only log to the error
console, which is not duplicated in the log file) is also replicated in
~/.secondlife/logs/CoolVLViewer.log
- this is helpful to read when troubleshooting, especially 'WARNING' and
'ERROR' lines.

VOICE PROBLEMS ?
See Appendix A for Voice troubleshooting information.

SPACENAVIGATOR OR JOYSTICK PROBLEMS ?
See Appendix B for configuration information.

PROBLEM 1: the Cool VL Viewer fails to start up, with a warning on the console
like:
   'Error creating window.' or
   'Unable to create window, be sure screen is set at 32 bits color' or
   'SDL: Could not find matching GLX visual.'
SOLUTION: Usually this indicates that your graphics card does not meet the
minimum requirements, or that your system's OpenGL 3D graphics driver is not
updated and configured correctly. If you believe that your graphics card DOES
meet the minimum requirements then you likely need to install the official
so-called 'non-free' NVIDIA or AMD graphics drivers; we suggest one of the
following options:
 * Consult your Linux distribution's documentation for installing these
   official drivers. For example, Ubuntu provides documentation here:
   https://help.ubuntu.com/community/BinaryDriverHowto
 * If your distribution does not make it easy, then you can (and in fact,
   should) download the required Linux drivers straight from your graphics
   card manufacturer (with great benefits for both speed and stability):
   - NVIDIA cards: http://www.nvidia.com/Download/index.aspx
   - AMD cards: http://support.amd.com/en-us/download

PROBLEM 2: The viewer fails to detect my (i)GPU (V)RAM amount and limits the
texture memory to 128Mb.
SOLUTION: Some GPUs (most often, iGPUs that do not have any VRAM but use the
system RAM instead) do not allow Xorg to report the available memory amount
available to them, causing a failure by viewers to detect the said amount
(which then defaults to the lowest supported setting). You may bbypass the
viewer detection algorithm by editing the cool_vl_viewer.conf file and
uncommenting the "#export LL_VRAM_MB=512" line to read:
----------
## - If the viewer fails to properly detect the amount of VRAM on your graphics
##   card, you may specify it (in megabytes) via this variable.
export LL_VRAM_MB=512
----------
You can of course change 512 for anything fancying you, as long as your
graphics driver and GPU can handle it, and you got enough RAM to fit it.

PROBLEM 3: After I minimize the Cool VL Viewer window, it is just blank when it
comes back.
SOLUTION: Some Linux desktop 'Visual Effects' features are incompatible with
Second Life viewers. One reported solution is to use your desktop configuration
program to disable such effects. For example, on Ubuntu 7.10, use the desktop
toolbar menu to select System -> Preferences -> Appearance, then change
'Visual Effects' to 'None'.

PROBLEM 4: Music and sound effects are silent or very stuttery.
SOLUTION: This may be a conflict between sound backends.
See the chapter 5 below, "Audio" section for sound backend selections.

PROBLEM 5: Using the 'ALT' key to control the camera does not work or just
moves the Cool VL Viewer window.
SOLUTION: Some window managers eat the ALT key for their own purposes; you can
configure your window manager to use a different key instead (for example, the
'Windows' key) which will allow the ALT key to function properly with mouse
actions in the Cool VL Viewer and other applications.

PROBLEM 6: In-world movie and/or music playback doesn't work for me.
SOLUTION: You need to have a working installation of gstreamer; this is usually
an optional package for most versions of Linux. If you have installed gstreamer
and you can play some music/movies but not others then you need to install a
wider selection of gstreamer plugins, either from your vendor or an appropriate
third party.


5. ADVANCED TROUBLESHOOTING
-=-=-=-=-=-=-=-=-=-=-=-=-=-

* Audio - You may configure which sound backend (OpenAL or FMOD and for the
latter ALSA, OSS, Pulseaudio) you wish to use or disable, by checking the
corresponding options in the "Advanced -> "Media" menu (shown while logged in)
and restarting the sound engine with the corresponding entry in that same menu.


6. GETTING MORE HELP AND REPORTING PROBLEMS
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

Support for the Cool VL Viewer is provided *exclusively* through its dedicated
forum (http://sldev.free.fr/forum/).

For general help and support with Second Life:
http://secondlife.com/community/support.php


*******************************************************************************
APPENDIX A: VOICE ISSUES
*******************************************************************************

WHAT IS IT ?
-=-=-=-=-=-=

Second Life viewers allow to participate in voice-chat with other residents and
groups inside Second Life, with an appropriate headset/microphone.

The Linux Voice client is no more supported by Vivox or Linden Lab.
The Cool VL Viewer comes bundled with the "last known good" version of the
Linux voice client, but there is no guarantee whatsoever that it is still
working properly knowadays (in fact, as of September 2020, it is largely broken
with, for example, slow or non-updating speakers list).

If you wish to use SL Voice with this viewer, it is recommended that you
install Wine on your system and use the provided install-wine-SLVoice.sh script
which will itself care to install the up to date Windows SLVoice.exe client and
setup everything for the viewer to use it. Simply execute the script from a
terminal and follow its instructions. The installation needs to be performed
only once per Linux user account (and can be repeated anytime, for example to
update the SLVoice.exe client to a newer version). You do not need to reinstall
the SLVoice.exe client when updating the viewer to a new version.


REQUIREMENTS
-=-=-=-=-=-=

* A headset/microphone supported by your chosen version of Linux
* ALSA,or Pulseaudio sound.


TROUBLESHOOTING
-=-=-=-=-=-=-=-

PROBLEM 1: I don't see a white dot over the head of my avatar or other
  Voice-using avatars.
SOLUTION:
a. Ensure that "Enable voice chat" box is checked in the "Voice chat" tab of
   the "Preferences" window and that you are in a voice-enabled area (areas
   without voice support got a barred voice icon in the menu-bar).
b. Sometimes, the voice client fails to connect after a login or TP: restarting
   it (by un-checking and, after a couple seconds, re-checking the "Enable
   voice chat" box in the "Voice chat" tab of the "Preferences" window) should
   solve such an issue.

IMPORTANT NOTE: OUTDATED/UNVERIFIED solutions below with regard to ALSA
(especially if you are using the Windows voice client via Wine, like you should
be doing):

PROBLEM 2: I have a white dot over my head but I never see (or hear!) anyone
  except myself listed in the Active Speakers dialog when I'm sure that other
  residents nearby are active Voice users.
SOLUTION: this is an incompatibility between the Voice support and your
  system's audio (ALSA) driver version/configuration.
a. Back-up and remove your ~/.asoundrc file, re-test.
b. Check for updates to your kernel, kernel modules and ALSA-related
  packages using your Linux distribution's package-manager - install these,
  reboot and re-test.
c. Update to the latest version of ALSA manually.  For a guide, see the
  'Update to the Latest Version of ALSA' section of this page:
  <https://help.ubuntu.com/community/HdaIntelSoundHowto> or the official
  documentation on the ALSA site: <http://www.alsa-project.org/> - reboot
  and re-test.

PROBLEM 3: I can hear other people, but they cannot hear me.
SOLUTION:
a. Ensure that you have the 'Talk' button activated while you are trying to
  speak.
b. Ensure that your microphone jack is inserted into the correct socket of your
  sound card, where appropriate.
c. Use your system mixer-setting program or the 'alsamixer' program to ensure
  that microphone input is set as the active input source and is not muted.
d. Verify that audio input works in other applications, i.e. Audacity

PROBLEM 4: Other people just hear bursts of loud noise when I speak.
SOLUTION:
a. Use your system mixer-setting program or the 'alsamixer' program to ensure
  that microphone Gain/Boost is not set too high.


*******************************************************************************
APPENDIX B: JOYSTICK ISSUES
*******************************************************************************

WHAT IS IT ?
-=-=-=-=-=-=

This feature allows the use of a joystick or other supported multi-axis device
for controlling your avatar and camera.

REQUIREMENTS
-=-=-=-=-=-=

* A joystick or other generic multi-axis input device supported by your chosen
  version of Linux

- OR -

* A SpaceNavigator device (additional configuration may be required, see below)

CONFIGURATION
-=-=-=-=-=-=-

SPACE NAVIGATOR: *Important* - do not install the Linux SpaceNavigator drivers
from the disk included with the device - these are problematic. Some Linux
distributions (such as Ubuntu or Gentoo) might need some system configuration
to make the SpaceNavigator usable by applications such as a Second Life viewer.
Please, see the https://github.com/janoc/libndofdev readme for a list of
supported devices.

* Ubuntu or Gentoo Linux configuration:
  For a quick start, you can simply paste the following line into a terminal
  before plugging in your SpaceNavigator - this only needs to be done once:
  sudo bash -c 'echo KERNEL==\"event[0-9]*\", SYSFS{idVendor}==\"046d\", SYSFS{idProduct}==\"c626\", SYMLINK+=\"input/spacenavigator\", GROUP=\"plugdev\", MODE=\"664\" >> /etc/udev/rules.d/91-spacenavigator.rules'

JOYSTICKS: These should be automatically detected and configured on all modern
distributions of Linux.

ALL: Your joystick or SpaceNavigator should be plugged-in before you start the
Cool VL Viewer, so that it may be detected. If you have multiple input devices
attached, only the first detected SpaceNavigator or joystick device will be
available.

Once your system recognises your joystick or SpaceNavigator correctly, you can
go into the Cool VL Viewer's Preferences dialog, click the 'Input & Camera' tab,
and click the 'Joystick Setup' button. From here you may enable and disable
joystick support and change some configuration settings such as sensitivity.
SpaceNavigator users are recommended to click the 'SpaceNavigator Defaults'
button.

KNOWN PROBLEMS
-=-=-=-=-=-=-=

* If your chosen version of Linux treats your joystick/SpaceNavigator as if it
were a mouse when you plug it in (i.e. it is automatically used to control your
desktop cursor), then the viewer may detect this device *but* will be unable to
use it properly.
