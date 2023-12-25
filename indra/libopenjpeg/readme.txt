I decided to integrate libopenjpeg to the viewer sources, because the version
in use is very specific to the viewer and got many fixes (crash bugs, decoding
issues, security fixes) applied by me. It is also optimized with SSE/AVX code
and I also optimized manually the non-vectorized code for speedier decoding/
encoding. Finally, it got NEON vectorization support added via sse2neon.

Henri Beauchamp.

Note: for the libopenjpeg license, please see the doc/licenses-linux.txt file.
