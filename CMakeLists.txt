cmake_minimum_required(VERSION 3.10)

project(ttk_root)

option(BUILD_LNDIR "Build lndir utility" OFF)

add_subdirectory(src)

if(BUILD_LNDIR)
    add_executable(lndir lndir.c)
endif()