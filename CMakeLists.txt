cmake_minimum_required(VERSION 2.8)
project(CAB_SDN)

set(CMAKE_CXX_COMPILER /etc/alternatives/g++)
set(executable_output_path ${project_binary_dir}/bin)
set(library_output_path ${project_binary_dir}/lib)

set(CMAKE_CXX_FLAGS "-std=c++0x -g")
find_package(Boost 1.55.0 COMPONENTS system thread filesystem iostreams log log_setup )

include_directories(./)
aux_source_directory(. src_list)
set(CAB BucketTree.cpp MicRuleTree.cpp RuleList.cpp OFswitch.cpp BucketTree.cpp TraceAnalyze.cpp TraceGen.cpp)
add_library(cab ${CAB})
add_executable(SimuFinal SimuFinal.cpp)
add_executable(CABDaemon CABDaemon.cpp)
add_executable(trace2pcap pcap_dump.cpp)
add_executable(testBuckeTree test_bucket_tree.cpp)
if(UNIX)
	target_link_libraries(
		SimuFinal
		cab
		${Boost_LIBRARIES}
		pthread
		z
		)
	target_link_libraries(
		CABDaemon
		cab
		${Boost_LIBRARIES}
		pthread
		)
	target_link_libraries(
		trace2pcap
		cab
		${Boost_LIBRARIES}
		pcap
		)
	target_link_libraries(
		testBuckeTree	
		cab
		${Boost_LIBRARIES}
		)
elseif(WIN32)
elseif(APPLE)
endif(UNIX)

message(STATUS "c++ flag : "${CMAKE_CXX_FLAGS}) 
message(STATUS "this is source dir "${project_source_dir})
message(STATUS "this is binary dir "${project_binary_dir})
