set(EXAMPLE_SOURCES
    ${CMAKE_SOURCE_DIR}/examples/examples_common.cpp
    ${CMAKE_SOURCE_DIR}/examples/tephra_examples.cpp
    ${CMAKE_SOURCE_DIR}/examples/window.cpp
    
    ${CMAKE_SOURCE_DIR}/examples/1_cube/cube.cpp
)

add_executable(TephraExamples ${EXAMPLE_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(TephraExamples PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(TephraExamples PRIVATE VK_USE_PLATFORM_WIN32_KHR UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    # Assume X over Wayland
    find_package(X11 REQUIRED)
    target_compile_definitions(TephraExamples PRIVATE VK_USE_PLATFORM_XLIB_KHR)
    target_link_libraries(TephraExamples PRIVATE X11)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_compile_definitions(TephraExamples PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()

target_link_libraries(TephraExamples PRIVATE Tephra)

target_compile_features(TephraExamples PRIVATE cxx_std_17)
