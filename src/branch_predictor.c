#include "branch_predictor.h"
#include <stdlib.h>
#include <string.h>

static int get_table_size(predictor_type_t type) {
    switch (type) {
        case PRED_BIMODAL_256:
        case PRED_GSHARE_256:
            return 256;
        case PRED_BIMODAL_1K:
        case PRED_GSHARE_1K:
            return 1024;
        case PRED_BIMODAL_4K:
        case PRED_GSHARE_4K:
            return 4096;
        case PRED_BIMODAL_16K:
        case PRED_GSHARE_16K:
            return 16384;
        default:
            return 0;
    }
}

static int get_history_bits(int table_size) {
    int bits = 0;
    int size = table_size;
    while (size > 1) {
        bits++;
        size >>= 1;
    }
    return bits;
}

branch_predictor_t* predictor_create(predictor_type_t type) {
    branch_predictor_t *pred = (branch_predictor_t*)malloc(sizeof(branch_predictor_t));
    if (!pred) return NULL;
    
    pred->type = type;
    pred->stats.type = type;
    pred->stats.total_branches = 0;
    pred->stats.mispredictions = 0;
    pred->table = NULL;
    pred->table_size = 0;
    pred->global_history = 0;
    pred->history_bits = 0;
    
    int table_size = get_table_size(type);
    if (table_size > 0) {
        pred->table = (uint8_t*)malloc(table_size);
        if (!pred->table) {
            free(pred);
            return NULL;
        }
        pred->table_size = table_size;
        
        memset(pred->table, 2, table_size);
        
        if (type >= PRED_GSHARE_256 && type <= PRED_GSHARE_16K) {
            pred->history_bits = get_history_bits(table_size);
        }
    }
    
    return pred;
}

void predictor_destroy(branch_predictor_t *pred) {
    if (pred) {
        if (pred->table) {
            free(pred->table);
        }
        free(pred);
    }
}

int predictor_predict(branch_predictor_t *pred, uint32_t pc, uint32_t target) {
    if (!pred || pred->type == PRED_NONE) {
        return 0;
    }
    
    switch (pred->type) {
        case PRED_NT:
            return 0;
            
        case PRED_BTFNT:
            return (target < pc) ? 1 : 0;
            
        case PRED_BIMODAL_256:
        case PRED_BIMODAL_1K:
        case PRED_BIMODAL_4K:
        case PRED_BIMODAL_16K: {
            int index = pc % pred->table_size;
            uint8_t counter = pred->table[index];
            return (counter >= 2) ? 1 : 0;
        }
            
        case PRED_GSHARE_256:
        case PRED_GSHARE_1K:
        case PRED_GSHARE_4K:
        case PRED_GSHARE_16K: {
            int index_bits = pred->history_bits;
            uint32_t pc_bits = (pc >> 2) & ((1 << index_bits) - 1);
            uint32_t hist_bits = pred->global_history & ((1 << index_bits) - 1);
            int index = (pc_bits ^ hist_bits) % pred->table_size;
            uint8_t counter = pred->table[index];
            return (counter >= 2) ? 1 : 0;
        }
            
        default:
            return 0;
    }
}

void predictor_update(branch_predictor_t *pred, uint32_t pc, uint32_t target, int taken) {
    if (!pred || pred->type == PRED_NONE) {
        return;
    }

    int prediction = predictor_predict(pred, pc, target);

    pred->stats.total_branches++;
    if (prediction != taken) {
        pred->stats.mispredictions++;
    }

    switch (pred->type) {
        case PRED_BIMODAL_256:
        case PRED_BIMODAL_1K:
        case PRED_BIMODAL_4K:
        case PRED_BIMODAL_16K: {
            int index = pc % pred->table_size;
            uint8_t *counter = &pred->table[index];
            
            if (taken) {
                if (*counter < 3) {
                    (*counter)++;
                }
            } else {
                if (*counter > 0) {
                    (*counter)--;
                }
            }
            break;
        }
            
        case PRED_GSHARE_256:
        case PRED_GSHARE_1K:
        case PRED_GSHARE_4K:
        case PRED_GSHARE_16K: {
            int index_bits = pred->history_bits;
            uint32_t pc_bits = (pc >> 2) & ((1 << index_bits) - 1);
            uint32_t hist_bits = pred->global_history & ((1 << index_bits) - 1);
            int index = (pc_bits ^ hist_bits) % pred->table_size;
            uint8_t *counter = &pred->table[index];
            
            if (taken) {
                if (*counter < 3) {
                    (*counter)++;
                }
            } else {
                if (*counter > 0) {
                    (*counter)--;
                }
            }
            
            pred->global_history = ((pred->global_history << 1) | (taken ? 1 : 0)) & ((1 << index_bits) - 1);
            break;
        }
            
        default:
            break;
    }
}

void predictor_print_stats(branch_predictor_t *pred) {
    if (!pred) return;
    
    printf("\n=== Branch Predictor Statistics ===\n");
    printf("Predictor: %s\n", predictor_name(pred->type));
    printf("Total branches: %lu\n", pred->stats.total_branches);
    printf("Mispredictions: %lu\n", pred->stats.mispredictions);
    
    if (pred->stats.total_branches > 0) {
        double rate = (double)pred->stats.mispredictions / pred->stats.total_branches * 100.0;
        printf("Misprediction rate: %.2f%%\n", rate);
    } else {
        printf("Misprediction rate: N/A (no branches)\n");
    }
    printf("===================================\n\n");
}

const char* predictor_name(predictor_type_t type) {
    switch (type) {
        case PRED_NONE:         return "None";
        case PRED_NT:           return "NT (Never Taken)";
        case PRED_BTFNT:        return "BTFNT (Backward Taken, Forward Not Taken)";
        case PRED_BIMODAL_256:  return "Bimodal (256 entries)";
        case PRED_BIMODAL_1K:   return "Bimodal (1024 entries)";
        case PRED_BIMODAL_4K:   return "Bimodal (4096 entries)";
        case PRED_BIMODAL_16K:  return "Bimodal (16384 entries)";
        case PRED_GSHARE_256:   return "gShare (256 entries)";
        case PRED_GSHARE_1K:    return "gShare (1024 entries)";
        case PRED_GSHARE_4K:    return "gShare (4096 entries)";
        case PRED_GSHARE_16K:   return "gShare (16384 entries)";
        default:                return "Unknown";
    }
}
