# SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

# The MOXI Receiver SDK is a separate download from j-mex (see README.md). Unpack it into this
# directory as MOXIReceiverSDK/ and no build flags are needed -- .gitignore keeps it out of the
# repository, as with the vendor SDK the manus plugin expects.
#
# CMAKE_PREFIX_PATH and MOXI_SDK_ROOT still point the search elsewhere; the local directory is only
# the last place looked. Either one wants the platform directory holding lib/cmake/MOXIReceiverSDK:
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/MOXIReceiverSDK-1.1.0/sdk/linux-x64
#
# Without the SDK the whole plugin is skipped rather than failing the build, so a tree without it
# still configures and builds -- the same arrangement the haptikos and manus plugins use.
set(_JMEX_LOCAL_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/MOXIReceiverSDK")

if(NOT DEFINED MOXI_SDK_ROOT AND DEFINED ENV{MOXI_SDK_ROOT})
    set(MOXI_SDK_ROOT "$ENV{MOXI_SDK_ROOT}" CACHE PATH "Path to the MOXI Receiver SDK platform directory")
endif()

# PATHS is searched after CMAKE_PREFIX_PATH, and within PATHS the explicit root precedes the local
# one, so an override always wins over whatever happens to be unpacked here.
#
# 1.1 is a hard minimum, not a preference: the sources include MoxiRobotReceiver.h, which 1.0.0 does
# not ship, and call MxRStartSystem with the udp start port it grew in 1.1.0. Asking for the version
# here turns an older SDK into the same clean skip as a missing one, rather than a compile error.
find_package(MOXIReceiverSDK 1.1 QUIET PATHS
             ${MOXI_SDK_ROOT}
             "${_JMEX_LOCAL_SDK_ROOT}/sdk/linux-x64"
             "${_JMEX_LOCAL_SDK_ROOT}")

if(NOT TARGET Moxi::Receiver)
    message(STATUS "Skipping jmex plugin build: MOXIReceiverSDK 1.1 or newer not found "
                   "(unpack the SDK into ${_JMEX_LOCAL_SDK_ROOT}, or set CMAKE_PREFIX_PATH or "
                   "MOXI_SDK_ROOT to <sdk>/sdk/linux-x64)")
    return()
endif()

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tools)

install(FILES plugin.yaml README.md DESTINATION plugins/jmex)
