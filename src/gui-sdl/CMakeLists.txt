
include_directories(. ../.. ../debug ../includes ${SDL_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(GuiSdl
	dlgAbout.c dlgAlert.c dlgDevice.c dlgFileSelect.c 
	dlgMain.c dlgMemory.c
	dlgRom.c dlgScreen.c dlgSystem.c dlgKeyboard.c
	sdlgui.c
	)
