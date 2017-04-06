
include_directories(. ../.. ../debug ../includes ${SDL2_INCLUDE_DIR}) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(SoftFloat
    softfloat.c softfloat_extension.c softfloat_decimal.c fsincos.c fyl2x.c
)
