#ifndef __BRANCH_PREDICTOR_H__
#define __BRANCH_PREDICTOR_H__

#include <stdint.h>
#include <stdio.h>

typedef enum {
    PRED_NONE,          
    PRED_NT,            
    PRED_BTFNT,        
    PRED_BIMODAL_256, 
    PRED_BIMODAL_1K,
    PRED_BIMODAL_4K,
    PRED_BIMODAL_16K,
    PRED_GSHARE_256,
    PRED_GSHARE_1K,
    PRED_GSHARE_4K,
    PRED_GSHARE_16K
} predictor_type_t;

typedef struct {
    predictor_type_t type;
    uint64_t total_branches;
    uint64_t mispredictions;
} predictor_stats_t;

typedef struct {
    predictor_type_t type;
    predictor_stats_t stats;
    

    uint8_t *table;
    int table_size;
    
    uint32_t global_history;
    int history_bits;
} branch_predictor_t;

branch_predictor_t* predictor_create(predictor_type_t type);
void predictor_destroy(branch_predictor_t *pred);
int predictor_predict(branch_predictor_t *pred, uint32_t pc, uint32_t target);
void predictor_update(branch_predictor_t *pred, uint32_t pc, uint32_t target, int taken);
void predictor_print_stats(branch_predictor_t *pred);
const char* predictor_name(predictor_type_t type);

#endif
