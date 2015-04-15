CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR)
CMAKE_POLICY(VERSION 2.8)
FIND_PACKAGE(PkgConfig REQUIRED)

PROJECT(nemo_example)
SET(nemo_example_VERSION_MAJOR 0)
SET(nemo_example_VERSION_MINOR 1)

#INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")

PKG_CHECK_MODULES(PKGS REQUIRED pixman-1 cairo libpng12 harfbuzz freetype2 fontconfig nemotale nemotool)

FOREACH(i ${PKGS_CFLAGS})
    SET(CFLAGS "${CFLAGS} ${i}")
ENDFOREACH(i)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=auto -Wall -g -fPIC")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,defs -Wl,--as-needed -Wl,--hash-style=both")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

INCLUDE_DIRECTORIES(${PKGS_INCLUDE_DIRS})
LINK_DIRECTORIES(${PKGS_LIBRARY_DIRS})

ADD_LIBRARY(helper STATIC
    util.c talehelper.c cairo_view.c view.c text.c pieview.c
    )
TARGET_LINK_LIBRARIES(helper "${PKGS_LIBRARIES}" m rt)

#ADD_EXECUTABLE(nemotest nemotest.c)
#TARGET_LINK_LIBRARIES(nemotest helper "${PKGS_LIBRARIES}" m rt)

#ADD_EXECUTABLE(future future.c)
#TARGET_LINK_LIBRARIES(future helper "${PKGS_LIBRARIES}" m rt)

ADD_EXECUTABLE(cpuview cpuview.c)
TARGET_LINK_LIBRARIES(cpuview helper "${PKGS_LIBRARIES}" m rt)

#ADD_EXECUTABLE(memview memview.c)
#TARGET_LINK_LIBRARIES(memview helper "${PKGS_LIBRARIES}" m rt)

#ADD_EXECUTABLE(table table.c)
#TARGET_LINK_LIBRARIES(table helper "${PKGS_LIBRARIES}" m rt)
