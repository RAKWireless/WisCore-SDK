cmake_minimum_required(VERSION 3.0)

project(libavio VERSION 1.2.1)

find_library(ASOUND_LIBRARY NAMES asound)
find_library(MAD_LIBRARY NAMES mad)
if("${MAD_LIBRARY}" STREQUAL "MAD_LIBRARY-NOTFOUND")
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmad.so)
		set(MAD_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmad.so)
	elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmad.a)
		set(MAD_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmad.a)
	elseif(EXISTS ${CMAKE_BINARY_DIR}/lib/libmad.a)
		set(MAD_LIBRARY ${CMAKE_BINARY_DIR}/lib/libmad.a)
	elseif(EXISTS ${CMAKE_BINARY_DIR}/lib/libmad.so)
		set(MAD_LIBRARY ${CMAKE_BINARY_DIR}/lib/libmad.so)
	else()
		message("libmad not be found")
	endif()
endif()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src c_source_libavio_code)

add_definitions(-Wall -O2 -g -shared -fPIC)

set(TARGET_LIBAVIO avio)

add_library(${TARGET_LIBAVIO} SHARED ${c_source_libavio_code})

target_include_directories(${TARGET_LIBAVIO} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_LIBAVIO} PUBLIC m ${ASOUND_LIBRARY} ${MAD_LIBRARY})
set_target_properties(${TARGET_LIBAVIO} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

install(TARGETS ${TARGET_LIBAVIO}
	RUNTIME DESTINATION bin
	BUNDLE  DESTINATION bin
	FRAMEWORK DESTINATION lib
	LIBRARY DESTINATION lib	
	ARCHIVE DESTINATION lib/static)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/player_io.h ${CMAKE_CURRENT_SOURCE_DIR}/src/avcodec.h DESTINATION include)


