cmake_minimum_required(VERSION 3.3)
project(bEMU)

set(SOURCE_FILES main.c nes/cpu.c nes/cpu.h nes/disassembler.c nes/disassembler.h nes/memory.c nes/memory.h nes/ppu.c nes/ppu.h)
add_executable(bEMU ${SOURCE_FILES})