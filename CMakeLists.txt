cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Raster)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

file(GLOB raster_files 
	"src/*.c"
	"src/*.h"
	)


add_executable(raster ${raster_files})
target_link_libraries(raster ${SDL2_LIBRARY})

