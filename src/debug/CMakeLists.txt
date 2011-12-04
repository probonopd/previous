
if(READLINE_FOUND)
	include_directories(${READLINE_INCLUDE_DIR})
endif(READLINE_FOUND)

add_library(Debug
            68kDisass.c log.c debugui.c breakcond.c debugcpu.c debugInfo.c
            ${DSPDBG_C} evaluate.c profile.c symbols.c 68kDisass.c)
