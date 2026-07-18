cmake_minimum_required(VERSION 3.24)

project(Nyxbyte VERSION 0.2.0 LANGUAGES CXX RC)

add_executable(Nyxbyte WIN32
    src/main.cpp
    src/ambient_brain.cpp
    src/settings.cpp
    src/resource.rc
)

target_compile_features(Nyxbyte PRIVATE cxx_std_20)
target_include_directories(Nyxbyte PRIVATE include src)
target_compile_definitions(Nyxbyte PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(Nyxbyte PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(Nyxbyte PRIVATE
    gdiplus
    shell32
    user32
    gdi32
)

add_custom_command(TARGET Nyxbyte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Nyxbyte>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/spritesheet.png"
        "$<TARGET_FILE_DIR:Nyxbyte>/assets/spritesheet.png"
    COMMENT "Copying Nyxbyte sprites"
)

include(CTest)

if(BUILD_TESTING)
    add_executable(NyxbyteMonitorTests
        tests/monitor_layout_tests.cpp
    )
    target_compile_features(NyxbyteMonitorTests PRIVATE cxx_std_20)
    target_include_directories(NyxbyteMonitorTests PRIVATE include)
    if(MSVC)
        target_compile_options(NyxbyteMonitorTests PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    add_test(NAME monitor-layout COMMAND NyxbyteMonitorTests)

    add_executable(NyxbyteSettingsTests
        tests/settings_tests.cpp
        src/settings.cpp
    )
    target_compile_features(NyxbyteSettingsTests PRIVATE cxx_std_20)
    target_include_directories(NyxbyteSettingsTests PRIVATE include)
    if(MSVC)
        target_compile_options(NyxbyteSettingsTests PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    add_test(NAME settings-persistence COMMAND NyxbyteSettingsTests)
endif()
