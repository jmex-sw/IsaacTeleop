# SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_executable(jmex_plugin
    main.cpp
)

target_link_libraries(jmex_plugin
    PRIVATE
        jmex::jmex_plugin_core
)

install(TARGETS jmex_plugin RUNTIME DESTINATION plugins/jmex)
