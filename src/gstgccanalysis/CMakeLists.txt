project(gstgccanalysis LANGUAGES CXX)
cmake_minimum_required(VERSION 3.16)

set(SOURCE_LIB src/gstgccanalysis.cpp) #src/log.cpp)
#set(HEADER_FILES include/gstgccanalysis.h include/log.h)
find_package(PkgConfig REQUIRED)



pkg_check_modules(GST REQUIRED gstreamer-1.0)
include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})
add_definitions(${GST_CFLAGS_OTHER})

pkg_check_modules(GST_RTP REQUIRED gstreamer-rtp-1.0)
include_directories(${GST_RTP_INCLUDE_DIRS})
link_directories(${GST_RTP_LIBRARY_DIRS})
add_definitions(${GST_RTP_CFLAGS_OTHER})
set(LIBS ${LIBS} ${GST_RTP_LIBRARIES} ${GST_LIBRARIES})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include_directories("./hdr")

add_library(gstgccanalysis SHARED ${SOURCE_LIB})

target_link_libraries(gstgccanalysis ${LIBS})

#set_target_properties( gstgccanalysis
#    PROPERTIES
#    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/debug/lib"
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/debug/lib"
#    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/debug/bin"
#)


#install(TARGETS gstgccanalysis DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../../plugin)
