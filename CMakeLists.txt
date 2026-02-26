#===============================================================================
# QNote - A Better Notepad for Windows
# CMakeLists.txt - CMake build configuration
#===============================================================================

cmake_minimum_required(VERSION 3.16)

project(QNote
    VERSION 1.0.0
    DESCRIPTION "A Better Notepad for Windows"
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
    
    # Strip /GR from default CXX flags (we add /GR- below to disable RTTI)
    string(REGEX REPLACE "/GR " "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # Runtime library (static for release, dynamic for debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Optimization for release - maximize speed with full optimization
    # /O2 = /Os- /Ot /Oy /Ob2 /GF /Gy /Gs (maximize speed)
    # /GL = whole program optimization
    # /Gw = optimize global data
    # /Oi = enable intrinsic functions
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /Gw /Oi /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF=5 /INCREMENTAL:NO /MERGE:.rdata=.text /MERGE:.pdata=.text /EMITTOOLVERSIONINFO:NO")
    
    # Remove unreferenced COMDAT data and enable string pooling
    add_compile_options(/Zc:inline)
    
    # Disable thread-safe statics init overhead in release (single-threaded UI app)
    add_compile_options("$<$<CONFIG:Release>:/Zc:threadSafeInit->")
    
    # Security flags (debug only for size; release relies on /DYNAMICBASE /NXCOMPAT)
    add_compile_options("$<$<CONFIG:Debug>:/GS>")
    add_compile_options("$<$<CONFIG:Release>:/GS->")
    add_link_options(/DYNAMICBASE /NXCOMPAT)
    
    # Use our custom manifest instead of auto-generated one
    add_link_options(/MANIFEST:NO)
    
    # Disable RTTI (not needed - we don't use dynamic_cast or typeid)
    add_compile_options(/GR-)
    
    # Ensure no debug info in release builds
    add_link_options("$<$<CONFIG:Release>:/DEBUG:NONE>")
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MinGW-w64 / Clang settings
    
    # Warning level
    add_compile_options(-Wall -Wextra)
    
    # Static linking of runtime libraries
    add_link_options(-static -static-libgcc -static-libstdc++)
    
    # Optimization for release - maximize speed
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -ffunction-sections -fdata-sections -flto -DNDEBUG")
    
    # Strip symbols and remove unused sections in release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s -Wl,--gc-sections -flto")
    
    # Disable RTTI (not needed)
    add_compile_options(-fno-rtti)
    
    # Remove unwind tables (not needed for release)
    add_compile_options("$<$<CONFIG:Release>:-fno-unwind-tables>")
    add_compile_options("$<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>")
    
    # Windows subsystem
    add_link_options(-mwindows)
endif()

#-------------------------------------------------------------------------------
# Output directories - place exe in build directory
#-------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})

#-------------------------------------------------------------------------------
# Source files
#-------------------------------------------------------------------------------
set(SOURCES
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/ui/Editor.cpp
    src/ui/Dialogs.cpp
    src/ui/FindBar.cpp
    src/ui/CaptureWindow.cpp
    src/ui/NoteListWindow.cpp
    src/ui/LineNumbersGutter.cpp
    src/ui/TabBar.cpp
    src/ui/DocumentManager.cpp
    src/ui/SettingsWindow.cpp
    src/core/Settings.cpp
    src/core/FileIO.cpp
    src/core/NoteStore.cpp
)

set(HEADERS
    src/app/MainWindow.h
    src/ui/Editor.h
    src/ui/Dialogs.h
    src/ui/FindBar.h
    src/ui/CaptureWindow.h
    src/ui/NoteListWindow.h
    src/ui/LineNumbersGutter.h
    src/ui/TabBar.h
    src/ui/DocumentManager.h
    src/ui/SettingsWindow.h
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
    oleaut32
    dwmapi
    uxtheme
    msimg32
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
