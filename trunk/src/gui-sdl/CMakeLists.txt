
include_directories(. ../.. ../debug ../includes ${SDL_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(GuiSdl
	dlgAbout.c dlgAdvanced.c dlgAlert.c dlgBoot.c dlgDevice.c
	dlgFileSelect.c dlgHardDisk.c dlgMain.c dlgMemory.c
	dlgMissingFile.c dlgRom.c dlgScreen.c dlgSystem.c dlgKeyboard.c
	sdlgui.c
	)
