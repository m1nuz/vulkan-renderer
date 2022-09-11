cmake_minimum_required(VERSION 3.0.0)
project(VulkanRenderer VERSION 0.1.0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(xxhash)

include(CTest)
enable_testing()

add_subdirectory(Libraries/xxhash)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(FMT REQUIRED)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

function(add_shader TARGET SHADER)
    find_program(GLSLC glslc REQUIRED FATAL_ERROR)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${SHADER})
    set(current-output-path ${CMAKE_BINARY_DIR}/../Assets/Shaders/${SHADER}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
        OUTPUT ${current-output-path}
        COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
        DEPENDS ${current-shader-path}
        IMPLICIT_DEPENDS CXX ${current-shader-path}
        VERBATIM)

    # Make sure our build depends on this output.
    set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
    target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

set(APP_NAME "VulkanRenderer")

add_executable(${APP_NAME}
    Sources/main.cpp
    Sources/Journal.cpp
    Sources/Storage.cpp
    Sources/Application.cpp
    Sources/Window.cpp
    Sources/Renderer.cpp
    Sources/Vulkan.cpp
    Sources/VulkanDebug.cpp
)

add_shader(${APP_NAME} Base.vert)
add_shader(${APP_NAME} Base.frag)

target_compile_features(${APP_NAME}
    PUBLIC
    cxx_std_20
)

if(UNIX AND NOT APPLE)
    target_compile_options(${APP_NAME}
        PUBLIC
        -pthread
        -pedantic
        -Wall
        -Wextra
        -Werror
    )
    set(PLATFORM_LIBRARIES
        stdc++
        stdc++fs
    )
elseif(APPLE)
    target_compile_options(${APP_NAME}
        PUBLIC
        -pthread
        -pedantic
        -Wall
        -Wextra
        -Werror
    )

    set(PLATFORM_LIBRARIES
        -stdlib=libstdc++
        -lc++abi
    )
elseif(MSVC)
    target_compile_options(${APP_NAME}
        PUBLIC
        /W3
        /WX
    )
endif()

target_include_directories(${APP_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    ${GLFW_INCLUDE_DIRS}
)

target_link_libraries(${APP_NAME}
    PUBLIC
    fmt::fmt
    stdc++
    stdc++fs
    Threads::Threads
    Vulkan::Vulkan
    xxHash
    ${GLFW_STATIC_LIBRARIES}
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
