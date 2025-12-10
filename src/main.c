#include "memory.h"
#include "read_elf.h"
#include "disassemble.h"
#include "simulate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "branch_predictor.h"

void terminate(const char *error) {
  printf("%s\n", error);
  printf("RISC-V Simulator v0.11.0: Usage:\n");
  printf("  sim riscv-elf sim-options -- prog-args\n");
  printf("    sim-options: options to the simulator\n");
  printf("      sim riscv-elf -d         // disassemble text segment of riscv-elf file to stdout\n");
  printf("      sim riscv-elf -l log     // simulate and log each instruction to file 'log'\n");
  printf("      sim riscv-elf -s log     // simulate and log only summary to file 'log'\n");
  printf("      sim riscv-elf -p TYPE    // enable branch predictor (see types below)\n");
  printf("    predictor types:\n");
  printf("      NT, BTFNT, bimodal-256, bimodal-1K, bimodal-4K, bimodal-16K,\n");
  printf("      gshare-256, gshare-1K, gshare-4K, gshare-16K\n");
  printf("    prog-args: arguments to the simulated program\n");
  printf("               these arguments are provided through argv. Puts '--' in argv[0]\n");
  printf("      sim riscv-elf -- gylletank   // run riscv-elf with 'gylletank' in argv[1]\n");
  exit(-1);
}

// Helper function - grabs args to simulated program from command line and places them in simulated memory
int pass_args_to_program(struct memory* mem, int argc, char* argv[]) {
  int seperator_position = 1; // skip first, it is the path to the simulator
  int seperator_found = 0;
  while (seperator_position < argc) {
    seperator_found = strcmp(argv[seperator_position],"--") == 0;
    if (seperator_found) break;
    seperator_position++;
  }
  if (seperator_found) { // we've got args for the program!!
    // the seperator is the first arg.
    int first_arg = seperator_position;
    int num_args = argc - first_arg;
    unsigned count_addr = 0x1000000;
    unsigned argv_addr = 0x1000004;
    unsigned str_addr = argv_addr + 4 * num_args;
    memory_wr_w(mem, count_addr, num_args);
    for (int index = 0; index < num_args; ++index) {
      memory_wr_w(mem, argv_addr + 4 * index, str_addr);
      char* cp = argv[first_arg + index];
      int c;
      do {
        c = *cp++;
        memory_wr_b(mem, str_addr++, c);
      } while (c);
    }
  }
  // leave it to main to handle args before the seperator
  return seperator_position;
}

// Helper function, prints disassembly
void disassemble_to_stdout(struct memory* mem, struct program_info* prog_info, struct symbols* symbols) 
{
  const int buf_size = 100;
  char disassembly[buf_size];
  for (unsigned int addr = prog_info->text_start; addr < prog_info->text_end; addr += 4) {
    unsigned int instruction = memory_rd_w(mem, addr);
    disassemble(addr, instruction, disassembly, buf_size, symbols);
    printf("%8x : %08X       %s\n", addr, instruction, disassembly);
  }
}

predictor_type_t parse_predictor_type(const char *str) {
    if (strcmp(str, "NT") == 0) return PRED_NT;
    if (strcmp(str, "BTFNT") == 0) return PRED_BTFNT;
    if (strcmp(str, "bimodal-256") == 0) return PRED_BIMODAL_256;
    if (strcmp(str, "bimodal-1K") == 0) return PRED_BIMODAL_1K;
    if (strcmp(str, "bimodal-4K") == 0) return PRED_BIMODAL_4K;
    if (strcmp(str, "bimodal-16K") == 0) return PRED_BIMODAL_16K;
    if (strcmp(str, "gshare-256") == 0) return PRED_GSHARE_256;
    if (strcmp(str, "gshare-1K") == 0) return PRED_GSHARE_1K;
    if (strcmp(str, "gshare-4K") == 0) return PRED_GSHARE_4K;
    if (strcmp(str, "gshare-16K") == 0) return PRED_GSHARE_16K;
    return PRED_NONE;
}

int main(int argc, char *argv[])
{
  struct memory *mem = memory_create();
  branch_predictor_t *predictor = NULL;
  argc = pass_args_to_program(mem, argc, argv);
  if (argc == 2 || argc == 3 || argc == 4 || argc == 5)
  {
    FILE *log_file = NULL;
    FILE *prof_file = NULL;
    int arg_idx = 2;
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (!strcmp(argv[arg_idx], "-l") && arg_idx + 1 < argc) {
            log_file = fopen(argv[arg_idx + 1], "w");
            if (log_file == NULL) {
                terminate("Could not open logfile, terminating.");
            }
            arg_idx += 2;
        }
        else if (!strcmp(argv[arg_idx], "-s") && arg_idx + 1 < argc) {
            prof_file = fopen(argv[arg_idx + 1], "w");
            if (prof_file == NULL) {
                terminate("Could not open file for exec profile, terminating.");
            }
            arg_idx += 2;
        }
        else if (!strcmp(argv[arg_idx], "-p") && arg_idx + 1 < argc) {
            predictor_type_t type = parse_predictor_type(argv[arg_idx + 1]);
            if (type == PRED_NONE) {
                printf("Unknown predictor type: %s\n", argv[arg_idx + 1]);
                terminate("Invalid predictor type");
            }
            predictor = predictor_create(type);
            if (!predictor) {
                terminate("Could not create predictor");
            }
            arg_idx += 2;
        }
        else {
            break;
        }
    }

    struct program_info prog_info;
    int status = read_elf(mem, &prog_info, argv[1], log_file);
    if (status) exit(status);
    // The use of symbols provide for a nicer disassembly, but their us in A4 is optional,
    // so feel free to remove/ignore setup and use of symbols.
    struct symbols* symbols = symbols_read_from_elf(argv[1]);
    if (symbols == NULL) {
      exit(-1);
    }
    if (argc == 3 && !strcmp(argv[2], "-d")) {
      // disassemble text segment to stdout
      disassemble_to_stdout(mem, &prog_info, symbols);
      exit(0);
    }
    int start_addr = prog_info.start;
    clock_t before = clock();
    struct Stat stats = simulate(mem, start_addr, log_file, symbols, predictor);
    long int num_insns = stats.insns;
    clock_t after = clock();
    int ticks = after - before;
    double mips = (1.0 * num_insns * CLOCKS_PER_SEC) / ticks / 1000000;
    if (argc == 4 && !strcmp(argv[2], "-s"))
    {
      log_file = fopen(argv[3], "w");
      if (log_file == NULL)
      {
        terminate("Could not open logfile, terminating.");
      }
    }
    if (log_file)
    {
      fprintf(log_file, "\nSimulated %ld instructions in %d host ticks (%f MIPS)\n", num_insns, ticks, mips);
      fclose(log_file);
    }
    else
    {
      printf("\nSimulated %ld instructions in %d host ticks (%f MIPS)\n", num_insns, ticks, mips);
    }
    if (predictor) {
        predictor_destroy(predictor);
    }
    memory_delete(mem);
  }
  else {
    terminate("Missing operands");
    if (predictor) {
        predictor_destroy(predictor);
    }
    memory_delete(mem);
  }
}
