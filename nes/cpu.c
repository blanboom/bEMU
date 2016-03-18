//
// Created by Blanboom on 16/1/12.
//

#include "cpu.h"
#include "memory.h"


/* 用于获得 CPU 状态寄存器中的指定状态, 具体内容见后面的注释 */
#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_OVERFLOW  0x40
#define FLAG_NEGATIVE  0x80

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
    cpu.p  |= FLAG_INTERRUPT;
    memory_write_byte(0x4015, 0);  // APU was silenced
    cpu.pc = memory_read_word(0xfffc);
}

/* 检查并设置 Zero Flag 与 Negative Flag */
void cpu_checknz(uint8_t n)
{
    if((n >> 7) & 1) { cpu.p |= FLAG_NEGATIVE; } else { cpu.p &= !FLAG_NEGATIVE; }
    if(n == 0)       { cpu.p |= FLAG_ZERO; }     else { cpu.p &= !FLAG_ZERO; }
}

/* 修改 Flags */
void cpu_modify_flag(uint8_t flag, int value) {
    if(value) { cpu.p |= flag; }
    else      { cpu.p &= !flag; }
}

/* 栈操作 */
void cpu_stack_push_byte(uint8_t data) { memory_write_byte(0x100 + cpu.sp, data); cpu.sp -= 1; }
void cpu_stack_pushw(uint16_t data)    { memory_write_word(0x0ff + cpu.sp, data); cpu.sp -= 2; }
uint8_t  cpu_stack_popb()              { cpu.sp += 1; return memory_read_byte(0x100 + cpu.sp); }
uint16_t cpu_stack_popw()              { cpu.sp += 2; return memory_read_word(0x0ff + cpu.sp); }


/* CPU 寻址方式
 * 参考资料:
 * http://wiki.nesdev.com/w/index.php/CPU_addressing_modes
 * http://ewind.us/2015/nes-emu-5-6502-disassembler/
 * http://nicotine.knight.blog.163.com/blog/static/2692611220089705423961/
 * http://nicotine.knight.blog.163.com/blog/static/26926112200896032919/
 *
 * 程序代码参考了此处:
 * https://github.com/NJUOS/LiteNES
 */


/* 存储 CPU 经过寻址后得到的地址和该地址对应的值 */
uint16_t op_address;
uint8_t  op_value;

/* implied (1 字节)
 * 隐含寻址. 与累加器寻址类似, 不过指令所需的操作数不在 A 中, 而在其他寄存器中
 */
void cpu_addressing_implied() {}

/* accumulator (1 字节)
 * 缩写: A
 * 累加器寻址. 指令所需操作数在累加器 A 中, 无需操作数
 */
void cpu_addressing_accumulator() {}

/* immediate (2 字节)
 * 缩写: #v
 * 立即数寻址. 后面跟一个 8 位的立即数
 */
void cpu_addressing_immediate() {
    op_value = memory_read_byte(cpu.pc);
    cpu.pc++;
}

/* zeropage (2 字节)
 * 缩写: d
 * 零页寻址. 地址 00 ~ FF 为零页地址
 */
void cpu_addressing_zeropage() {
    op_address = memory_read_byte(cpu.pc);
    op_value = memory_read_byte[op_address];
    cpu.pc++;
}

/* zeropage, X-indexed (2 字节)
 * 缩写: d,x
 * 使用寄存器 X 的零页寻址. 在零页寻址的基础上, 地址与 X 中的值相加
 */
void cpu_addressing_zeropage_x() {
    op_address = (memory_read_byte(cpu.pc) + cpu.x) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
}

/* zeropage, Y-indexed (2 字节)
 * 缩写: d,y
 * 使用寄存器 Y 的零页寻址. 在零页寻址的基础上, 地址与 Y 中的值相加
 */
void cpu_addressing_zeropage_y() {
    op_address = (memory_read_byte(cpu.pc) + cpu.y) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
}

/* absolute (3 字节)
 * 缩写: a
 * 直接寻址. 操作数即为内存地址, 低位在前, 高位在后
 */
void cpu_addressing_absolute() {
    op_address = memory_read_word(cpu.pc);
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
}

/* absolute, X-indexed (3 字节)
 * 缩写: a,x
 * 使用寄存器 X 的直接变址寻址. 16 位地址做为基地址, 与寄存器 X 的内容相加
 */
void cpu_addressing_absolute_x() {
    op_address = memory_read_word(cpu.pc) + cpu.x;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
}

/* absolute, Y-indexed (3 字节)
 * 缩写: a,y
 * 使用寄存器 Y 的直接变址寻址. 16 位地址做为基地址, 与寄存器 Y 的内容相加
 */
void cpu_addressing_absolute_y() {
    op_address = memory_read_word(cpu.pc) + cpu.y;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
}

/* relative (2 字节)
 * 缩写: label
 * 相对寻址. 用于条件转移指令. 指令第二字节为偏移量, 可正可负.
 */
void cpu_addressing_relative() {
    op_address = memory_read_byte(cpu.pc);
    cpu.pc++;
    if(op_address & 0x80) { op_address -= 0x100; }
    op_address += cpu.pc;
}

/* indirect (3 字节)
 * 缩写: (a)
 * 间接寻址. 对应地址内存单元中的数做为地址.
 */
void cpu_addressing_indirect() {
    uint16_t arg_addr = memory_read_word(cpu.pc);

    /* 据说这是 6502 的 Bug */
    if((arg_addr & 0xff) == 0xff) {
        // 有 Bug 的情况下
        op_address = (memory_read_byte(arg_addr & 0xff00) << 8) + memory_read_byte(arg_addr);
    } else {
        // 正常情况下
        op_address = memory_read_word(arg_addr);
    }
    cpu.pc += 2;
}

/* indirect, X-indexed (2 字节)
 * 缩写: (d,x)
 * 先变址 X 后间接寻址. 以 X 做为变址, 与基地址相加, 然后间接寻址
 */
void cpu_addressing_indirect_x() {
    uint8_t arg_addr = memory_read_byte(cpu.PC);
    op_address = (memory_read_byte((arg_addr + cpu.x + 1) & 0xff) << 8) | memory_read_byte((arg_addr + cpu.x) & 0xff);
    op_value = memory_read_byte(op_address);
    cpu.pc++;
}

/* indirect, Y-indexed (2 字节)
 * 缩写: (d),y
 * 后变址 Y 间接寻址. 对操作数中的零页地址先做一次间接寻址, 得到 16 位地址, 再与 Y 相加, 对相加后得到的地址进行直接寻址.
 */
void cpu_addressing_indirect_y() {
    uint8_t arg_addr = memory_read_byte(cpu.PC);
    op_address = (((memory_read_byte((arg_addr + 1) & 0xff) << 8) | memory_read_byte(arg_addr)) + cpu.y) & 0xffff;
    op_value = memory_read_byte(op_address);
    cpu.PC++;
}

/* CPU 指令 ************************************************************/

/* ALU ******/

void cpu_ora() {
    cpu.a |= op_value;
    cpu_checknz(cpu.a);
}

void cpu_and() {
    cpu.a &= op_value;
    cpu_checknz(cpu.a);
}

void cpu_eor() {
    cpu.a ^= op_value;
    cpu_checknz(cpu.a);
}

void cpu_asl() {
    cpu_modify_flag(FLAG_CARRY, op_value & 0x80);
    op_value <<= 1;
    cpu_checknz(op_value);
    memory_write_byte(op_address, op_value);
}

void cpu_asla() {
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x80);
    cpu.a <<= 1;
    cpu_checknz(cpu.a);
}

void cpu_rol() {
    uint8_t tmp = cpu.p | FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, op_value & 0x80);
    op_value <<= 1;
    op_value |= tmp ? 1 : 0;
    memory_write_byte(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_rola() {
    uint8_t tmp = cpu.p | FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x80);
    cpu.a <<= 1;
    cpu.a |= tmp ? 1 : 0;
    cpu_checknz(cpu.a);
}

void cpu_ror() {
    uint8_t tmp = cpu.p | FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, op_value & 0x01);
    op_value >>= 1;
    op_value |= (tmp ? 1 : 0) << 7;
    memory_write_byte(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_rora() {
    uint8_t tmp = cpu.p | FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu.a |= (tmp ? 1 : 0) << 7;
    cpu_checknz(cpu.a);
}

void cpu_lsr() {
    cpu_modify_flag(FLAG_CARRY, op_value & 0x01);
    op_value >>= 1;
    cpu_write_memory(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_lsra() {
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu_checknz(cpu.a);
}

void cpu_adc() {
    uint16_t tmp;
    tmp = op_value + cpu.a + ((cpu.p | FLAG_CARRY) ? 1 : 0);
    cpu_modify_flag(FLAG_CARRY, tmp & 0xff00);
    cpu_modify_flag(FLAG_OVERFLOW, ((cpu.a ^ op_value) & (cpu.a ^ tmp)) & 0x80);
    cpu.a = (uint8_t)(tmp & 0xff);
    cpu_checknz(cpu.a);
}

void cpu_sbc() {
    uint16_t tmp;
    tmp = cpu.a - op_value - (1 - ((cpu.p | FLAG_CARRY) ? 1 : 0));
    cpu_modify_flag(FLAG_CARRY, (tmp & 0xff00) == 0);
    cpu_modify_flag(FLAG_OVERFLOW, ((cpu.a ^ op_value) & (cpu.a ^ tmp)) & 0x80);
    cpu.a = (uint8_t)(tmp & 0xff);
    cpu_checknz(cpu.a);
}

/* Branching ******/

void cpu_bmi() { if(cpu.p | FLAG_NEGATIVE) { cpu.pc = op_address; }}
void cpu_bcs() { if(cpu.p | FLAG_CARRY) { cpu.pc = op_address; }}
void cpu_beq() { if(cpu.p | FLAG_ZERO) { cpu.pc = op_address; }}
void cpu_bvs() { if(cpu.p | FLAG_OVERFLOW) { cpu.pc = op_address; }}

void cpu_bpl() { if(!(cpu.p | FLAG_NEGATIVE)) { cpu.pc = op_address; }}
void cpu_bcc() { if(!(cpu.p | FLAG_CARRY)) { cpu.pc = op_address; }}
void cpu_bne() { if(!(cpu.p | FLAG_ZERO)) { cpu.pc = op_address; }}
void cpu_bvc() { if(!(cpu.p | FLAG_OVERFLOW)) { cpu.pc = op_address; }}

/* Comapre ******/

void cpu_bit() {
    cpu_modify_flag(FLAG_OVERFLOW, op_value & 0x40);
    cpu_modify_flag(FLAG_NEGATIVE, op_value & 0x80);
    cpu_modify_flag(FLAG_ZERO, op_value & cpu.a);
}

void cpu_cmp() {
    int tmpc = cpu.a - op_value;
    cpu_modify_flag(FLAG_CARRY, tmp >= 0);
    cpu_checknz((uint8_t)tmpc);
}

void cpu_cpx() {
    int tmpc = cpu.x - op_value;
    cpu_modify_flag(FLAG_CARRY, tmp >= 0);
    cpu_checknz((uint8_t)tmpc);
}

void cpu_cpy() {
    int tmpc = cpu.x - op_value;
    cpu_modify_flag(FLAG_CARRY, tmp >= 0);
    cpu_checknz((uint8_t)tmpc);
}

/* Flag ******/

void cpu_clc() { cpu_modify_flag(FLAG_CARRY, 0); }
void cpu_cli() { cpu_modify_flag(FLAG_INTERRUPT, 0); }
void cpu_cld() { cpu_modify_flag(FLAG_DECIMAL, 0); }
void cpu_clv() { cpu_modify_flag(FLAG_OVERFLOW, 0); }
void cpu_sec() { cpu_modify_flag(FLAG_CARRY, 1); }
void cpu_sei() { cpu_modify_flag(FLAG_INTERRUPT, 1); }
void cpu_sed() { cpu_modify_flag(FLAG_DECIMAL, 1); }

/* Inc & Dec ******/

void cpu_dec() {
    uint8_t tmp = op_value - 1;
    memory_write_byte(op_address, result);
    cpu_checknz(result);
}

void cpu_dex() {
    cpu.x--;
    cpu_checknz(cpu.x);
}

void cpu_dey() {
    cpu.y--;
    cpu_checknz(cpu.y);
}

void cpu_inc() {
    uint8_t tmp = op_value + 1;
    memory_write_byte(op_address, result);
    cpu_checknz(result);
}

void cpu_inx() {
    cpu.x++;
    cpu_checknz(cpu.x);
}

void cpu_iny() {
    cpu.y++;
    cpu_checknz(cpu.y);
}

/* Load & Store ******/

void cpu_lda() { cpu.a = op_value; cpu_checknz(cpu.a); }
void cpu_ldx() { cpu.x = op_value; cpu_checknz(cpu.x); }
void cpu_ldy() { cpu.y = op_value; cpu_checknz(cpu.y); }
void cpu_sta() { memory_write_byte(op_address, cpu.a); }
void cpu_stx() { memory_write_byte(op_address, cpu.x); }
void cpu_sty() { memory_write_byte(op_address, cpu.y); }


