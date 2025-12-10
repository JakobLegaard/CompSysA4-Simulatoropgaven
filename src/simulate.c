#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "simulate.h"
#include "memory.h"
#include "disassemble.h"
#include "branch_predictor.h"

static int32_t registers[32];
static uint32_t pc;
static uint32_t get_bits(uint32_t instr, int start, int end) {
    return (instr >> start) & ((1 << (end - start + 1)) - 1);
}
static int32_t sign_extend(uint32_t value, int bits) {
    if (value & (1 << (bits - 1))) {
        return value | (~0U << bits);
    }
    return value;
}
static void init_register(void) {
    for (int i = 0; i < 32; i++) {
        registers[i] = 0;
    }
}
static void write_register(uint32_t reg, int32_t value) {
    if (reg != 0 && reg < 32) {
        registers[reg] = value;
    }
}
static int32_t read_register(uint32_t reg) {
    if (reg < 32) {
        return registers[reg];
    }
    return 0;
}
static void execute_r_type(uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t funct3, uint32_t funct7) {
    int32_t val1 = read_register(rs1);
    int32_t val2 = read_register(rs2);
    uint32_t uval1 = (uint32_t)val1;
    uint32_t uval2 = (uint32_t)val2;
    int32_t result = 0;
    
    if (funct7 == 0x00) {
        switch (funct3) {
            case 0x0: result = val1 + val2; break;
            case 0x1: result = val1 << (val2 & 0x1F); break;
            case 0x2: result = (val1 < val2) ? 1 : 0; break;
            case 0x3: result = (uval1 < uval2) ? 1 : 0; break;
            case 0x4: result = val1 ^ val2; break;
            case 0x5: result = uval1 >> (val2 & 0x1F); break;
            case 0x6: result = val1 | val2; break;
            case 0x7: result = val1 & val2; break;
        }
    } else if (funct7 == 0x20) {
        switch (funct3) {
            case 0x0: result = val1 - val2; break;
            case 0x5: result = val1 >> (val2 & 0x1F); break;
        }
    } else if (funct7 == 0x01) {  
        switch (funct3) {
            case 0x0: {
                result = val1 * val2;
                break;
            }
            case 0x1: {
                int64_t prod = (int64_t)val1 * (int64_t)val2;
                result = (int32_t)(prod >> 32);
                break;
            }
            case 0x2: {
                int64_t prod = (int64_t)val1 * (uint64_t)uval2;
                result = (int32_t)(prod >> 32);
                break;
            }
            case 0x3: {
                uint64_t prod = (uint64_t)uval1 * (uint64_t)uval2;
                result = (int32_t)(prod >> 32);
                break;
            }
            case 0x4: {
                if (val2 == 0) {
                    result = -1;
                } else if (val1 == (int32_t)0x80000000 && val2 == -1) {
                    result = val1;
                } else {
                    result = val1 / val2;
                }
                break;
            }
            case 0x5: {
                if (uval2 == 0) {
                    result = (int32_t)0xFFFFFFFF;
                } else {
                    result = (int32_t)(uval1 / uval2);
                }
                break;
            }
            case 0x6: {
                if (val2 == 0) {
                    result = val1;
                } else if (val1 == (int32_t)0x80000000 && val2 == -1) {
                    result = 0;
                } else {
                    result = val1 % val2;
                }
                break;
            }
            case 0x7: {
                if (uval2 == 0) {
                    result = val1;
                } else {
                    result = (int32_t)(uval1 % uval2);
                }
                break;
            }
        }
    }
    
    write_register(rd, result);
}
static void execute_i_type_alu(uint32_t rd, uint32_t rs1, uint32_t funct3, int32_t imm, uint32_t funct7) {
    int32_t val1 = read_register(rs1);
    uint32_t uval1 = (uint32_t)val1;
    int32_t result = 0;
    
    switch (funct3) {
        case 0x0: result = val1 + imm; break;
        case 0x2: result = (val1 < imm) ? 1 : 0; break;
        case 0x3: result = (uval1 < (uint32_t)imm) ? 1 : 0; break;
        case 0x4: result = val1 ^ imm; break;
        case 0x6: result = val1 | imm; break;
        case 0x7: result = val1 & imm; break;
        case 0x1: result = val1 << (imm & 0x1F); break;
        case 0x5:
            if (funct7 == 0x00) {
                result = uval1 >> (imm & 0x1F);
            } else {
                result = val1 >> (imm & 0x1F);
            }
            break;
    }
    
    write_register(rd, result);
}
static void execute_load(struct memory *mem, uint32_t rd, uint32_t rs1, uint32_t funct3, int32_t imm) {
    int32_t base = read_register(rs1);
    uint32_t addr = (uint32_t)(base + imm);
    int32_t result = 0;
    
    switch (funct3) {
        case 0x0: {
            int8_t byte = (int8_t)memory_rd_b(mem, (int)addr);
            result = (int32_t)byte;
            break;
        }
        case 0x1: {
            int16_t half = (int16_t)memory_rd_h(mem, (int)addr);
            result = (int32_t)half;
            break;
        }
        case 0x2: {
            result = (int32_t)memory_rd_w(mem, (int)addr);
            break;
        }
        case 0x4: {
            uint8_t byte = (uint8_t)memory_rd_b(mem, (int)addr);
            result = (int32_t)byte;
            break;
        }
        case 0x5: {
            uint16_t half = (uint16_t)memory_rd_h(mem, (int)addr);
            result = (int32_t)half;
            break;
        }
    }
    
    write_register(rd, result);
}
static void execute_store(struct memory *mem, uint32_t rs1, uint32_t rs2, uint32_t funct3, int32_t imm) {
    int32_t base = read_register(rs1);
    uint32_t addr = (uint32_t)(base + imm);
    int32_t value = read_register(rs2);
    
    switch (funct3) {
        case 0x0:
            memory_wr_b(mem, (int)addr, (int)(uint8_t)value);
            break;
        case 0x1:
            memory_wr_h(mem, (int)addr, (int)(uint16_t)value);
            break;
        case 0x2:
            memory_wr_w(mem, (int)addr, (int)(uint32_t)value);
            break;
    }
}
static int execute_branch(uint32_t rs1, uint32_t rs2, uint32_t funct3, int32_t imm, uint32_t current_pc) {
    int32_t val1 = read_register(rs1);
    int32_t val2 = read_register(rs2);
    uint32_t uval1 = (uint32_t)val1;
    uint32_t uval2 = (uint32_t)val2;
    int branch_taken = 0;
    
    switch (funct3) {
        case 0x0: branch_taken = (val1 == val2); break;
        case 0x1: branch_taken = (val1 != val2); break;
        case 0x4: branch_taken = (val1 < val2); break;
        case 0x5: branch_taken = (val1 >= val2); break;
        case 0x6: branch_taken = (uval1 < uval2); break;
        case 0x7: branch_taken = (uval1 >= uval2); break;
    }
    
    if (branch_taken) {
        pc = (uint32_t)((int32_t)current_pc + imm);
        return 1;
    }
    return 0;
}
static int handle_ecall(void) {
    int32_t syscall_num = read_register(17);
    
    switch (syscall_num) {
        case 1: {
            int c = getchar();
            write_register(10, c);
            break;
        }
        case 2: {
            int c = read_register(10);
            putchar(c);
            fflush(stdout);
            break;
        }
        case 3:
        case 93:
            return 1;
        default:
            fprintf(stderr, "Unknown systemcall: %d\n", syscall_num);
            return 1;
    }
    
    return 0;
}
struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, 
                     struct symbols* symbols, branch_predictor_t *predictor) {
    struct Stat stats;
    stats.insns = 0;
    
    init_register();
    pc = (uint32_t)start_addr;

    uint32_t jump_target = 0;
    
    while (1) {
        uint32_t instr = (uint32_t)memory_rd_w(mem, (int)pc);
        uint32_t current_pc = pc;

        int is_jump_target = (current_pc == jump_target);
        
        char disassembly[256];
        disassemble(current_pc, instr, disassembly, sizeof(disassembly), symbols);
        
        uint32_t opcode = get_bits(instr, 0, 6);
        uint32_t rd = get_bits(instr, 7, 11);
        uint32_t funct3 = get_bits(instr, 12, 14);
        uint32_t rs1 = get_bits(instr, 15, 19);
        uint32_t rs2 = get_bits(instr, 20, 24);
        uint32_t funct7 = get_bits(instr, 25, 31);
        
        pc = current_pc + 4;

        int reg_written = -1;
        int32_t reg_value = 0;
        int mem_written = 0;
        uint32_t mem_addr = 0;
        int32_t mem_value = 0;
        int branch_taken = -1;

        stats.insns++;

        if (log_file) {
            if (is_jump_target) {
                fprintf(log_file, "| %ld => | %08x : %08x | %-20s |", 
                        stats.insns, current_pc, instr, disassembly);
            } else {
                fprintf(log_file, "| %ld    | %08x : %08x | %-20s |", 
                        stats.insns, current_pc, instr, disassembly);
            }
        }
        
        switch (opcode) {
            case 0x33: {
                execute_r_type(rd, rs1, rs2, funct3, funct7);
                reg_written = rd;
                reg_value = read_register(rd);
                break;
            }
            
            case 0x13: {
                int32_t imm;
                if (funct3 == 0x1 || funct3 == 0x5) {
                    imm = rs2;
                } else {
                    imm = sign_extend(get_bits(instr, 20, 31), 12);
                }
                execute_i_type_alu(rd, rs1, funct3, imm, funct7);
                reg_written = rd;
                reg_value = read_register(rd);
                break;
            }
            
            case 0x03: {
                int32_t imm = sign_extend(get_bits(instr, 20, 31), 12);
                execute_load(mem, rd, rs1, funct3, imm);
                reg_written = rd;
                reg_value = read_register(rd);
                break;
            }
            
            case 0x23: {
                int32_t imm = sign_extend((get_bits(instr, 25, 31) << 5) | get_bits(instr, 7, 11), 12);
                execute_store(mem, rs1, rs2, funct3, imm);
                mem_written = 1;
                mem_addr = (uint32_t)(read_register(rs1) + imm);
                mem_value = read_register(rs2);
                break;
            }
            
            case 0x63: {
                int32_t imm = sign_extend(
                    (get_bits(instr, 31, 31) << 12) |
                    (get_bits(instr, 7, 7) << 11) |
                    (get_bits(instr, 25, 30) << 5) |
                    (get_bits(instr, 8, 11) << 1),
                    13
                );  
                uint32_t target_addr = (uint32_t)((int32_t)current_pc + imm);
                branch_taken = execute_branch(rs1, rs2, funct3, imm, current_pc);
                if (predictor) {
                    predictor_update(predictor, current_pc, target_addr, branch_taken);
                }                
                if (branch_taken) {
                    jump_target = pc;
                }
                break;
            }
            
            case 0x6F: {
                int32_t imm = sign_extend(
                    (get_bits(instr, 31, 31) << 20) |
                    (get_bits(instr, 12, 19) << 12) |
                    (get_bits(instr, 20, 20) << 11) |
                    (get_bits(instr, 21, 30) << 1),
                    21
                );
                write_register(rd, (int32_t)(current_pc + 4));
                pc = (uint32_t)((int32_t)current_pc + imm);
                reg_written = rd;
                reg_value = read_register(rd);
                jump_target = pc;
                break;
            }
            
            case 0x67: {
                int32_t imm = sign_extend(get_bits(instr, 20, 31), 12);
                int32_t base = read_register(rs1);
                uint32_t target = (uint32_t)(base + imm);
                target &= ~1U;
                write_register(rd, (int32_t)(current_pc + 4));
                pc = target;
                reg_written = rd;
                reg_value = read_register(rd);
                jump_target = pc;
                break;
            }
            
            case 0x37: {
                uint32_t imm = get_bits(instr, 12, 31) << 12;
                write_register(rd, (int32_t)imm);
                reg_written = rd;
                reg_value = read_register(rd);
                break;
            }
            
            case 0x17: {
                uint32_t imm = get_bits(instr, 12, 31) << 12;
                write_register(rd, (int32_t)(current_pc + imm));
                reg_written = rd;
                reg_value = read_register(rd);
                break;
            }
            
            case 0x73: {
                if (instr == 0x00000073) {
                    if (handle_ecall()) {
                        if (log_file) {
                            fprintf(log_file, "\n");
                        }
                        
                        if (predictor) {
                            predictor_print_stats(predictor);
                        }
                        
                        return stats;
                    }
                }
                break;
            }
            default:
                fprintf(stderr, "Unknown instruction: 0x%08x at PC=0x%08x\n", instr, current_pc);
                if (predictor) {
                    predictor_print_stats(predictor);
                }
                
                return stats;
        }
        if (log_file) {
            if (reg_written >= 0) {
                fprintf(log_file, " R[%2d] <- %08x", reg_written, (uint32_t)reg_value);
            }
            if (mem_written) {
                if (reg_written >= 0) fprintf(log_file, " |");
                fprintf(log_file, " M[%08x] <- %08x", mem_addr, (uint32_t)mem_value);
            }
            if (branch_taken >= 0) {
                if (reg_written >= 0 || mem_written) fprintf(log_file, " |");
                fprintf(log_file, " {%c}", branch_taken ? 'T' : 'N');
            }
            fprintf(log_file, "\n");
        }
        
        if (stats.insns > 100000000) {
            fprintf(stderr, "Instruction limits reached\n");
            break;
        }
    }
  
    if (predictor) {
      predictor_print_stats(predictor);
    }
  
  return stats;
}