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
#define FLAG_UNUSED    0x20
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
void cpu_stack_push_word(uint16_t data)    { memory_write_word(0x0ff + cpu.sp, data); cpu.sp -= 2; }
uint8_t  cpu_stack_pop_byte()              { cpu.sp += 1; return memory_read_byte(0x100 + cpu.sp); }
uint16_t cpu_stack_pop_word()              { cpu.sp += 2; return memory_read_word(0x0ff + cpu.sp); }


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
uint8_t additional_cycle;  // 对于某些寻址方式, 如果跨页访问, 需要多使用一个 CPU Cycle

/* implied (1 字节)
 * 隐含寻址. 与累加器寻址类似, 不过指令所需的操作数不在 A 中, 而在其他寄存器中
 */
void cpu_addressing_implied() { additional_cycle = 0; }

/* accumulator (1 字节)
 * 缩写: A
 * 累加器寻址. 指令所需操作数在累加器 A 中, 无需操作数
 */
void cpu_addressing_accumulator() { additional_cycle = 0; }

/* immediate (2 字节)
 * 缩写: #v
 * 立即数寻址. 后面跟一个 8 位的立即数
 */
void cpu_addressing_immediate() {
    op_value = memory_read_byte(cpu.pc);
    cpu.pc++;
    additional_cycle = 0;
}

/* zeropage (2 字节)
 * 缩写: d
 * 零页寻址. 地址 00 ~ FF 为零页地址
 */
void cpu_addressing_zeropage() {
    op_address = memory_read_byte(cpu.pc);
    op_value = memory_read_byte[op_address];
    cpu.pc++;
    additional_cycle = 0;
}

/* zeropage, X-indexed (2 字节)
 * 缩写: d,x
 * 使用寄存器 X 的零页寻址. 在零页寻址的基础上, 地址与 X 中的值相加
 */
void cpu_addressing_zeropage_x() {
    op_address = (memory_read_byte(cpu.pc) + cpu.x) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycle = 0;
}

/* zeropage, Y-indexed (2 字节)
 * 缩写: d,y
 * 使用寄存器 Y 的零页寻址. 在零页寻址的基础上, 地址与 Y 中的值相加
 */
void cpu_addressing_zeropage_y() {
    op_address = (memory_read_byte(cpu.pc) + cpu.y) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycle = 0;
}

/* absolute (3 字节)
 * 缩写: a
 * 直接寻址. 操作数即为内存地址, 低位在前, 高位在后
 */
void cpu_addressing_absolute() {
    op_address = memory_read_word(cpu.pc);
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    additional_cycle = 0;
}

/* absolute, X-indexed (3 字节)
 * 缩写: a,x
 * 使用寄存器 X 的直接变址寻址. 16 位地址做为基地址, 与寄存器 X 的内容相加
 */
void cpu_addressing_absolute_x() {
    op_address = memory_read_word(cpu.pc) + cpu.x;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    if ((op_address >> 8) != (cpu.PC >> 8)) {
        additional_cycle = 1;
    } else {
        additional_cycle = 0;
    }
}

/* absolute, Y-indexed (3 字节)
 * 缩写: a,y
 * 使用寄存器 Y 的直接变址寻址. 16 位地址做为基地址, 与寄存器 Y 的内容相加
 */
void cpu_addressing_absolute_y() {
    op_address = memory_read_word(cpu.pc) + cpu.y;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    if ((op_address >> 8) != (cpu.PC >> 8)) {
        additional_cycle = 1;
    } else {
        additional_cycle = 0;
    }
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
    if ((op_address >> 8) != (cpu.PC >> 8)) {
        additional_cycle = 1;
    } else {
        additional_cycle = 0;
    }
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
    additional_cycle = 0;
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
    additional_cycle = 0;
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
    if ((op_address >> 8) != (cpu.PC >> 8)) {
        additional_cycle = 1;
    } else {
        additional_cycle = 0;
    }
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

/* Misc ******/

void cpu_nop {}

/* Stack & Jump ******/

void cpu_pha() { cpu_stack_push_byte(cpu.a); }
void cpu_php() { cpu_stack_push_byte(cpu.p | 0x30); }
void cpu_pla() { cpu.a = cpu_stack_pop_byte(); cpu_checknz(cpu.a); }
void cpu_plp() { cpu.p = (cpu_stack_pop_byte() & 0xef) | 0x20; }
void cpu_rts() { cpu.pc = cpu_stack_pop_word() + 1; }
void cpu_rti() { cpu.p = cpu_stack_pop_byte() | FLAG_UNUSED; cpu.pc = cpu_stack_pop_word(); }
void cpu_jmp() { cpu.pc = op_address; }
void cpu_jsr() { cpu_stack_push_word(cpu.pc - 1); cpu.pc = op_address; }
void cpu_brk() {
    cpu_stack_push_word(cpu.pc - 1);
    cpu_stack_push_byte(cpu.p);
    cpu.p |= FLAG_UNISED | FLAG_BREAK;
    cpu.pc = memory_read_address(0xfffa); // NMI 中断
}

/* Transfer ******/

void cpu_tax() { cpu.x = cpu.a; cpu_checknz(cpu.x); }
void cpu_tay() { cpu.y = cpu.a; cpu_checknz(cpu.y); }
void cpu_txa() { cpu.a = cpu.x; cpu_checknz(cpu.a); }
void cpu_tya() { cpu.a = cpu.y; cpu_checknz(cpu.a); }
void cpu_tsx() { cpu.x = cpu.sp; cpu_checknz(cpu.x); }
void cpu_txs() { cpu.sp = cpu.x; }

/* Undocumented ******/

/****
 * TODO: 尚未实现
 * void cpu_lax() {
 *     cpu.a = op_value;
 *     cpu.x = op_value;
 *     cpu_checknz(cpu.a);
 * }
 *
 * void cpu_sax() {
 *    memory_write_byte(op_address, cpu.a & cpu.x);
}
*****/

/****************************************************************************************/

/* CPU 运行指定 Cycle */

void cpu_run(int cycles) {
    uint8_t opcode;
    while(cycles > 0) {
        opcode = memory_read_byte(cpu.pc++);
        switch(opcode) {
            case 0x00: //IMPLIED("BRK");
                break;
            case 0x01: //INDIRECT_X("ORA");
                break;
            case 0x02: //IMPLIED("KIL");
                break;
            case 0x03: //INDIRECT_X("SLO");
                break;
            case 0x04: //ZERO_PAGE("NOP");
                break;
            case 0x05: //ZERO_PAGE("ORA");
                break;
            case 0x06: //ZERO_PAGE("ASL");
                break;
            case 0x07: //ZERO_PAGE("SLO");
                break;
            case 0x08: //IMPLIED("PHP");
                break;
            case 0x09: //IMMEDIATE("ORA");
                break;
            case 0x0A: //ACCUMULATOR("ASL");
                break;
            case 0x0B: //IMMEDIATE("ANC");
                break;
            case 0x0C: //ABSOLUTE("NOP");
                break;
            case 0x0D: //ABSOLUTE("ORA");
                break;
            case 0x0E: //ABSOLUTE("ASL");
                break;
            case 0x0F: //ABSOLUTE("SLO");
                break;
            case 0x10: //RELATIVE("BPL");
                break;
            case 0x11: //INDIRECT_Y("ORA");
                break;
            case 0x12: //IMPLIED("KIL");
                break;
            case 0x13: //INDIRECT_Y("SLO");
                break;
            case 0x14: //ZERO_PAGE_X("NOP");
                break;
            case 0x15: //ZERO_PAGE_X("ORA");
                break;
            case 0x16: //ZERO_PAGE_X("ASL");
                break;
            case 0x17: //ZERO_PAGE_X("SLO");
                break;
            case 0x18: //IMPLIED("CLC");
                break;
            case 0x19: //ABSOLUTE_Y("ORA");
                break;
            case 0x1A: //ACCUMULATOR("NOP");
                break;
            case 0x1B: //ABSOLUTE_Y("SLO");
                break;
            case 0x1C: //ABSOLUTE_X("NOP");
                break;
            case 0x1D: //ABSOLUTE_X("ORA");
                break;
            case 0x1E: //ABSOLUTE_X("ASL");
                break;
            case 0x1F: //ABSOLUTE_X("SLO");
                break;
            case 0x20: //ABSOLUTE("JSR");
                break;
            case 0x21: //INDIRECT_X("AND");
                break;
            case 0x22: //IMPLIED("KIL");
                break;
            case 0x23: //INDIRECT_X("RLA");
                break;
            case 0x24: //ZERO_PAGE("BIT");
                break;
            case 0x25: //ZERO_PAGE("AND");
                break;
            case 0x26: //ZERO_PAGE("ROL");
                break;
            case 0x27: //ZERO_PAGE("RLA");
                break;
            case 0x28: //IMPLIED("PLP");
                break;
            case 0x29: //IMMEDIATE("AND");
                break;
            case 0x2A: //ACCUMULATOR("ROL");
                break;
            case 0x2B: //IMMEDIATE("ANC");
                break;
            case 0x2C: //ABSOLUTE("BIT");
                break;
            case 0x2D: //ABSOLUTE("AND");
                break;
            case 0x2E: //ABSOLUTE("ROL");
                break;
            case 0x2F: //ABSOLUTE("RLA");
                break;
            case 0x30: //RELATIVE("BMI");
                break;
            case 0x31: //INDIRECT_Y("AND");
                break;
            case 0x32: //IMPLIED("KIL");
                break;
            case 0x33: //INDIRECT_Y("RLA");
                break;
            case 0x34: //ZERO_PAGE_X("NOP");
                break;
            case 0x35: //ZERO_PAGE_X("AND");
                break;
            case 0x36: //ZERO_PAGE_X("ROL");
                break;
            case 0x37: //ZERO_PAGE_X("RLA");
                break;
            case 0x38: //IMPLIED("SEC");
                break;
            case 0x39: //ABSOLUTE_Y("AND");
                break;
            case 0x3A: //ACCUMULATOR("NOP");
                break;
            case 0x3B: //ABSOLUTE_Y("RLA");
                break;
            case 0x3C: //ABSOLUTE_X("NOP");
                break;
            case 0x3D: //ABSOLUTE_X("AND");
                break;
            case 0x3E: //ABSOLUTE_X("ROL");
                break;
            case 0x3F: //ABSOLUTE_X("RLA");
                break;
            case 0x40: //IMPLIED("RTI");
                break;
            case 0x41: //INDIRECT_X("EOR");
                break;
            case 0x42: //IMPLIED("KIL");
                break;
            case 0x43: //INDIRECT_X("SRE");
                break;
            case 0x44: //ZERO_PAGE("NOP");
                break;
            case 0x45: //ZERO_PAGE("EOR");
                break;
            case 0x46: //ZERO_PAGE("LSR");
                break;
            case 0x47: //ZERO_PAGE("SRE");
                break;
            case 0x48: //IMPLIED("PHA");
                break;
            case 0x49: //IMMEDIATE("EOR");
                break;
            case 0x4A: //ACCUMULATOR("LSR");
                break;
            case 0x4B: //IMMEDIATE("ALR");
                break;
            case 0x4C: //ABSOLUTE("JMP");
                break;
            case 0x4D: //ABSOLUTE("EOR");
                break;
            case 0x4E: //ABSOLUTE("LSR");
                break;
            case 0x4F: //ABSOLUTE("SRE");
                break;
            case 0x50: //RELATIVE("BVC");
                break;
            case 0x51: //INDIRECT_Y("EOR");
                break;
            case 0x52: //IMPLIED("KIL");
                break;
            case 0x53: //INDIRECT_Y("SRE");
                break;
            case 0x54: //ZERO_PAGE_X("NOP");
                break;
            case 0x55: //ZERO_PAGE_X("EOR");
                break;
            case 0x56: //ZERO_PAGE_X("LSR");
                break;
            case 0x57: //ZERO_PAGE_X("SRE");
                break;
            case 0x58: //IMPLIED("CLI");
                break;
            case 0x59: //ABSOLUTE_Y("EOR");
                break;
            case 0x5A: //ACCUMULATOR("NOP");
                break;
            case 0x5B: //ABSOLUTE_Y("SRE");
                break;
            case 0x5C: //ABSOLUTE_X("NOP");
                break;
            case 0x5D: //ABSOLUTE_X("EOR");
                break;
            case 0x5E: //ABSOLUTE_X("LSR");
                break;
            case 0x5F: //ABSOLUTE_X("SRE");
                break;
            case 0x60: //IMPLIED("RTS");
                break;
            case 0x61: //INDIRECT_X("ADC");
                break;
            case 0x62: //IMPLIED("KIL");
                break;
            case 0x63: //INDIRECT_X("RRA");
                break;
            case 0x64: //ZERO_PAGE("NOP");
                break;
            case 0x65: //ZERO_PAGE("ADC");
                break;
            case 0x66: //ZERO_PAGE("ROR");
                break;
            case 0x67: //ZERO_PAGE("RRA");
                break;
            case 0x68: //IMPLIED("PLA");
                break;
            case 0x69: //IMMEDIATE("ADC");
                break;
            case 0x6A: //ACCUMULATOR("ROR");
                break;
            case 0x6B: //IMMEDIATE("ARR");
                break;
            case 0x6C: //INDIRECT("JMP");
                break;
            case 0x6D: //ABSOLUTE("ADC");
                break;
            case 0x6E: //ABSOLUTE("ROR");
                break;
            case 0x6F: //ABSOLUTE("RRA");
                break;
            case 0x70: //RELATIVE("BVS");
                break;
            case 0x71: //INDIRECT_Y("ADC");
                break;
            case 0x72: //IMPLIED("KIL");
                break;
            case 0x73: //INDIRECT_Y("RRA");
                break;
            case 0x74: //ZERO_PAGE_X("NOP");
                break;
            case 0x75: //ZERO_PAGE_X("ADC");
                break;
            case 0x76: //ZERO_PAGE_X("ROR");
                break;
            case 0x77: //ZERO_PAGE_X("RRA");
                break;
            case 0x78: //IMPLIED("SEI");
                break;
            case 0x79: //ABSOLUTE_Y("ADC");
                break;
            case 0x7A: //ACCUMULATOR("NOP");
                break;
            case 0x7B: //ABSOLUTE_Y("RRA");
                break;
            case 0x7C: //ABSOLUTE_X("NOP");
                break;
            case 0x7D: //ABSOLUTE_X("ADC");
                break;
            case 0x7E: //ABSOLUTE_X("ROR");
                break;
            case 0x7F: //ABSOLUTE_X("RRA");
                break;
            case 0x80: //IMMEDIATE("NOP");
                break;
            case 0x81: //INDIRECT_X("STA");
                break;
            case 0x82: //IMMEDIATE("NOP");
                break;
            case 0x83: //INDIRECT_X("SAX");
                break;
            case 0x84: //ZERO_PAGE("STY");
                break;
            case 0x85: //ZERO_PAGE("STA");
                break;
            case 0x86: //ZERO_PAGE("STX");
                break;
            case 0x87: //ZERO_PAGE("SAX");
                break;
            case 0x88: //IMPLIED("DEY");
                break;
            case 0x89: //IMMEDIATE("NOP");
                break;
            case 0x8A: //IMPLIED("TXA");
                break;
            case 0x8B: //IMMEDIATE("XAA");
                break;
            case 0x8C: //ABSOLUTE("STY");
                break;
            case 0x8D: //ABSOLUTE("STA");
                break;
            case 0x8E: //ABSOLUTE("STX");
                break;
            case 0x8F: //ABSOLUTE("SAX");
                break;
            case 0x90: //RELATIVE("BCC");
                break;
            case 0x91: //INDIRECT_Y("STA");
                break;
            case 0x92: //IMPLIED("KIL");
                break;
            case 0x93: //INDIRECT_Y("AHX");
                break;
            case 0x94: //ZERO_PAGE_X("STY");
                break;
            case 0x95: //ZERO_PAGE_X("STA");
                break;
            case 0x96: //ZERO_PAGE_Y("STX");
                break;
            case 0x97: //ZERO_PAGE_Y("SAX");
                break;
            case 0x98: //IMPLIED("TYA");
                break;
            case 0x99: //ABSOLUTE_Y("STA");
                break;
            case 0x9A: //IMPLIED("TXS");
                break;
            case 0x9B: //ABSOLUTE_Y("TAS");
                break;
            case 0x9C: //ABSOLUTE_X("SHY");
                break;
            case 0x9D: //ABSOLUTE_X("STA");
                break;
            case 0x9E: //ABSOLUTE_Y("SHX");
                break;
            case 0x9F: //ABSOLUTE_Y("AHX");
                break;
            case 0xA0: //IMMEDIATE("LDY");
                break;
            case 0xA1: //INDIRECT_X("LDA");
                break;
            case 0xA2: //IMMEDIATE("LDX");
                break;
            case 0xA3: //INDIRECT_X("LAX");
                break;
            case 0xA4: //ZERO_PAGE("LDY");
                break;
            case 0xA5: //ZERO_PAGE("LDA");
                break;
            case 0xA6: //ZERO_PAGE("LDX");
                break;
            case 0xA7: //ZERO_PAGE("LAX");
                break;
            case 0xA8: //IMPLIED("TAY");
                break;
            case 0xA9: //IMMEDIATE("LDA");
                break;
            case 0xAA: //IMPLIED("TAX");
                break;
            case 0xAB: //IMMEDIATE("LAX");
                break;
            case 0xAC: //ABSOLUTE("LDY");
                break;
            case 0xAD: //ABSOLUTE("LDA");
                break;
            case 0xAE: //ABSOLUTE("LDX");
                break;
            case 0xAF: //ABSOLUTE("LAX");
                break;
            case 0xB0: //RELATIVE("BCS");
                break;
            case 0xB1: //INDIRECT_Y("LDA");
                break;
            case 0xB2: //IMPLIED("KIL");
                break;
            case 0xB3: //INDIRECT_Y("LAX");
                break;
            case 0xB4: //ZERO_PAGE_X("LDY");
                break;
            case 0xB5: //ZERO_PAGE_X("LDA");
                break;
            case 0xB6: //ZERO_PAGE_Y("LDX");
                break;
            case 0xB7: //ZERO_PAGE_Y("LAX");
                break;
            case 0xB8: //IMPLIED("CLV");
                break;
            case 0xB9: //ABSOLUTE_Y("LDA");
                break;
            case 0xBA: //IMPLIED("TSX");
                break;
            case 0xBB: //ABSOLUTE_Y("LAS");
                break;
            case 0xBC: //ABSOLUTE_X("LDY");
                break;
            case 0xBD: //ABSOLUTE_X("LDA");
                break;
            case 0xBE: //ABSOLUTE_Y("LDX");
                break;
            case 0xBF: //ABSOLUTE_Y("LAX");
                break;
            case 0xC0: //IMMEDIATE("CPY");
                break;
            case 0xC1: //INDIRECT_X("CMP");
                break;
            case 0xC2: //IMMEDIATE("NOP");
                break;
            case 0xC3: //INDIRECT_X("DCP");
                break;
            case 0xC4: //ZERO_PAGE("CPY");
                break;
            case 0xC5: //ZERO_PAGE("CMP");
                break;
            case 0xC6: //ZERO_PAGE("DEC");
                break;
            case 0xC7: //ZERO_PAGE("DCP");
                break;
            case 0xC8: //IMPLIED("INY");
                break;
            case 0xC9: //IMMEDIATE("CMP");
                break;
            case 0xCA: //IMPLIED("DEX");
                break;
            case 0xCB: //IMMEDIATE("AXS");
                break;
            case 0xCC: //ABSOLUTE("CPY");
                break;
            case 0xCD: //ABSOLUTE("CMP");
                break;
            case 0xCE: //ABSOLUTE("DEC");
                break;
            case 0xCF: //ABSOLUTE("DCP");
                break;
            case 0xD0: //RELATIVE("BNE");
                break;
            case 0xD1: //INDIRECT_Y("CMP");
                break;
            case 0xD2: //IMPLIED("KIL");
                break;
            case 0xD3: //INDIRECT_Y("DCP");
                break;
            case 0xD4: //ZERO_PAGE_X("NOP");
                break;
            case 0xD5: //ZERO_PAGE_X("CMP");
                break;
            case 0xD6: //ZERO_PAGE_X("DEC");
                break;
            case 0xD7: //ZERO_PAGE_X("DCP");
                break;
            case 0xD8: //IMPLIED("CLD");
                break;
            case 0xD9: //ABSOLUTE_Y("CMP");
                break;
            case 0xDA: //ACCUMULATOR("NOP");
                break;
            case 0xDB: //ABSOLUTE_Y("DCP");
                break;
            case 0xDC: //ABSOLUTE_X("NOP");
                break;
            case 0xDD: //ABSOLUTE_X("CMP");
                break;
            case 0xDE: //ABSOLUTE_X("DEC");
                break;
            case 0xDF: //ABSOLUTE_X("DCP");
                break;
            case 0xE0: //IMMEDIATE("CPX");
                break;
            case 0xE1: //INDIRECT_X("SBC");
                break;
            case 0xE2: //IMMEDIATE("NOP");
                break;
            case 0xE3: //INDIRECT_X("ISC");
                break;
            case 0xE4: //ZERO_PAGE("CPX");
                break;
            case 0xE5: //ZERO_PAGE("SBC");
                break;
            case 0xE6: //ZERO_PAGE("INC");
                break;
            case 0xE7: //ZERO_PAGE("ISC");
                break;
            case 0xE8: //IMPLIED("INX");
                break;
            case 0xE9: //IMMEDIATE("SBC");
                break;
            case 0xEA: //ACCUMULATOR("NOP");
                break;
            case 0xEB: //IMMEDIATE("SBC");
                break;
            case 0xEC: //ABSOLUTE("CPX");
                break;
            case 0xED: //ABSOLUTE("SBC");
                break;
            case 0xEE: //ABSOLUTE("INC");
                break;
            case 0xEF: //ABSOLUTE("ISC");
                break;
            case 0xF0: //RELATIVE("BEQ");
                break;
            case 0xF1: //INDIRECT_Y("SBC");
                break;
            case 0xF2: //IMPLIED("KIL");
                break;
            case 0xF3: //INDIRECT_Y("ISC");
                break;
            case 0xF4: //ZERO_PAGE_X("NOP");
                break;
            case 0xF5: //ZERO_PAGE_X("SBC");
                break;
            case 0xF6: //ZERO_PAGE_X("INC");
                break;
            case 0xF7: //ZERO_PAGE_X("ISC");
                break;
            case 0xF8: //IMPLIED("SED");
                break;
            case 0xF9: //ABSOLUTE_Y("SBC");
                break;
            case 0xFA: //ACCUMULATOR("NOP");
                break;
            case 0xFB: //ABSOLUTE_Y("ISC");
                break;
            case 0xFC: //ABSOLUTE_X("NOP");
                break;
            case 0xFD: //ABSOLUTE_X("SBC");
                break;
            case 0xFE: //ABSOLUTE_X("INC");
                break;
            case 0xFF: //ABSOLUTE_X("ISC");
                break;
        }
    }
}
