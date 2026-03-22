# SPDX-License-Identifier: Zlib
# Copyright (c) 2026 François Chabot

add_executable(maru_example_basic_metal main.m)
target_link_libraries(maru_example_basic_metal PRIVATE maru::maru ${example_settings})
target_link_libraries(maru_example_basic_metal PRIVATE "-framework Metal" "-framework QuartzCore" "-framework Cocoa")
