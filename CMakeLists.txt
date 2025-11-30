cmake_minimum_required(VERSION 3.11)
project(project VERSION 1.0.0 LANGUAGES C CXX)

# Set C/C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Platform-specific settings
if(APPLE)
    # macOS specific flags
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

# Set raylib paths
set(RAYLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/raylib)
set(RAYLIB_INCLUDE_DIR ${RAYLIB_DIR}/include)
set(RAYLIB_LIB_DIR ${RAYLIB_DIR}/lib)

# Collect source files
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Set output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${RAYLIB_INCLUDE_DIR}
)

# Link raylib based on platform
if(APPLE)
    # Use dynamic library on macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR}/libraylib.dylib)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )

    # Set rpath so the executable can find the dylib
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH ${RAYLIB_LIB_DIR}
        INSTALL_RPATH "@executable_path/../lib/raylib/lib"
    )
elseif(UNIX AND NOT APPLE)
    # Linux - use shared library
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR}/libraylib.so)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        m
        pthread
        dl
    )

    # Find and link OpenGL
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

    # Find and link X11 (if using desktop)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})

    # Set rpath so the executable can find libraylib.so
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH ${RAYLIB_LIB_DIR}
        INSTALL_RPATH "$ORIGIN/../lib/raylib/lib"
    )
elseif(WIN32)
    # Windows - use static library and copy DLL
    if(MINGW)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR}/libraylib.a)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR}/raylib.lib)
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        winmm
        gdi32
        opengl32
    )

    # Copy raylib.dll to output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RAYLIB_LIB_DIR}/raylib.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )

    # Set subsystem to windows to avoid console window (optional)
    # set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-missing-field-initializers  # Common with raylib structs
    )

    # Debug-specific flags
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2>
    )
endif()

# Copy resources to build directory (if you have an assets folder)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
endif()

# Optional: Create a custom target to run the application
add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running ${PROJECT_NAME}"
)
