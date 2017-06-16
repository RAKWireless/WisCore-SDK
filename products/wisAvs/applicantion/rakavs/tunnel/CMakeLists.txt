cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(TUNNEL VERSION 1.0.1)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src c_source_tunnel_code)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_definitions(-O2 -D_LARGEFILE64_SOURCE=1)

set(TARGET_TUNNEL tunnel)
set(c_common_source_dir ${CMAKE_SOURCE_DIR}/common/src)

set(c_common_source_code ${c_common_source_dir}/RKIoctrlApi.c ${c_common_source_dir}/RKUartApi.c)

add_executable(${TARGET_TUNNEL} ${c_source_tunnel_code} ${c_common_source_code})

target_include_directories(${TARGET_TUNNEL} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_TUNNEL} PUBLIC pthread)

set_target_properties(${TARGET_TUNNEL} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#get_target_property(OUTPUT_THIS ${TARGET_TUNNEL} RUNTIME_OUTPUT_DIRECTORY)
#message("RUNTIME_OUTPUT_DIRECTORY=${OUTPUT_THIS}")

install(TARGETS ${TARGET_TUNNEL} 
        RUNTIME DESTINATION bin
	BUNDLE  DESTINATION bin
        LIBRARY DESTINATION lib
	FRAMEWORK DESTINATION lib
        ARCHIVE DESTINATION lib/static)
