
include_directories(. ../.. ../debug ../includes ${SDL2_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(Dimension
    dimension.c nd_mem.c nd_nbic.c nd_devs.c nd_vio.c nd_rom.c 
    i860.cpp i860dis.cpp nd_sdl.c
)
