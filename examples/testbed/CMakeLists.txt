add_executable(maru_testbed 
    main.cpp 
    imgui_impl_maru.cpp
    app.cpp
    modules/event_log_module.cpp
    modules/event_module.cpp
    modules/timing_module.cpp
    modules/monitor_module.cpp
    modules/cursor_module.cpp
    modules/window_module.cpp
    modules/input_module.cpp
    modules/controller_module.cpp
    modules/instrumentation_module.cpp
    modules/composition_module.cpp
    modules/dataexchange_module.cpp
    modules/context_module.cpp
)
target_link_libraries(maru_testbed PRIVATE maru::maru imgui ${example_settings})
target_compile_features(maru_testbed PRIVATE cxx_std_20)
