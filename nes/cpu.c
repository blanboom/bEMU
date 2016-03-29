/* 6502 CPU
 */

#include "cpu.h"
#include "memory.h"
#include "nes.h"

uint64_t cpu_cycles;

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
    cpu_cycles = 0;
    uint16_t i;
    cpu.a  = 0;
    cpu.x  = 0;
    cpu.y  = 0;
    cpu.p  = 0x24;
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
    if((n >> 7) & 1) { cpu.p |= FLAG_NEGATIVE; } else { cpu.p &= ~FLAG_NEGATIVE; }
    if(n == 0)       { cpu.p |= FLAG_ZERO; }     else { cpu.p &= ~FLAG_ZERO; }
}

/* 修改 Flags */
void cpu_modify_flag(uint8_t flag, int value) {
    if(value) { cpu.p |= flag; }
    else      { cpu.p &= ~flag; }
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
uint8_t additional_cycles;  // 对于某些寻址方式, 如果跨页访问, 需要多使用一个 CPU Cycle

/* implied (1 字节)
 * 隐含寻址. 与累加器寻址类似, 不过指令所需的操作数不在 A 中, 而在其他寄存器中
 */
void cpu_addressing_implied() { additional_cycles = 0; }

/* accumulator (1 字节)
 * 缩写: A
 * 累加器寻址. 指令所需操作数在累加器 A 中, 无需操作数
 */
void cpu_addressing_accumulator() { additional_cycles = 0; }

/* immediate (2 字节)
 * 缩写: #v
 * 立即数寻址. 后面跟一个 8 位的立即数
 */
void cpu_addressing_immediate() {
    op_value = memory_read_byte(cpu.pc);
    cpu.pc++;
    additional_cycles = 0;
}

/* zeropage (2 字节)
 * 缩写: d
 * 零页寻址. 地址 00 ~ FF 为零页地址
 */
void cpu_addressing_zeropage() {
    op_address = memory_read_byte(cpu.pc);
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycles = 0;
}

/* zeropage, X-indexed (2 字节)
 * 缩写: d,x
 * 使用寄存器 X 的零页寻址. 在零页寻址的基础上, 地址与 X 中的值相加
 */
void cpu_addressing_zeropage_x() {
    op_address = (memory_read_byte(cpu.pc) + cpu.x) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycles = 0;
}

/* zeropage, Y-indexed (2 字节)
 * 缩写: d,y
 * 使用寄存器 Y 的零页寻址. 在零页寻址的基础上, 地址与 Y 中的值相加
 */
void cpu_addressing_zeropage_y() {
    op_address = (memory_read_byte(cpu.pc) + cpu.y) & 0xff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycles = 0;
}

/* absolute (3 字节)
 * 缩写: a
 * 直接寻址. 操作数即为内存地址, 低位在前, 高位在后
 */
void cpu_addressing_absolute() {
    op_address = memory_read_word(cpu.pc);
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    additional_cycles = 0;
}

/* absolute, X-indexed (3 字节)
 * 缩写: a,x
 * 使用寄存器 X 的直接变址寻址. 16 位地址做为基地址, 与寄存器 X 的内容相加
 */
void cpu_addressing_absolute_x() {
    op_address = memory_read_word(cpu.pc) + cpu.x;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    if ((op_address >> 8) != (cpu.pc >> 8)) {
        additional_cycles = 1;
    } else {
        additional_cycles = 0;
    }
}

/* absolute, Y-indexed (3 字节)
 * 缩写: a,y
 * 使用寄存器 Y 的直接变址寻址. 16 位地址做为基地址, 与寄存器 Y 的内容相加
 */
void cpu_addressing_absolute_y() {
    op_address = (memory_read_word(cpu.pc) + cpu.y) & 0xffff;
    op_value = memory_read_byte(op_address);
    cpu.pc += 2;
    if ((op_address >> 8) != (cpu.pc >> 8)) {
        additional_cycles = 1;
    } else {
        additional_cycles = 0;
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
    if ((op_address >> 8) != (cpu.pc >> 8)) {
        additional_cycles = 1;
    } else {
        additional_cycles = 0;
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
    additional_cycles = 0;
}

/* indirect, X-indexed (2 字节)
 * 缩写: (d,x)
 * 先变址 X 后间接寻址. 以 X 做为变址, 与基地址相加, 然后间接寻址
 */
void cpu_addressing_indirect_x() {
    uint8_t arg_addr = memory_read_byte(cpu.pc);
    op_address = (memory_read_byte((arg_addr + cpu.x + 1) & 0xff) << 8) | memory_read_byte((arg_addr + cpu.x) & 0xff);
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    additional_cycles = 0;
}

/* indirect, Y-indexed (2 字节)
 * 缩写: (d),y
 * 后变址 Y 间接寻址. 对操作数中的零页地址先做一次间接寻址, 得到 16 位地址, 再与 Y 相加, 对相加后得到的地址进行直接寻址.
 */
void cpu_addressing_indirect_y() {
    uint8_t arg_addr = memory_read_byte(cpu.pc);
    op_address = (((memory_read_byte((arg_addr + 1) & 0xff) << 8) | memory_read_byte(arg_addr)) + cpu.y) & 0xffff;
    op_value = memory_read_byte(op_address);
    cpu.pc++;
    if ((op_address >> 8) != (cpu.pc >> 8)) {
        additional_cycles = 1;
    } else {
        additional_cycles = 0;
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
    uint8_t tmp = cpu.p & FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, op_value & 0x80);
    op_value <<= 1;
    op_value |= tmp ? 1 : 0;
    memory_write_byte(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_rola() {
    uint8_t tmp = cpu.p & FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x80);
    cpu.a <<= 1;
    cpu.a |= tmp ? 1 : 0;
    cpu_checknz(cpu.a);
}

void cpu_ror() {
    uint8_t tmp = cpu.p & FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, op_value & 0x01);
    op_value >>= 1;
    op_value |= (tmp ? 1 : 0) << 7;
    memory_write_byte(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_rora() {
    uint8_t tmp = cpu.p & FLAG_CARRY;
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu.a |= (tmp ? 1 : 0) << 7;
    cpu_checknz(cpu.a);
}

void cpu_lsr() {
    cpu_modify_flag(FLAG_CARRY, op_value & 0x01);
    op_value >>= 1;
    memory_write_byte(op_address, op_value);
    cpu_checknz(op_value);
}

void cpu_lsra() {
    cpu_modify_flag(FLAG_CARRY, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu_checknz(cpu.a);
}

void cpu_adc() {
    uint16_t tmp;
    tmp = op_value + cpu.a + ((cpu.p & FLAG_CARRY) ? 1 : 0);
    cpu_modify_flag(FLAG_CARRY, tmp & 0xff00);
    cpu_modify_flag(FLAG_OVERFLOW, ((op_value ^ tmp) & (cpu.a ^ tmp)) & 0x80);
    cpu.a = (uint8_t)(tmp & 0xff);
    cpu_checknz(cpu.a);
}

void cpu_sbc() {
    uint16_t tmp;
    tmp = cpu.a - op_value - (1 - ((cpu.p & FLAG_CARRY) ? 1 : 0));
    cpu_modify_flag(FLAG_CARRY, (tmp & 0xff00) == 0);
    cpu_modify_flag(FLAG_OVERFLOW, ((cpu.a ^ op_value) & (cpu.a ^ tmp)) & 0x80);
    cpu.a = (uint8_t)(tmp & 0xff);
    cpu_checknz(cpu.a);
}

/* Branching ******/

void cpu_bmi() { if(cpu.p & FLAG_NEGATIVE) { cpu.pc = op_address; }}
void cpu_bcs() { if(cpu.p & FLAG_CARRY) { cpu.pc = op_address; }}
void cpu_beq() { if(cpu.p & FLAG_ZERO) { cpu.pc = op_address; }}
void cpu_bvs() { if(cpu.p & FLAG_OVERFLOW) { cpu.pc = op_address; }}

void cpu_bpl() { if(!(cpu.p & FLAG_NEGATIVE)) { cpu.pc = op_address; }}
void cpu_bcc() { if(!(cpu.p & FLAG_CARRY)) { cpu.pc = op_address; }}
void cpu_bne() { if(!(cpu.p & FLAG_ZERO)) { cpu.pc = op_address; }}
void cpu_bvc() { if(!(cpu.p & FLAG_OVERFLOW)) { cpu.pc = op_address; }}

/* Comapre ******/

void cpu_bit() {
    cpu_modify_flag(FLAG_OVERFLOW, op_value & 0x40);
    cpu_modify_flag(FLAG_NEGATIVE, op_value & 0x80);
    cpu_modify_flag(FLAG_ZERO, !(op_value & cpu.a));
}

void cpu_cmp() {
    int tmpc = cpu.a - op_value;
    cpu_modify_flag(FLAG_CARRY, tmpc >= 0);
    cpu_checknz((uint8_t)tmpc);
}

void cpu_cpx() {
    int tmpc = cpu.x - op_value;
    cpu_modify_flag(FLAG_CARRY, tmpc >= 0);
    cpu_checknz((uint8_t)tmpc);
}

void cpu_cpy() {
    int tmpc = cpu.y - op_value;
    cpu_modify_flag(FLAG_CARRY, tmpc >= 0);
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
    memory_write_byte(op_address, tmp);
    cpu_checknz(tmp);
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
    memory_write_byte(op_address, tmp);
    cpu_checknz(tmp);
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

void cpu_nop() {}

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
    cpu.p |= FLAG_UNUSED | FLAG_BREAK;
    cpu.pc = memory_read_word(0xfffa); // NMI 中断
}

/* Transfer ******/

void cpu_tax() { cpu.x = cpu.a; cpu_checknz(cpu.x); }
void cpu_tay() { cpu.y = cpu.a; cpu_checknz(cpu.y); }
void cpu_txa() { cpu.a = cpu.x; cpu_checknz(cpu.a); }
void cpu_tya() { cpu.a = cpu.y; cpu_checknz(cpu.a); }
void cpu_tsx() { cpu.x = cpu.sp; cpu_checknz(cpu.x); }
void cpu_txs() { cpu.sp = cpu.x; }

/* Undocumented Opcodes: 未实现 ******/

/****************************************************************************************/

uint64_t cpu_clock() {
    return cpu_cycles;
}

/* CPU 运行指定 Cycle */

void cpu_run(int cycles) {
    uint8_t opcode;
    int tmp = cycles;
    while(cycles > 0) {
        // 仅供调试时使用
        // printf("PC: %x\t", cpu.pc);
        //////

        opcode = memory_read_byte(cpu.pc);

        // 仅供调试时使用
        // printf("%x\t", opcode);
        // disasm_once(cartridge.prg_rom, (cpu.pc - 0x8000) % prg_rom_size);
        //////

        cpu.pc++;

        switch(opcode) {
            /* STEP 1: 根据寻址方式取出操作数
             * STEP 2: 执行对应指令
             * STEP 3: 更新 cycles
             */
            case 0x00: cpu_addressing_implied();     cpu_brk();  cycles -= 7; break;
            case 0x01: cpu_addressing_indirect_x();  cpu_ora();  cycles -= 6; break;
            case 0x04: cpu_addressing_zeropage();    cpu_nop();  cycles -= 1; break;
            case 0x05: cpu_addressing_zeropage();    cpu_ora();  cycles -= 3; break;
            case 0x06: cpu_addressing_zeropage();    cpu_asl();  cycles -= 5; break;
            case 0x08: cpu_addressing_implied();     cpu_php();  cycles -= 3; break;
            case 0x09: cpu_addressing_immediate();   cpu_ora();  cycles -= 2; break;
            case 0x0A: cpu_addressing_accumulator(); cpu_asla(); cycles -= 2; break;
            case 0x0C: cpu_addressing_absolute();    cpu_nop();  cycles -= 1; break;
            case 0x0D: cpu_addressing_absolute();    cpu_ora();  cycles -= 4; break;
            case 0x0E: cpu_addressing_absolute();    cpu_asl();  cycles -= 6; break;
            case 0x10: cpu_addressing_relative();    cpu_bpl();  cycles -= 2; break;
            case 0x11: cpu_addressing_indirect_y();  cpu_ora();  cycles -= 5; break;
            case 0x14: cpu_addressing_zeropage_x();  cpu_nop();  cycles -= 1; break;
            case 0x15: cpu_addressing_zeropage_x();  cpu_ora();  cycles -= 4; break;
            case 0x16: cpu_addressing_zeropage_x();  cpu_asl();  cycles -= 6; break;
            case 0x18: cpu_addressing_implied();     cpu_clc();  cycles -= 2; break;
            case 0x19: cpu_addressing_absolute_y();  cpu_ora();  cycles -= 4; break;
            case 0x1A: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0x1C: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0x1D: cpu_addressing_absolute_x();  cpu_ora();  cycles -= 4; break;
            case 0x1E: cpu_addressing_absolute_x();  cpu_asl();  cycles -= 7; break;
            case 0x20: cpu_addressing_absolute();    cpu_jsr();  cycles -= 6; break;
            case 0x21: cpu_addressing_indirect_x();  cpu_and();  cycles -= 6; break;
            case 0x24: cpu_addressing_zeropage();    cpu_bit();  cycles -= 3; break;
            case 0x25: cpu_addressing_zeropage();    cpu_and();  cycles -= 3; break;
            case 0x26: cpu_addressing_zeropage();    cpu_rol();  cycles -= 5; break;
            case 0x28: cpu_addressing_implied();     cpu_plp();  cycles -= 3; break;
            case 0x29: cpu_addressing_immediate();   cpu_and();  cycles -= 2; break;
            case 0x2A: cpu_addressing_accumulator(); cpu_rola(); cycles -= 2; break;
            case 0x2C: cpu_addressing_absolute();    cpu_bit();  cycles -= 4; break;
            case 0x2D: cpu_addressing_absolute();    cpu_and();  cycles -= 2; break;
            case 0x2E: cpu_addressing_absolute();    cpu_rol();  cycles -= 6; break;
            case 0x30: cpu_addressing_relative();    cpu_bmi();  cycles -= 2; break;
            case 0x31: cpu_addressing_indirect_y();  cpu_and();  cycles -= 5; break;
            case 0x34: cpu_addressing_zeropage_x();  cpu_nop();  cycles -= 1; break;
            case 0x35: cpu_addressing_zeropage_x();  cpu_and();  cycles -= 4; break;
            case 0x36: cpu_addressing_zeropage_x();  cpu_rol();  cycles -= 6; break;
            case 0x38: cpu_addressing_implied();     cpu_sec();  cycles -= 2; break;
            case 0x39: cpu_addressing_absolute_y();  cpu_and();  cycles -= 4; break;
            case 0x3A: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0x3C: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0x3D: cpu_addressing_absolute_x();  cpu_and();  cycles -= 4; break;
            case 0x3E: cpu_addressing_absolute_x();  cpu_rol();  cycles -= 7; break;
            case 0x40: cpu_addressing_implied();     cpu_rti();  cycles -= 6; break;
            case 0x41: cpu_addressing_indirect_x();  cpu_eor();  cycles -= 6; break;
            case 0x44: cpu_addressing_zeropage();    cpu_nop();  cycles -= 1; break;
            case 0x45: cpu_addressing_zeropage();    cpu_eor();  cycles -= 3; break;
            case 0x46: cpu_addressing_zeropage();    cpu_lsr();  cycles -= 5; break;
            case 0x48: cpu_addressing_implied();     cpu_pha();  cycles -= 3; break;
            case 0x49: cpu_addressing_immediate();   cpu_eor();  cycles -= 2; break;
            case 0x4A: cpu_addressing_accumulator(); cpu_lsra(); cycles -= 2; break;
            case 0x4C: cpu_addressing_absolute();    cpu_jmp();  cycles -= 3; break;
            case 0x4D: cpu_addressing_absolute();    cpu_eor();  cycles -= 4; break;
            case 0x4E: cpu_addressing_absolute();    cpu_lsr();  cycles -= 6; break;
            case 0x50: cpu_addressing_relative();    cpu_bvc();  cycles -= 2; break;
            case 0x51: cpu_addressing_indirect_y();  cpu_eor();  cycles -= 5; break;
            case 0x54: cpu_addressing_zeropage_x();  cpu_nop();  cycles -= 1; break;
            case 0x55: cpu_addressing_zeropage_x();  cpu_eor();  cycles -= 4; break;
            case 0x56: cpu_addressing_zeropage_x();  cpu_lsr();  cycles -= 6; break;
            case 0x59: cpu_addressing_absolute_y();  cpu_eor();  cycles -= 4; break;
            case 0x5A: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0x5C: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0x5D: cpu_addressing_absolute_x();  cpu_eor();  cycles -= 4; break;
            case 0x5E: cpu_addressing_absolute_x();  cpu_lsr();  cycles -= 7; break;
            case 0x60: cpu_addressing_implied();     cpu_rts();  cycles -= 6; break;
            case 0x61: cpu_addressing_indirect_x();  cpu_adc();  cycles -= 6; break;
            case 0x64: cpu_addressing_zeropage();    cpu_nop();  cycles -= 1; break;
            case 0x65: cpu_addressing_zeropage();    cpu_adc();  cycles -= 3; break;
            case 0x66: cpu_addressing_zeropage();    cpu_ror();  cycles -= 5; break;
            case 0x68: cpu_addressing_implied();     cpu_pla();  cycles -= 4; break;
            case 0x69: cpu_addressing_immediate();   cpu_adc();  cycles -= 2; break;
            case 0x6A: cpu_addressing_accumulator(); cpu_rora(); cycles -= 2; break;
            case 0x6C: cpu_addressing_indirect();    cpu_jmp();  cycles -= 5; break;
            case 0x6D: cpu_addressing_absolute();    cpu_adc();  cycles -= 4; break;
            case 0x6E: cpu_addressing_absolute();    cpu_ror();  cycles -= 6; break;
            case 0x70: cpu_addressing_relative();    cpu_bvs();  cycles -= 2; break;
            case 0x71: cpu_addressing_indirect_y();  cpu_adc();  cycles -= 5; break;
            case 0x74: cpu_addressing_zeropage();    cpu_nop();  cycles -= 1; break;
            case 0x75: cpu_addressing_zeropage_x();  cpu_adc();  cycles -= 4; break;
            case 0x76: cpu_addressing_zeropage_x();  cpu_ror();  cycles -= 6; break;
            case 0x78: cpu_addressing_implied();     cpu_sei();  cycles -= 2; break;
            case 0x79: cpu_addressing_absolute_y();  cpu_adc();  cycles -= 4; break;
            case 0x7A: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0x7C: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0x7D: cpu_addressing_absolute_x();    cpu_adc();  cycles -= 4; break;
            case 0x7E: cpu_addressing_absolute_x();    cpu_ror();  cycles -= 7; break;
            case 0x80: cpu_addressing_immediate();   cpu_nop();  cycles -= 1; break;
            case 0x81: cpu_addressing_indirect_x();  cpu_sta();  cycles -= 6; break;
            case 0x84: cpu_addressing_zeropage();    cpu_sty();  cycles -= 3; break;
            case 0x85: cpu_addressing_zeropage();    cpu_sta();  cycles -= 3; break;
            case 0x86: cpu_addressing_zeropage();    cpu_stx();  cycles -= 3; break;
            case 0x88: cpu_addressing_implied();     cpu_dey();  cycles -= 2; break;
            case 0x8A: cpu_addressing_implied();     cpu_txa();  cycles -= 2; break;
            case 0x8C: cpu_addressing_absolute();    cpu_sty();  cycles -= 4; break;
            case 0x8D: cpu_addressing_absolute();    cpu_sta();  cycles -= 4; break;
            case 0x8E: cpu_addressing_absolute();    cpu_stx();  cycles -= 4; break;
            case 0x90: cpu_addressing_relative();    cpu_bcc();  cycles -= 2; break;
            case 0x91: cpu_addressing_indirect_y();  cpu_sta();  cycles -= 6; break;
            case 0x94: cpu_addressing_zeropage_x();  cpu_sty();  cycles -= 4; break;
            case 0x95: cpu_addressing_zeropage_x();  cpu_sta();  cycles -= 4; break;
            case 0x96: cpu_addressing_zeropage_y();  cpu_stx();  cycles -= 4; break;
            case 0x98: cpu_addressing_implied();     cpu_tya();  cycles -= 2; break;
            case 0x99: cpu_addressing_absolute_y();    cpu_sta();  cycles -= 5; break;
            case 0x9A: cpu_addressing_implied();     cpu_txs();  cycles -= 2; break;
            case 0x9D: cpu_addressing_absolute_x();  cpu_sta();  cycles -= 5; break;
            case 0xA0: cpu_addressing_immediate();   cpu_ldy();  cycles -= 2; break;
            case 0xA1: cpu_addressing_indirect_x();  cpu_lda();  cycles -= 6; break;
            case 0xA2: cpu_addressing_immediate();   cpu_ldx();  cycles -= 2; break;
            case 0xA4: cpu_addressing_zeropage();    cpu_ldy();  cycles -= 3; break;
            case 0xA5: cpu_addressing_zeropage();    cpu_lda();  cycles -= 3; break;
            case 0xA6: cpu_addressing_zeropage();    cpu_ldx();  cycles -= 3; break;
            case 0xA8: cpu_addressing_implied();     cpu_tay();  cycles -= 3; break;
            case 0xA9: cpu_addressing_immediate();   cpu_lda();  cycles -= 2; break;
            case 0xAA: cpu_addressing_implied();     cpu_tax();  cycles -= 2; break;
            case 0xAC: cpu_addressing_absolute();    cpu_ldy();  cycles -= 4; break;
            case 0xAD: cpu_addressing_absolute();    cpu_lda();  cycles -= 4; break;
            case 0xAE: cpu_addressing_absolute();    cpu_ldx();  cycles -= 4; break;
            case 0xB0: cpu_addressing_relative();    cpu_bcs();  cycles -= 2; break;
            case 0xB1: cpu_addressing_indirect_y();  cpu_lda();  cycles -= 5; break;
            case 0xB4: cpu_addressing_zeropage_x();  cpu_ldy();  cycles -= 4; break;
            case 0xB5: cpu_addressing_zeropage_x();  cpu_lda();  cycles -= 4; break;
            case 0xB6: cpu_addressing_zeropage_y();  cpu_ldx();  cycles -= 4; break;
            case 0xB8: cpu_addressing_implied();     cpu_clv();  cycles -= 2; break;
            case 0xB9: cpu_addressing_absolute_y();  cpu_lda();  cycles -= 4; break;
            case 0xBA: cpu_addressing_implied();     cpu_tsx();  cycles -= 2; break;
            case 0xBC: cpu_addressing_absolute_x();  cpu_ldy();  cycles -= 4; break;
            case 0xBD: cpu_addressing_absolute_x();  cpu_lda();  cycles -= 4; break;
            case 0xBE: cpu_addressing_absolute_y();  cpu_ldx();  cycles -= 4; break;
            case 0xC0: cpu_addressing_immediate();   cpu_cpy();  cycles -= 2; break;
            case 0xC1: cpu_addressing_indirect_x();  cpu_cmp();  cycles -= 6; break;
            case 0xC4: cpu_addressing_zeropage();    cpu_cpy();  cycles -= 3; break;
            case 0xC5: cpu_addressing_zeropage();    cpu_cmp();  cycles -= 3; break;
            case 0xC6: cpu_addressing_zeropage();    cpu_dec();  cycles -= 5; break;
            case 0xC8: cpu_addressing_implied();     cpu_iny();  cycles -= 2; break;
            case 0xC9: cpu_addressing_immediate();   cpu_cmp();  cycles -= 2; break;
            case 0xCA: cpu_addressing_implied();     cpu_dex();  cycles -= 2; break;
            case 0xCC: cpu_addressing_absolute();    cpu_cpy();  cycles -= 4; break;
            case 0xCD: cpu_addressing_absolute();    cpu_cmp();  cycles -= 4; break;
            case 0xCE: cpu_addressing_absolute();    cpu_dec();  cycles -= 6; break;
            case 0xD0: cpu_addressing_relative();    cpu_bne();  cycles -= 2; break;
            case 0xD1: cpu_addressing_indirect_y();  cpu_cmp();  cycles -= 5; break;
            case 0xD4: cpu_addressing_zeropage_x();  cpu_nop();  cycles -= 1; break;
            case 0xD5: cpu_addressing_zeropage_x();  cpu_cmp();  cycles -= 5; break;
            case 0xD6: cpu_addressing_zeropage_x();  cpu_dec();  cycles -= 6; break;
            case 0xD8: cpu_addressing_implied();     cpu_cld();  cycles -= 2; break;
            case 0xD9: cpu_addressing_absolute_y();  cpu_cmp();  cycles -= 4; break;
            case 0xDA: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0xDC: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0xDD: cpu_addressing_absolute_x();  cpu_cmp();  cycles -= 4; break;
            case 0xDE: cpu_addressing_absolute_x();  cpu_dec();  cycles -= 7; break;
            case 0xE0: cpu_addressing_immediate();   cpu_cpx();  cycles -= 2; break;
            case 0xE1: cpu_addressing_indirect_x();  cpu_sbc();  cycles -= 6; break;
            case 0xE4: cpu_addressing_zeropage();    cpu_cpx();  cycles -= 3; break;
            case 0xE5: cpu_addressing_zeropage();    cpu_sbc();  cycles -= 3; break;
            case 0xE6: cpu_addressing_zeropage();    cpu_inc();  cycles -= 5; break;
            case 0xE8: cpu_addressing_implied();     cpu_inx();  cycles -= 2; break;
            case 0xE9: cpu_addressing_immediate();   cpu_sbc();  cycles -= 2; break;
            case 0xEA: cpu_addressing_accumulator(); cpu_nop();  cycles -= 2; break;
            case 0xEC: cpu_addressing_absolute();    cpu_cpx();  cycles -= 4; break;
            case 0xED: cpu_addressing_absolute();    cpu_sbc();  cycles -= 4; break;
            case 0xEE: cpu_addressing_absolute();    cpu_inc();  cycles -= 6; break;
            case 0xF0: cpu_addressing_relative();    cpu_beq();  cycles -= 2; break;
            case 0xF1: cpu_addressing_indirect_y();  cpu_sbc();  cycles -= 5; break;
            case 0xF4: cpu_addressing_zeropage_x();  cpu_nop();  cycles -= 1; break;
            case 0xF5: cpu_addressing_zeropage_x();  cpu_sbc();  cycles -= 4; break;
            case 0xF6: cpu_addressing_zeropage_x();  cpu_inc();  cycles -= 6; break;
            case 0xF8: cpu_addressing_implied();     cpu_sed();  cycles -= 2; break;
            case 0xF9: cpu_addressing_absolute_y();  cpu_sbc();  cycles -= 4; break;
            case 0xFA: cpu_addressing_accumulator(); cpu_nop();  cycles -= 1; break;
            case 0xFC: cpu_addressing_absolute_x();  cpu_nop();  cycles -= 1; break;
            case 0xFD: cpu_addressing_absolute_x();  cpu_sbc();  cycles -= 4; break;
            case 0xFE: cpu_addressing_absolute_x();  cpu_inc();  cycles -= 7; break;
            default:
                break;
        }
        cycles -= additional_cycles;
    }
    cpu_cycles += tmp - cycles;
}

void cpu_interrupt() {
    if(ppu_generate_nmi()) {
        cpu.p |= FLAG_INTERRUPT;
        cpu_stack_push_word(cpu.pc);
        cpu_stack_push_byte(cpu.p);
        cpu.pc = memory_read_word(0xfffa);
    }
}
