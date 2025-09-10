#ifndef MICROCODE_DEFS_H
#define MICROCODE_DEFS_H

#include <stdint.h> // For uint32_t

// MCode struct as defined in docs/microcode_encoding_migration.md
typedef struct {
    uint32_t state;
    uint32_t mask;
    uint32_t jadr;
    uint32_t varSel;
    uint32_t timerSel;
    uint32_t timerLd;
    uint32_t switch_sel;
    uint32_t switch_adr;
    uint32_t state_capture;
    uint32_t var_or_timer;
    uint32_t branch;
    uint32_t forced_jmp;
    uint32_t sub;
    uint32_t rtn;
} MCode;

// Code struct as defined in docs/microcode_encoding_migration.md
typedef struct {
    union {
        MCode mcode; // Use 'mcode' to avoid conflict with struct name
        uint32_t value[14]; // Assuming 14 fields, each uint32_t
    } uword;
    uint32_t level; // Metadata for hotstate compatibility/debugging
    char *label;    // Debug label for the instruction
} Code;

#endif // MICROCODE_DEFS_H