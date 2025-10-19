#[=======================================================================[

FindCThreads
---------

Find cthreads includes and library.

Imported Targets
^^^^^^^^^^^^^^^^

An :ref:`imported target <Imported targets>` named
``CTHREADS::THREAD`` is provided if cthreads has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``CTHREADS_FOUND``
  True if cthreads was found, false otherwise.
``CTHREADS_INCLUDE_DIRS``
  Include directories needed to include cthreads headers.
``CTHREADS_LIBRARIES``
  Libraries needed to link to cthreads.
``CTHREADS_VERSION``
  The version of cthreads found.
``CTHREADS_VERSION_MAJOR``
  The major version of cthreads.
``CTHREADS_VERSION_MINOR``
  The minor version of cthreads.
``CTHREADS_VERSION_PATCH``
  The patch version of cthreads.

Cache Variables
^^^^^^^^^^^^^^^

This module uses the following cache variables:

``CTHREADS_LIBRARY``
  The location of the cthreads library file.
``CTHREADS_INCLUDE_DIR``
  The location of the cthreads include directory containing ``cthreads.h``.

The cache variables should not be used by project code.
They may be set by end users to point at cthreads components.
#]=======================================================================]

find_library(cthreads_LIBRARY
  NAMES
  	cthreads
  	libcthreads
  )
mark_as_advanced(cthreads_LIBRARY)

find_path(cthreads_INCLUDE_DIR
  NAMES cthreads.h
  )
mark_as_advanced(cthreads_INCLUDE_DIR)

#-----------------------------------------------------------------------------
# Extract version number if possible.
set(_CTHREADS_H_REGEX "#[ \t]*define[ \t]+CTHREADS_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
if(CTHREADS_INCLUDE_DIR AND EXISTS "${CTHREADS_INCLUDE_DIR}/cthreads.h")
  file(STRINGS "${CTHREADS_INCLUDE_DIR}/cthreads.h" _CTHREADS_H REGEX "${_CTHREADS_H_REGEX}")
else()
  set(_CTHREADS_H "")
endif()
foreach(c MAJOR MINOR PATCH)
  if(_CTHREADS_H MATCHES "#[ \t]*define[ \t]+CTHREADS_VERSION_${c}[ \t]+([0-9]+)")
    set(_CTHREADS_VERSION_${c} "${CMAKE_MATCH_1}")
  else()
    unset(_CTHREADS_VERSION_${c})
  endif()
endforeach()

if(DEFINED _CTHREADS_VERSION_MAJOR AND DEFINED _CTHREADS_VERSION_MINOR)
  set(CTHREADS_VERSION_MAJOR "${_CTHREADS_VERSION_MAJOR}")
  set(CTHREADS_VERSION_MINOR "${_CTHREADS_VERSION_MINOR}")
  set(CTHREADS_VERSION "${CTHREADS_VERSION_MAJOR}.${CTHREADS_VERSION_MINOR}")
  if(DEFINED _CTHREADS_VERSION_PATCH)
    set(CTHREADS_VERSION_PATCH "${_CTHREADS_VERSION_PATCH}")
    set(CTHREADS_VERSION "${CTHREADS_VERSION}.${CTHREADS_VERSION_PATCH}")
  else()
    unset(CTHREADS_VERSION_PATCH)
  endif()
else()
  set(CTHREADS_VERSION_MAJOR "")
  set(CTHREADS_VERSION_MINOR "")
  set(CTHREADS_VERSION_PATCH "")
  set(CTHREADS_VERSION "")
endif()
unset(_CTHREADS_VERSION_MAJOR)
unset(_CTHREADS_VERSION_MINOR)
unset(_CTHREADS_VERSION_PATCH)
unset(_CTHREADS_H_REGEX)
unset(_CTHREADS_H)

#-----------------------------------------------------------------------------
# Set Find Package Arguments
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(cthreads
    FOUND_VAR cthreads_FOUND
    REQUIRED_VARS CTHREADS_LIBRARY CTHREADS_INCLUDE_DIR
    VERSION_VAR CTHREADS_VERSION
    HANDLE_COMPONENTS
        FAIL_MESSAGE
        "Could NOT find CThreads"
)

set(CTHREADS_FOUND ${cthreads_FOUND})
set(CTHREADS_LIBRARIES ${CTHREADS_LIBRARY})

#-----------------------------------------------------------------------------
# Provide documented result variables and targets.
if(CTHREADS_FOUND)
  set(CTHREADS_INCLUDE_DIRS ${CTHREADS_INCLUDE_DIR})
  set(CTHREADS_LIBRARIES ${CTHREADS_LIBRARY})
  if(NOT TARGET CTHREADS::THREAD)
    add_library(CTHREADS::THREAD UNKNOWN IMPORTED)
    set_target_properties(CTHREADS::THREAD PROPERTIES
      IMPORTED_LOCATION "${CTHREADS_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CTHREADS_INCLUDE_DIRS}"
      )
  endif()
endif()
