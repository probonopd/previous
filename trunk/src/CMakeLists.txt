set(SOURCES
	cart.c cfgopts.c
	configuration.c options.c  change.c
	control.c cycInt.c cycles.c dialog.c file.c
	ioMem.c ioMemTabNEXT.c memorySnapShot.c keymap.c
	m68000.c main.c
	paths.c resolution.c reset.c
	scandir.c nextMemory.c screen.c screenSnapShot.c shortcut.c
	statusbar.c str.c zip.c unzip.c utils.c 
	video.c )


# When building for Windows, define specific sources for gui and ressources
if(WIN32)
	set(GUIWIN_SOURCES gui-win/opencon.c)
	set(GUIWIN_RES gui-win/hatari-winicon.rc)
endif(WIN32)

include_directories(${CMAKE_BINARY_DIR} includes debug falcon uae-cpu
		    ${SDL_INCLUDE_DIR})

if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIR})
endif(ZLIB_FOUND)

if(PNG_FOUND)
	include_directories(${PNG_INCLUDE_DIR})
endif(PNG_FOUND)

if(X11_FOUND)
	include_directories(${X11_INCLUDE_DIR})
endif(X11_FOUND)


link_directories(debug gui-sdl uae-cpu) 

add_subdirectory(debug)
add_subdirectory(gui-sdl)
add_subdirectory(uae-cpu)

# When building for Windows, add specific sources + method to compile .rc files
if(WIN32)
	# Set a default rc compiler if it was not defined yet
	if(NOT CMAKE_RC_COMPILER)
		set(CMAKE_RC_COMPILER windres)
	endif(NOT CMAKE_RC_COMPILER)
	ENABLE_LANGUAGE(RC)
	set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -Ocoff -o <OBJECT> <SOURCE>")
	set_source_files_properties(${GUIWIN_RES} PROPERTIES LANGUAGE RC)
	add_executable(previous ${GUIWIN_RES} ${SOURCES} ${GUIWIN_SOURCES})

# Other targets, use default sources
else()
	add_executable(previous ${SOURCES})
endif()

target_link_libraries(previous Debug GuiSdl UaeCpu ${SDL_LIBRARY})

if(MATH_FOUND AND NOT APPLE)
	target_link_libraries(previous ${MATH_LIBRARY})
endif()

if(SDLMAIN_LIBRARY)
	target_link_libraries(previous ${SDLMAIN_LIBRARY})
endif(SDLMAIN_LIBRARY)

if(READLINE_FOUND)
	target_link_libraries(previous ${READLINE_LIBRARY})
endif(READLINE_FOUND)

if(ZLIB_FOUND)
	target_link_libraries(previous ${ZLIB_LIBRARY})
endif(ZLIB_FOUND)

if(PNG_FOUND)
	target_link_libraries(previous ${PNG_LIBRARY})
endif(PNG_FOUND)

if(X11_FOUND)
	target_link_libraries(previous ${X11_X11_LIB})
endif(X11_FOUND)

if(PORTAUDIO_FOUND)
	target_link_libraries(previous ${PORTAUDIO_LIBRARY})
endif(PORTAUDIO_FOUND)

if(WIN32)
	# Needed for socket() on Windows
	target_link_libraries(hatari ws2_32)
endif(WIN32)


	install(TARGETS previous RUNTIME DESTINATION ${BINDIR})
	install(FILES previous-icon.bmp DESTINATION ${DATADIR})
