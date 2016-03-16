//
// Created by Blanboom on 16/1/12.
//

#include "cpu.h"
#include "memory.h"

/* CPU 寻址方式
 * http://wiki.nesdev.com/w/index.php/CPU_addressing_modes
 * http://ewind.us/2015/nes-emu-5-6502-disassembler/
 * http://nicotine.knight.blog.163.com/blog/static/2692611220089705423961/
 * http://nicotine.knight.blog.163.com/blog/static/26926112200896032919/
 *
 * 寻址方式:
 * - accumulator
 *   缩写: A
 *   累加器寻址. 指令所需操作数在累加器 A 中, 无需操作数 (1 字节)
 *
 * - absolute
 *   缩写: a
 *   直接寻址. 操作数即为内存地址, 低位在前, 高位在后 (3 字节)
 *
 * - absolute, X-indexed
 *   缩写: a,x
 *   使用寄存器 X 的直接变址寻址. 16 位地址做为基地址, 与寄存器 X 的内容相加 (3 字节)
 *
 * - absolute, Y-indexed
 *   缩写: a,y
 *   使用寄存器 Y 的直接变址寻址. 16 位地址做为基地址, 与寄存器 Y 的内容相加 (3 字节)
 *
 * - immediate
 *   缩写: #v
 *   立即数寻址. 后面跟一个 8 位的立即数 (2 字节)
 *
 * - implied
 *   隐含寻址. 与累加器寻址类似, 不过指令所需的操作数不在 A 中, 而在其他寄存器中 (1 字节)
 *
 * - indirect
 *   缩写: (a)
 *   间接寻址. 对应地址内存单元中的数做为地址. (3 字节)
 *
 * - indirect, X-indexed
 *   缩写: (d,x)
 *   先变址 X 后间接寻址. 以 X 做为变址, 与基地址相加, 然后间接寻址 (2 字节)
 *
 * - indirect, Y-indexed
 *   缩写: (d),y
 *   后变址 Y 间接寻址. 对操作数中的零页地址先做一次间接寻址, 得到 16 位地址, 再与 Y 相加, 对相加后得到的地址进行直接寻址. (2 字节)
 *
 * - relative
 *   缩写: label
 *   相对寻址. 用于条件转移指令. 指令第二字节为偏移量, 可正可负. (2 字节)
 *
 * - zeropage
 *   缩写: d
 *   零页寻址. 地址 00 ~ FF 为零页地址 (2 字节)
 *
 * - zeropage, X-indexed
 *   缩写: d,x
 *   使用寄存器 X 的零页寻址. 在零页寻址的基础上, 地址与 X 中的值相加 (2 字节)
 *
 * - zeropage, Y-indexed
 *   缩写: d,y
 *   使用寄存器 Y 的零页寻址. 在零页寻址的基础上, 地址与 Y 中的值相加 (2 字节)
 */

/* 用于获得 CPU 状态寄存器中的指定状态, 具体内容见后面的注释 */
#define CARRY_FLAG     0x01
#define ZERO_FLAG      0x02
#define INTERRUPT_FLAG 0x04
#define DECIMAL_FLAG   0x08
#define BREAK_FLAG     0x10
#define OVERFLOW_FLAG  0x40
#define SIGN_FLAG      0x80

/* CPU 寄存器
 * 各个寄存器的名称已经在程序中给出
 * 状态寄存器 p 中各个位的作用:
 *    7  bit  0
 *    ---- ----
 *    NVsB DIZC
 *    |||| ||||
 *    |||| |||+- Carry: 进位标志. 加法有进位或减法**无**借位时置 1
 *    |||| ||+-- Zero: 零标志. 运算结果为 0 时, 该位置 1
 *    |||| |+--- Interrupt: 中断屏蔽标志. 为 0 的时候允许 IRQ 和 NMI, 为 1 的时候只允许 NMI
 *    |||| +---- Decimal:  十进制标志置 1 后, ADC 和 SBC 指令使用十进制表示 (在 NES 中未被使用)
 *    ||++------ B: 用于表示软件中断的状态等, 具体参考此处. http://wiki.nesdev.com/w/index.php/CPU_status_flag_behavior
 *    |+-------- Overflow: 溢出标志. 有溢出时为 1, 无溢出时为 0
 *    +--------- Negative: 负数标志. 无溢出时 1 表示结果为负, 有溢出是 1 表示结果为正
 */
struct _cpu {
    uint8_t  a;    // 累加寄存器 Accumulator
    uint8_t  x;    // 变址寄存器 Index Register X
    uint8_t  y;    // 变址寄存器 Index Register Y
    uint8_t  sp;   // 堆栈指针   Stack Pointer
    uint8_t  p;    // 状态寄存器 Status Register
    uint16_t pc;   // 程序计数器 Program Counter
} cpu;

/* 初始化 CPU */
void cpu_init() {
    // http://wiki.nesdev.com/w/index.php/CPU_power_up_state
    uint16_t i;
    cpu.a  = 0;
    cpu.x  = 0;
    cpu.y  = 0;
    cpu.p  = 0x34;
    cpu.sp = 0xfd;
    memory_write_byte(0x4017, 0); // frame irq enabled
    memory_write_byte(0x4015, 0); // all channels disabled
    for(i = 0x4017; i <= 0x400f; i++) {
        memory_write_byte(i, 0);
    }

    cpu.pc = memory_read_word(0xfffc);
}

/* CPU 复位 */
void cpu_reset() {
    cpu.sp -= 3;
    cpu.p  |= INTERRUPT_FLAG;
    memory_write_byte(0x4015, 0);  // APU was silenced
    cpu.pc = memory_read_word(0xfffc);
}

