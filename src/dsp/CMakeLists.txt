
include_directories(. ../.. ../debug ../includes ${SDL2_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(DSP
    dsp.c dsp_core.c dsp_cpu.c dsp_disasm.c
)
