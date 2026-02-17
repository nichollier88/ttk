cmake_minimum_required(VERSION 3.10)

project(ttk C CXX)

# Options
option(IPOD "Build for iPod" OFF)
option(TTF "Enable TTF support" ON)
set(GFXLIB "SDL" CACHE STRING "Graphics library to use (SDL, hotdog, mwin)")

# Add legacy optimizations to the standard Release configuration
# CMake automatically handles -g for Debug and -O3 for Release, but we append the specific tuning flags here.
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -frename-registers -finline-functions -funroll-loops -fomit-frame-pointer")

add_compile_options(-Wall -Wno-unused -Wno-implicit -Wno-char-subscripts)

if(IPOD)
    add_definitions(-DIPOD)
endif()

# Include Directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Flex
find_package(FLEX)
if(FLEX_FOUND)
    flex_target(AppearanceScanner appearance.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
    set(FLEX_SOURCE ${FLEX_AppearanceScanner_OUTPUTS})
else()
    message(WARNING "Flex not found. Expecting lex.yy.c to exist.")
    set(FLEX_SOURCE lex.yy.c)
endif()

# Sources
set(TTK_COMMON_SOURCES
    menu.c
    icons.c
    slider.c
    imgview.c
    textarea.c
    ${FLEX_SOURCE}
    mwin-emu.c
    gradient.c
)

set(TTK_C_SOURCES
    ttk.c
    ${TTK_COMMON_SOURCES}
)

set(TTK_CPP_SOURCES
    ttk.cpp
    TWidget.cpp
    TWindow.cpp
    ${TTK_COMMON_SOURCES}
)

if(TTF)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL_ttf.c")
        list(APPEND TTK_C_SOURCES SDL_ttf.c)
        list(APPEND TTK_CPP_SOURCES SDL_ttf.c)
    else()
        message(WARNING "SDL_ttf.c not found. Disabling TTF support.")
        add_definitions(-DNO_TF)
    endif()
else()
    add_definitions(-DNO_TF)
endif()

# Check for ttkmm extension
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ttkmm.cc")
    list(APPEND TTK_CPP_SOURCES ttkmm.cc)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ttkmm.c")
    list(APPEND TTK_C_SOURCES ttkmm.c)
    list(APPEND TTK_CPP_SOURCES ttkmm.c)
endif()

# GFXLIB Configuration
if(GFXLIB STREQUAL "hotdog")
    add_definitions(-DHDOG)
    list(APPEND TTK_C_SOURCES hotdog.c)
    list(APPEND TTK_CPP_SOURCES hotdog.c)
    include_directories(../../hotdog ../../../hotdog)
    
    if(NOT IPOD)
        find_package(SDL REQUIRED)
        include_directories(${SDL_INCLUDE_DIRS})
        include_directories(${SDL_INCLUDE_DIR})
        find_package(PNG REQUIRED)
        find_package(JPEG REQUIRED)
    endif()

elseif(GFXLIB STREQUAL "SDL")
    add_definitions(-DSDL)
    list(APPEND TTK_C_SOURCES sdl.c SDL_gfxPrimitives.c SDL_gfxPrimitives_Byte.c SDL_rotozoom.c SFont.c)
    list(APPEND TTK_CPP_SOURCES sdl.c SDL_gfxPrimitives.c SDL_gfxPrimitives_Byte.c SDL_rotozoom.c SFont.c)
    
    if(IPOD)
        include_directories(../sdlincludes)
    else()
        find_package(SDL2 QUIET)
        if(SDL2_FOUND)
            message(STATUS "SDL2 found, using it.")
            include_directories(${SDL2_INCLUDE_DIRS})
            set(SDL_DEPENDENCY_LIBS ${SDL2_LIBRARIES} SDL2_image)
        else()
            find_package(SDL REQUIRED)
            message(STATUS "SDL2 not found, falling back to SDL 1.x")
            include_directories(${SDL_INCLUDE_DIR})
            set(SDL_DEPENDENCY_LIBS ${SDL_LIBRARY} SDL_image)
        endif()
    endif()

elseif(GFXLIB STREQUAL "mwin")
    add_definitions(-DMWIN)
    include_directories(../mwincludes)
    list(APPEND TTK_C_SOURCES mwin.c)
    list(APPEND TTK_CPP_SOURCES mwin.c)
endif()

# Library
add_library(ttk STATIC ${TTK_C_SOURCES})
set_target_properties(ttk PROPERTIES OUTPUT_NAME ttk-${GFXLIB})

add_library(ttk-cpp STATIC ${TTK_CPP_SOURCES})
set_target_properties(ttk-cpp PROPERTIES OUTPUT_NAME ttk-${GFXLIB}-cpp)

# Dependencies
set(TTK_LIBS "")

if(IPOD)
    file(GLOB COMMON_LIBS "../libs/common/*.a")
    list(APPEND TTK_LIBS ${COMMON_LIBS})
    list(APPEND TTK_LIBS intl)
    
    if(GFXLIB STREQUAL "hotdog")
        list(APPEND TTK_LIBS "../../../hotdog/ipod/libhotdog.a")
    elseif(GFXLIB STREQUAL "SDL")
        file(GLOB SDL_LIBS "../libs/SDL/*.a")
        list(APPEND TTK_LIBS ${SDL_LIBS})
        list(APPEND TTK_LIBS pthread m)
    elseif(GFXLIB STREQUAL "mwin")
        file(GLOB MWIN_LIBS "../libs/mwin/*.a")
        list(APPEND TTK_LIBS ${MWIN_LIBS})
        list(APPEND TTK_LIBS m)
    endif()
else()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND TTK_LIBS intl)
    endif()
    
    if(GFXLIB STREQUAL "hotdog")
        list(APPEND TTK_LIBS "../../../hotdog/x11/libhotdog.a")
        list(APPEND TTK_LIBS ${SDL_LIBRARY} SDL_image png jpeg m)
    elseif(GFXLIB STREQUAL "SDL")
        list(APPEND TTK_LIBS ${SDL_DEPENDENCY_LIBS} png jpeg)
    elseif(GFXLIB STREQUAL "mwin")
        # Find microwindows libs dynamically as per makefile logic
        file(GLOB_RECURSE MWIN_LIBS_FOUND "../microwindows*/lib*.a" "../../microwindows*/lib*.a")
        if(MWIN_LIBS_FOUND)
             list(APPEND TTK_LIBS ${MWIN_LIBS_FOUND})
        else()
             message(WARNING "Microwindows libraries not found.")
        endif()
        list(APPEND TTK_LIBS X11 png jpeg m)
    endif()
endif()

# Examples
set(EXAMPLES exscroll exmenu eximage exti)
foreach(EX ${EXAMPLES})
    add_executable(${EX} ${EX}.c)
    target_link_libraries(${EX} ttk ${TTK_LIBS})
endforeach()