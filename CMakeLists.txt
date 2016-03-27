cmake_minimum_required(VERSION 3.3)
project(bEMU)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)
target_link_libraries(allegro allegro_main allegro_primitives)

set(SOURCE_FILES main.c emulator.c emulator.h nes/cpu.c nes/cpu.h nes/disassembler.c nes/disassembler.h nes/memory.c nes/memory.h nes/ppu.c nes/ppu.h nes/nes.c nes/nes.h)
add_executable(bemu ${SOURCE_FILES})
