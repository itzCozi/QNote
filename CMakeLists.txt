#===============================================================================
# QNote - A Lightweight Notepad Clone
# CMakeLists.txt - CMake build configuration
#===============================================================================

cmake_minimum_required(VERSION 3.16)

project(QNote
    VERSION 1.0.0
    DESCRIPTION "Lightweight Notepad replacement for Windows"
    LANGUAGES CXX
)

#-------------------------------------------------------------------------------
# C++ Standard
#-------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#-------------------------------------------------------------------------------
# Windows-specific settings
#-------------------------------------------------------------------------------
if(NOT WIN32)
    message(FATAL_ERROR "QNote is a Windows-only application")
endif()

# Target Windows 10
add_definitions(-D_WIN32_WINNT=0x0A00)
add_definitions(-DWINVER=0x0A00)

# Unicode
add_definitions(-DUNICODE -D_UNICODE)

# Lean and mean Windows headers
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)

#-------------------------------------------------------------------------------
# Compiler-specific settings
#-------------------------------------------------------------------------------
if(MSVC)
    # MSVC settings
    
    # Warning level
    add_compile_options(/W3)
    
    # Runtime library (static for release, dynamic for debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Optimization for release
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    
    # Additional security flags
    add_compile_options(/GS)
    add_link_options(/DYNAMICBASE /NXCOMPAT)
    
    # Use our custom manifest instead of auto-generated one
    add_link_options(/MANIFEST:NO)
    
    # Disable specific warnings if needed
    # add_compile_options(/wd4100)  # unreferenced formal parameter
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MinGW-w64 / Clang settings
    
    # Warning level
    add_compile_options(-Wall -Wextra)
    
    # Static linking of runtime libraries
    add_link_options(-static -static-libgcc -static-libstdc++)
    
    # Optimization for release
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    
    # Strip symbols in release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
    
    # Windows subsystem
    add_link_options(-mwindows)
endif()

#-------------------------------------------------------------------------------
# Source files
#-------------------------------------------------------------------------------
set(SOURCES
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/ui/Editor.cpp
    src/ui/Dialogs.cpp
    src/ui/CaptureWindow.cpp
    src/ui/NoteListWindow.cpp
    src/core/Settings.cpp
    src/core/FileIO.cpp
    src/core/NoteStore.cpp
)

set(HEADERS
    src/app/MainWindow.h
    src/ui/Editor.h
    src/ui/Dialogs.h
    src/ui/CaptureWindow.h
    src/ui/NoteListWindow.h
    src/core/Settings.h
    src/core/FileIO.h
    src/core/NoteStore.h
    src/resources/resource.h
)

set(RESOURCES
    src/resources/resource.rc
)

#-------------------------------------------------------------------------------
# Create executable
#-------------------------------------------------------------------------------
add_executable(QNote WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

#-------------------------------------------------------------------------------
# Include directories
#-------------------------------------------------------------------------------
target_include_directories(QNote PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources
)

#-------------------------------------------------------------------------------
# Link libraries
#-------------------------------------------------------------------------------
target_link_libraries(QNote PRIVATE
    user32
    gdi32
    shell32
    comdlg32
    comctl32
    advapi32
    ole32
    dwmapi
    uxtheme
)

#-------------------------------------------------------------------------------
# Resource compiler settings
#-------------------------------------------------------------------------------
if(MSVC)
    # MSVC handles .rc files automatically
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MinGW resource compiler
    enable_language(RC)
    set(CMAKE_RC_COMPILER_INIT windres)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DGCC_WINDRES")
endif()

#-------------------------------------------------------------------------------
# Install configuration
#-------------------------------------------------------------------------------
install(TARGETS QNote
    RUNTIME DESTINATION bin
)

#-------------------------------------------------------------------------------
# CPack configuration for installers (optional)
#-------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "QNote")
set(CPACK_PACKAGE_VENDOR "Open Source")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight Notepad replacement")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)

#-------------------------------------------------------------------------------
# Output information
#-------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "QNote Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
