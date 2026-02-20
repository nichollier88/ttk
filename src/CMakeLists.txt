cmake_minimum_required(VERSION 3.10)

project(ttk C CXX)

# Options
option(IPOD "Build for iPod" OFF)
option(TTF "Enable TTF support" ON)
set(GFXLIB "waveshare" CACHE STRING "Graphics library to use (SDL, hotdog, mwin, waveshare)")
# set(GFXLIB "hotdog" CACHE STRING "Graphics library to use (SDL, hotdog, mwin)")

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
set(TTK_SOURCES
    ttk_core.c
    ttk_window.c
    ttk_widget.c
    ttk_font.c
    ttk_input.c
    ttk_timer.c
    ttk_hw.c
    ttk_priv.h
    menu.c
    icons.c
    slider.c
    imgview.c
    textarea.c
    ${FLEX_SOURCE}
    mwin-emu.c
    gradient.c
)

if(TTF)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL_ttf.c")
        list(APPEND TTK_SOURCES SDL_ttf.c)
    else()
        message(WARNING "SDL_ttf.c not found. Disabling TTF support.")
        add_definitions(-DNO_TF)
    endif()
else()
    add_definitions(-DNO_TF)
endif()

# Check for ttkmm extension
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ttkmm.cc")
    list(APPEND TTK_SOURCES ttkmm.cc)
else()
    list(APPEND TTK_SOURCES ttkmm.c)
endif()

# GFXLIB Configuration
if(GFXLIB STREQUAL "hotdog")
    add_definitions(-DHDOG)
    list(APPEND TTK_SOURCES hotdog.c)
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
    list(APPEND TTK_SOURCES 
        sdl.c 
        SDL_gfxPrimitives.c 
        SDL_gfxPrimitives_Byte.c 
        SDL_rotozoom.c 
        SFont.c)
    
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
    list(APPEND TTK_SOURCES mwin.c)

elseif(GFXLIB STREQUAL "waveshare")
    add_definitions(-DWAVESHARE)
    include_directories(waveshare)
    add_definitions(-DSDL -DUSE_DEV_LIB)
    list(APPEND TTK_SOURCES 
        waveshare.c 
        SDL_gfxPrimitives.c 
        SDL_gfxPrimitives_Byte.c 
        SDL_rotozoom.c 
        SFont.c 
        waveshare/DEV_Config.c 
        waveshare/LCD_1in44.c 
        waveshare/GUI_Paint.c 
        waveshare/GUI_BMP.c)

    if(IPOD)
        include_directories(../sdlincludes)
    else()
        find_package(SDL REQUIRED)
        include_directories(${SDL_INCLUDE_DIR})
        set(SDL_DEPENDENCY_LIBS ${SDL_LIBRARY} SDL_image)
    endif()
endif()

# Library
add_library(ttk STATIC ${TTK_SOURCES})
set_target_properties(ttk PROPERTIES OUTPUT_NAME ttk-${GFXLIB})

# Dependencies
set(TTK_LIBS "")

if(IPOD)
    file(GLOB COMMON_LIBS "../libs/common/*.a")
    list(APPEND TTK_LIBS ${COMMON_LIBS})
    list(APPEND TTK_LIBS intl)
    
    if(GFXLIB STREQUAL "hotdog")
        list(APPEND TTK_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/../../hotdog/ipod/libhotdog.a")
    elseif(GFXLIB STREQUAL "SDL")
        file(GLOB SDL_LIBS "../libs/SDL/*.a")
        list(APPEND TTK_LIBS ${SDL_LIBS})
        list(APPEND TTK_LIBS pthread m)
    elseif(GFXLIB STREQUAL "mwin")
        file(GLOB MWIN_LIBS "../libs/mwin/*.a")
        list(APPEND TTK_LIBS ${MWIN_LIBS})
        list(APPEND TTK_LIBS m)
    elseif(GFXLIB STREQUAL "waveshare")
        file(GLOB SDL_LIBS "../libs/SDL/*.a")
        list(APPEND TTK_LIBS ${SDL_LIBS} lgpio pthread m)
    endif()
else()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND TTK_LIBS intl)
    endif()
    
    if(GFXLIB STREQUAL "hotdog")
        list(APPEND TTK_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/../../hotdog/x11/libhotdog.a")
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
    elseif(GFXLIB STREQUAL "waveshare")
        list(APPEND TTK_LIBS ${SDL_DEPENDENCY_LIBS} png jpeg lgpio)
    endif()
endif()

# Examples
set(EXAMPLES exscroll exmenu eximage exti)
foreach(EX ${EXAMPLES})
    add_executable(${EX} examples/${EX}.c)
    target_link_libraries(${EX} ttk ${TTK_LIBS})
endforeach()