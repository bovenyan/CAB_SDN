cmake_minimum_required(VERSION 2.8)
project(CAB_SDN)

set(CMAKE_CXX_COMPILER /etc/alternatives/g++)
set(executable_output_path ${project_binary_dir}/bin)
set(library_output_path ${project_binary_dir}/lib)

set(CMAKE_CXX_FLAGS "-std=c++11")
find_package(Boost 1.55.0 COMPONENTS system thread filesystem iostreams log log_setup)

include_directories(./)
aux_source_directory(. src_list)
add_executable(demo.out ${src_list})

if(UNIX)
	target_link_libraries(demo.out
		${Boost_LIBRARIES}
		pthread
		z
		)
elseif(WIN32)
elseif(APPLE)
endif(UNIX)

message(STATUS "c++ flag : "${CMAKE_CXX_FLAGS}) 
message(STATUS "this is source dir "${project_source_dir})
message(STATUS "this is binary dir "${project_binary_dir})
