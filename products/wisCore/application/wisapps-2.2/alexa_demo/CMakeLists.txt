

cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(AlexaDemo LANGUAGES C VERSION 2.1.0)
find_library(CRYPTO NAMES crypto)
find_library(SSL NAMES ssl )
find_library(CURL NAMES curl )
#find_library(ASOUND NAMES asound )
#find_library(AO NAMES ao)
#message(${AO})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src c_source_alexa_code)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/capture c_source_alexa_code)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/led c_source_alexa_code)

add_definitions(-O2 -D_LARGEFILE64_SOURCE=1)
set(TARGET_AVS alexa_run_demo)
set(c_common_source_dir ${CMAKE_SOURCE_DIR}/common/src)
set(c_common_source_code ${c_common_source_dir}/RKGpioApi.c ${c_common_source_dir}/RKIoctrlApi.c)
add_executable(${TARGET_AVS} ${c_source_alexa_code} ${c_common_source_code})

target_include_directories(${TARGET_AVS} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/capture ${CMAKE_CURRENT_SOURCE_DIR}/src/led)

target_link_libraries(${TARGET_AVS} PUBLIC avs-full nmedia avio cJSON cJSON_utils rkaudio zl380tw ${CURL} ${CRYPTO} ${SSL} ${ASOUND} ${AO} pthread m)

set_target_properties(${TARGET_AVS} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#add_dependencies(${TARGET_AVS} libavio)

#add_custom_command( TARGET ${TARGET_AVS}  PRE_BUILD 
#		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/lib/libavio* ${CMAKE_CURRENT_SOURCE_DIR}/lib 
#)


install(TARGETS ${TARGET_AVS}
	RUNTIME DESTINATION bin
	BUNDLE  DESTINATION bin
	FRAMEWORK DESTINATION lib
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib/static)

file(GLOB ALEXA_CUSTOM_LIBRARYS lib/lib*.so)

#....................NOTE.....................................

#if the cmake tools can duplicate symlink in our environment, 
#you can use the next command
#.............................................................

file(INSTALL ${ALEXA_CUSTOM_LIBRARYS} DESTINATION ${CMAKE_BINARY_DIR}/lib)


install(PROGRAMS ${ALEXA_CUSTOM_LIBRARYS} DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/sound DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/ssl DESTINATION etc)
