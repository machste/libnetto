cmake_minimum_required(VERSION 3.5)

find_package(PkgConfig REQUIRED)

pkg_check_modules(MODULES REQUIRED libmasc>=1.2.0)

project(netto VERSION 1.0.0)

include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(UNIX)
    set(CMAKE_INSTALL_PREFIX /usr)
endif(UNIX)

option(DEBUG "Enable Debugging (GDB support and no optimisation)." OFF)
option(LOG_ENABLED "Use logging for more detailed information." ON)

if(DEBUG)
	add_definitions(-O0 -ggdb3)
else()
	add_definitions(-Os)
endif()
add_definitions(-Wall -Werror -std=gnu11)

file(GLOB HEADERS include/netto/*.h)
file(GLOB_RECURSE SOURCES src/*.c)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${HEADERS}")

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_options(${PROJECT_NAME} PUBLIC ${MODULES_CFLAGS_OTHER})

#configure_file(lib${PROJECT_NAME}.pc.in lib${PROJECT_NAME}.pc @ONLY)

configure_file(config.h.in ${PROJECT_BINARY_DIR}/config.h)
add_definitions("-include config.h")

# Setup examples
file(GLOB EXAMPLE_SRCS example/*.c)

foreach(EXAMPLE_SRC ${EXAMPLE_SRCS})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})

    target_include_directories(${EXAMPLE_NAME} PUBLIC ${MODULES_INCLUDE_DIRS})
    target_compile_options(${EXAMPLE_NAME} PUBLIC ${MODULES_CFLAGS_OTHER})
	target_link_libraries(${EXAMPLE_NAME} ${MODULES_LDFLAGS} ${PROJECT_NAME})

    set_target_properties(${EXAMPLE_NAME} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/example_bin)
endforeach(EXAMPLE_SRC)

