
include_directories(. ../.. ../debug ../includes ${SDL2_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(GuiSdl
	dlgAbout.c dlgAdvanced.c dlgAlert.c dlgBoot.c dlgDevice.c
	dlgEthernet.c dlgFileSelect.c dlgHardDisk.c dlgKeyboard.c dlgMain.c
	dlgMemory.c dlgMemoryAdvanced.c dlgMissingFile.c dlgMouse.c
	dlgOpticalDisk.c dlgRom.c dlgScreen.c dlgSystem.c
	sdlgui.c
	)
