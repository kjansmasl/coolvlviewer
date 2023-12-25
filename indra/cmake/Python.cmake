# -*- cmake -*-
if (PYTHON_CMAKE_INCLUDED)
  return()
endif (PYTHON_CMAKE_INCLUDED)
set (PYTHON_CMAKE_INCLUDED TRUE)

set(PYTHONINTERP_FOUND)

if (WINDOWS)
  # On Windows, explicitly avoid Cygwin Python.
  find_program(PYTHON_EXECUTABLE
    NAMES python313.exe python312.exe python311.exe python310.exe python39.exe python38.exe python37.exe python36.exe python35.exe python34.exe python33.exe python27.exe python26.exe python.exe
    NO_DEFAULT_PATH # added so that cmake does not find cygwin python
    PATHS
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.13\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.13\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.12\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.12\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.11\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.11\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.10\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.10\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.9\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.9\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.8\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.8\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.7\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.7\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.6\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.6\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.5\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.5\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.4\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.4\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.3\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.3\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\2.7\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\2.7\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\2.6\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\2.6\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.9-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.9-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.8-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.8-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.7-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.7-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.6-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.6-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.5-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.5-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.4-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.4-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.3-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.3-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\2.7-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\2.7-32\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\2.6-32\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\2.6-32\\InstallPath]
    )
  if (PYTHON_EXECUTABLE)
    set(PYTHONINTERP_FOUND ON)
  endif (PYTHON_EXECUTABLE)
elseif (LINUX)
  string(REPLACE ":" ";" PATH_LIST "$ENV{PATH}")
  find_program(PYTHON_EXECUTABLE python3.13 python3.12 python3.11 python3.10 python3.9 python3.8 python3.7 python3.6 python3.5 python3.4 python3.3 python2.7 python2.6 python3 python2 python
               PATHS ${PATH_LIST})
  if (PYTHON_EXECUTABLE)
    set(PYTHONINTERP_FOUND ON)
  endif (PYTHON_EXECUTABLE)
elseif (DARWIN)
  # On MAC OS X be sure to search standard locations first
  string(REPLACE ":" ";" PATH_LIST "$ENV{PATH}")
  find_program(PYTHON_EXECUTABLE
    NAMES python313 python312 python311 python310 python39 python38 python37 python36 python35 python34 python33 python27 python26 python3 python2 python
    NO_DEFAULT_PATH # Avoid searching non-standard locations first
    PATHS
    /bin
    /usr/bin
    /usr/local/bin
    ${PATH_LIST}
    )
  if (PYTHON_EXECUTABLE)
    set(PYTHONINTERP_FOUND ON)
  endif (PYTHON_EXECUTABLE)
endif (WINDOWS)

if (NOT PYTHONINTERP_FOUND)
  message(FATAL_ERROR "No compatible Python interpreter found !")
endif (NOT PYTHONINTERP_FOUND)

mark_as_advanced(PYTHON_EXECUTABLE)
message("-- Using Python executable: ${PYTHON_EXECUTABLE}")
