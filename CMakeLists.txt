cmake_minimum_required(VERSION 3.3)
project(bEMU)

include_directories(/usr/local/include ${CMAKE_SOURCE_DIR})
link_directories(/usr/local/lib)

set(SOURCE_FILES main.c emulator.c emulator.h nes/cpu.c nes/cpu.h nes/disassembler.c nes/disassembler.h nes/memory.c nes/memory.h nes/ppu.c nes/ppu.h nes/nes.c nes/nes.h nes/io.c nes/io.h)
add_executable(bEMU ${SOURCE_FILES})

target_link_libraries(bEMU allegro allegro_main allegro_primitives)
