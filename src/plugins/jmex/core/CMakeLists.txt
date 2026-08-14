# SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(jmex_plugin_core STATIC
    moxi_session.cpp
    joint_state_publisher.cpp
    inc/jmex/moxi_session.hpp
    inc/jmex/joint_state_publisher.hpp
)

target_include_directories(jmex_plugin_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
)

target_link_libraries(jmex_plugin_core
    PUBLIC
        # Moxi::Receiver is PUBLIC because main.cpp needs MOXI_LOCAL_MOTION_ROBOT to pick a dialect.
        Moxi::Receiver
        pusherio::pusherio
        oxr::oxr_core
    PRIVATE
        oxr::oxr_utils
        isaacteleop_schema
)

set_target_properties(jmex_plugin_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

add_library(jmex::jmex_plugin_core ALIAS jmex_plugin_core)
