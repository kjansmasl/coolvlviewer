This directory contains files from the parallel-hashmap project by Gregory
Popovitch (https://github.com/greg7mdp/parallel-hashmap).

They are better kept with the viewer sources instead of being donwloaded
separately as a "pre-built library package", especially since they constitute
a header-only library common to all OSes.

Plus, I slightly modified the files to:
 - better integrate with the viewer code (with the use of LL_[NO_]INLINE);
 - provide a minor speed optimization (PHMAP_NO_MIXING);
 - allow the use of sse2neon.h with ARM64 builds, in order to use the optimized
   SSE2/SSSE3 code (which then gets automatically translated into their NEON
   counterparts);
 - explicitely disable the use of boost shared mutexes: we now use the C++11
   standard library mutexes, even though we also have boost used for other
   purposes in the viewer, which caused phmap to implicitely use boost for
   mutexes, sometimes (and not everywhere), due to nested boost headers
   inclusions (e.g. via boost/thread.hpp);
 - explicitely disable the use of Windows SRWLOCK mutexes: the viewer fails to
   compile with them and, anyway, I prefer to use the same type of mutexes
   (std::mutex) everywhere in the code...

I also skipped a couple changes that we either do not care about (compatibility
with gcc v4, which cannot compile today's viewer code anyway), or cause issues
(new hashing code, inspired from boost's, but that causes weird rendering
glitches).

HB
