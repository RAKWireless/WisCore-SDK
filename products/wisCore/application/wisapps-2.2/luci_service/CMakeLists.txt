
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(LUCI_SERVICE LANGUAGES C VERSION 1.0.1)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src c_source_luci_service_code)
find_library(LUCI uci)
find_library(UBOX ubox)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_definitions(-O2 -D_LARGEFILE64_SOURCE=1 -D_GNU_SOURCE)
set(TARGET_LUCI_SERVICE luci_service)
set(c_common_source_dir ${CMAKE_SOURCE_DIR}/common/src)
set(c_common_source_code ${c_common_source_dir}/RKIoctrlApi.c ${c_common_source_dir}/RKLog.c)

add_executable(${TARGET_LUCI_SERVICE} ${c_source_luci_service_code} ${c_common_source_code})

target_include_directories(${TARGET_LUCI_SERVICE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_LUCI_SERVICE} PUBLIC ${LUCI} ${UBOX} dl pthread)

set_target_properties(${TARGET_LUCI_SERVICE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS ${TARGET_LUCI_SERVICE} 
	RUNTIME DESTINATION bin
	BUNDLE  DESTINATION bin
	FRAMEWORK DESTINATION lib
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib/static)

