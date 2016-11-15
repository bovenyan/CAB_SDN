cmake_minimum_required(VERSION 2.8) 
project(CAB_LIB)

set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "-std=c++11 -g")

find_package(Boost 1.54.0 REQUIRED COMPONENTS system thread filesystem iostreams log log_setup)
if(Boost_FOUND)
	message(STATUS "Boost found")
	message(STATUS "BOOST_LIBRARIES: " ${Boost_LIBRARIES})
endif()

add_definitions(-DBOOST_LOG_DYN_LINK)

set(CAB Bucket.cpp MicRuleTree.cpp RuleList.cpp BucketTree.cpp)
add_library(cab ${CAB})

if(UNIX)
    target_link_libraries(
        TracePrepare
        cab
        ${Boost_LIBRARIES}
        pthread
        z
        pcap
        )
#	target_link_libraries(
#		TracePrepare_blk
#		cab
#		${Boost_LIBRARIES}
#		pthread
#		z
#		pcap
#		)
#	target_link_libraries(
#		TraceExam
#		cab
#		${Boost_LIBRARIES}
#		z
#		)
#	target_link_libraries(
#		CAB_Simu
#		cab
#		${Boost_LIBRARIES}
#		z
#		)
elseif(WIN32)
elseif(APPLE)
endif(UNIX)

message(STATUS "c++ flag : " ${CMAKE_CXX_FLAGS}) 
message(STATUS "this is source dir " ${project_source_dir})
message(STATUS "this is binary dir " ${project_binary_dir})
