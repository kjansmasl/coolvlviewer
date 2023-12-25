# -*- cmake -*-

# Ensure Tracy support is off for plugins and SLPlugin, to avoid crashes in
# them; seen occurring at least under Windoze; Linux is fine; macOS unknown. HB
include(00-BuildOptions)
set(USE_TRACY OFF)

# Plugins common to all OSes
add_subdirectory(base)
add_subdirectory(cef)
add_subdirectory(gstreamer)

# Plugin wrapper program
add_subdirectory(slplugin)
