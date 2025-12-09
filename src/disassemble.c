#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "disassemble.h"
#include "read_elf.h"
#include "memory.h"
#include "simulate.h"

static uint32_t get_bits(uint32_t instr, int start, int end) {
    return (instr >> start) & ((1 << (end - start + 1)) - 1);
}
static int32_t sign_extend(uint32_t value, int bits) {
    if (value & (1 << (bits - 1))) {
        return value | (~0U << bits);
    }
    return value;
}
static const char* reg_names[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};
void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    (void)symbols;
    
    uint32_t instr = instruction;
    uint32_t opcode = get_bits(instr, 0, 6);
    uint32_t rd = get_bits(instr, 7, 11);
    uint32_t funct3 = get_bits(instr, 12, 14);
    uint32_t rs1 = get_bits(instr, 15, 19);
    uint32_t rs2 = get_bits(instr, 20, 24);
    uint32_t funct7 = get_bits(instr, 25, 31);
    char temp[256];
    
    switch (opcode) {
        case 0x33: 
            if (funct7 == 0x00) {
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "add\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x1: snprintf(temp, sizeof(temp), "sll\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x2: snprintf(temp, sizeof(temp), "slt\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x3: snprintf(temp, sizeof(temp), "sltu\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x4: snprintf(temp, sizeof(temp), "xor\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x5: snprintf(temp, sizeof(temp), "srl\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x6: snprintf(temp, sizeof(temp), "or\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x7: snprintf(temp, sizeof(temp), "and\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            } else if (funct7 == 0x20) {
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "sub\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x5: snprintf(temp, sizeof(temp), "sra\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            } else if (funct7 == 0x01) { 
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "mul\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x1: snprintf(temp, sizeof(temp), "mulh\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x2: snprintf(temp, sizeof(temp), "mulhsu\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x3: snprintf(temp, sizeof(temp), "mulhu\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x4: snprintf(temp, sizeof(temp), "div\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x5: snprintf(temp, sizeof(temp), "divu\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x6: snprintf(temp, sizeof(temp), "rem\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    case 0x7: snprintf(temp, sizeof(temp), "remu\t%s,%s,%s", reg_names[rd], reg_names[rs1], reg_names[rs2]); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            } else {
                snprintf(temp, sizeof(temp), "unknown");
            }
            break;
            
        case 0x13: 
            {
                int32_t imm = sign_extend(get_bits(instr, 20, 31), 12);
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "addi\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x2: snprintf(temp, sizeof(temp), "slti\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x3: snprintf(temp, sizeof(temp), "sltiu\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x4: snprintf(temp, sizeof(temp), "xori\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x6: snprintf(temp, sizeof(temp), "ori\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x7: snprintf(temp, sizeof(temp), "andi\t%s,%s,%d", reg_names[rd], reg_names[rs1], imm); break;
                    case 0x1: snprintf(temp, sizeof(temp), "slli\t%s,%s,%d", reg_names[rd], reg_names[rs1], (int)rs2); break;
                    case 0x5:
                        if (funct7 == 0x00)
                            snprintf(temp, sizeof(temp), "srli\t%s,%s,%d", reg_names[rd], reg_names[rs1], (int)rs2);
                        else if (funct7 == 0x20)
                            snprintf(temp, sizeof(temp), "srai\t%s,%s,%d", reg_names[rd], reg_names[rs1], (int)rs2);
                        else
                            snprintf(temp, sizeof(temp), "unknown");
                        break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            }
            break;
            
        case 0x03:
            {
                int32_t imm = sign_extend(get_bits(instr, 20, 31), 12);
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "lb\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                    case 0x1: snprintf(temp, sizeof(temp), "lh\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                    case 0x2: snprintf(temp, sizeof(temp), "lw\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                    case 0x4: snprintf(temp, sizeof(temp), "lbu\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                    case 0x5: snprintf(temp, sizeof(temp), "lhu\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            }
            break;
            
        case 0x23: 
            {
                int32_t imm = sign_extend((get_bits(instr, 25, 31) << 5) | get_bits(instr, 7, 11), 12);
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "sb\t%s,%d(%s)", reg_names[rs2], imm, reg_names[rs1]); break;
                    case 0x1: snprintf(temp, sizeof(temp), "sh\t%s,%d(%s)", reg_names[rs2], imm, reg_names[rs1]); break;
                    case 0x2: snprintf(temp, sizeof(temp), "sw\t%s,%d(%s)", reg_names[rs2], imm, reg_names[rs1]); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            }
            break;
            
        case 0x63: 
            {
                int32_t imm = sign_extend(
                    (get_bits(instr, 31, 31) << 12) |
                    (get_bits(instr, 7, 7) << 11) |
                    (get_bits(instr, 25, 30) << 5) |
                    (get_bits(instr, 8, 11) << 1),
                    13
                );
                uint32_t target = addr + (uint32_t)imm;
                switch (funct3) {
                    case 0x0: snprintf(temp, sizeof(temp), "beq\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    case 0x1: snprintf(temp, sizeof(temp), "bne\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    case 0x4: snprintf(temp, sizeof(temp), "blt\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    case 0x5: snprintf(temp, sizeof(temp), "bge\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    case 0x6: snprintf(temp, sizeof(temp), "bltu\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    case 0x7: snprintf(temp, sizeof(temp), "bgeu\t%s,%s,%x", reg_names[rs1], reg_names[rs2], target); break;
                    default: snprintf(temp, sizeof(temp), "unknown"); break;
                }
            }
            break;
            
        case 0x6F:
            {
                int32_t imm = sign_extend(
                    (get_bits(instr, 31, 31) << 20) |
                    (get_bits(instr, 12, 19) << 12) |
                    (get_bits(instr, 20, 20) << 11) |
                    (get_bits(instr, 21, 30) << 1),
                    21
                );
                uint32_t target = addr + (uint32_t)imm;
                snprintf(temp, sizeof(temp), "jal\t%s,%x", reg_names[rd], target);
            }
            break;
            
        case 0x67:
            {
                int32_t imm = sign_extend(get_bits(instr, 20, 31), 12);
                snprintf(temp, sizeof(temp), "jalr\t%s,%d(%s)", reg_names[rd], imm, reg_names[rs1]);
            }
            break;
            
        case 0x37:
            {
                uint32_t imm = get_bits(instr, 12, 31) << 12;
                snprintf(temp, sizeof(temp), "lui\t%s,0x%x", reg_names[rd], imm >> 12);
            }
            break;
            
        case 0x17:
            {
                uint32_t imm = get_bits(instr, 12, 31) << 12;
                snprintf(temp, sizeof(temp), "auipc\t%s,0x%x", reg_names[rd], imm >> 12);
            }
            break;
            
        case 0x73:
            if (instr == 0x00000073) {
                snprintf(temp, sizeof(temp), "ecall");
            } else {
                snprintf(temp, sizeof(temp), "unknown");
            }
            break;
            
        default:
            snprintf(temp, sizeof(temp), "unknown");
            break;
    }
    strncpy(result, temp, buf_size - 1);
    result[buf_size - 1] = '\0';
}
