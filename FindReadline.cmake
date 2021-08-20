# Code copied from sethhall@github
#
# - Try to find readline include dirs and libraries 
#
# Usage of this module as follows:
#
#     find_package(Readline)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Readline_ROOT_DIR         Set this variable to the root installation of
#                            readline if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  READLINE_FOUND            System has readline, include and lib dirs found
#  Readline_INCLUDE_DIR      The readline include directories. 
#  Readline_LIBRARY          The readline library.

find_path(Readline_ROOT_DIR
    NAMES include/readline/readline.h
)

find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
)

find_library(Readline_LIBRARY
    NAMES readline
    HINTS ${Readline_ROOT_DIR}/lib
)

find_library(Tinfo_LIBRARY
    NAMES libtinfo libtinfo.so libtinfo.so.6 libtinfo.so.6.1
)

if(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Tinfo_LIBRARY)
    set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Tinfo_LIBRARY)
    find_library(Readline_LIBRARY NAMES readline)
    find_library(Tinfo_LIBRARY NAMES tinfo tinfo.so tinfo.so.6 tinfo.so.6.1)
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline
        DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY
    )
    MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARY Tinfo_LIBRARY)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Tinfo_LIBRARY)

mark_as_advanced(
    Readline_ROOT_DIR
    Readline_INCLUDE_DIR
    Readline_LIBRARY
    Tinfo_LIBRARY
)
