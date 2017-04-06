
include_directories(. ../.. ../debug ../includes ${SDL2_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(GuiSdl
    dlgAbout.c dlgAdvanced.c dlgAlert.c dlgBoot.c dlgDimension.c
    dlgDiskSelect.c dlgEthernet.c dlgFileSelect.c dlgFloppy.c
    dlgKeyboard.c dlgMain.c dlgMemory.c dlgMemoryAdvanced.c
    dlgMissingFile.c dlgMouse.c dlgOpticalDisk.c dlgPrinter.c
    dlgRom.c dlgSCSI.c dlgSound.c dlgSystem.c sdlgui.c
)
