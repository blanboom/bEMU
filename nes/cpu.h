//
// Created by Blanboom on 16/1/12.
//

#ifndef BEMU_CPU_H
#define BEMU_CPU_H

#include <stdint.h>

void cpu_init();
void cpu_interrupt();
uint64_t cpu_clock();
void cpu_run(int cycles);

#endif //BEMU_CPU_H
