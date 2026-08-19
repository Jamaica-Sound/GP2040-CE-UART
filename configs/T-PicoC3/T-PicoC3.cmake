# T-PicoC3 board configuration for GP2040-CE

set(PICO_BOARD t-picoc3 CACHE STRING "Board type" FORCE)

set(PICO_BOARD_HEADER_DIRS
    "${CMAKE_CURRENT_LIST_DIR}"
    CACHE STRING "Board header directories"
    FORCE
)