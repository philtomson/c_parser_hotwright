#ifndef CFG_H
#define CFG_H

#include "ast.h"
#include <stdbool.h>

// Forward declarations
typedef struct BasicBlock BasicBlock;
typedef struct CFG CFG;
typedef struct SSAValue SSAValue;
typedef struct SSAInstruction SSAInstruction;
typedef struct PhiNode PhiNode;

// Edge types for control flow
typedef enum {
    EDGE_UNCONDITIONAL,              // Normal flow
    EDGE_TRUE_BRANCH,                // If condition true
    EDGE_FALSE_BRANCH,               // If condition false
    EDGE_LOOP_BACK,                  // Loop back edge
    EDGE_BREAK,                      // Break statement
    EDGE_RETURN                      // Return statement
} EdgeType;

// SSA instruction types
typedef enum {
    SSA_PHI,           // Phi function
    SSA_ASSIGN,        // Simple assignment
    SSA_BINARY_OP,     // Binary operation
    SSA_UNARY_OP,      // Unary operation
    SSA_LOAD,          // Load from memory
    SSA_STORE,         // Store to memory
    SSA_CALL,          // Function call
    SSA_RETURN,        // Return statement
    SSA_BRANCH,        // Conditional branch
    SSA_JUMP           // Unconditional jump
} SSAInstructionType;

// SSA value types
typedef struct SSAValue {
    enum {
        SSA_VAR,       // SSA variable (e.g., x_1, x_2)
        SSA_CONST,     // Constant value
        SSA_TEMP       // Temporary value
    } type;
    union {
        struct {
            char* base_name;    // Original variable name
            int version;        // SSA version number
        } var;
        int const_value;
        int temp_id;
    } data;
} SSAValue;

// Dynamic array of SSA instructions
typedef struct {
    SSAInstruction** items;
    int count;
    int capacity;
} InstructionList;

// Dynamic array of phi nodes
typedef struct {
    PhiNode** items;
    int count;
    int capacity;
} PhiNodeList;

// SSA instruction structure
typedef struct SSAInstruction {
    SSAInstructionType type;
    SSAValue* dest;              // Destination (if any)
    SSAValue** operands;         // Source operands
    int operand_count;
    
    // Type-specific data
    union {
        struct {
            TokenType op;        // For binary/unary ops
        } op_data;
        struct {
            char* func_name;     // For calls
        } call_data;
        struct {
            BasicBlock* true_target;
            BasicBlock* false_target;
            SSAValue* condition;
        } branch_data;
        struct {
            BasicBlock* target;
        } jump_data;
        struct {
            SSAValue* value;
        } return_data;
    } data;
} SSAInstruction;

// Phi node structure
typedef struct PhiNode {
    SSAValue* dest;                      // Variable being defined
    struct {
        BasicBlock* block;               // Predecessor block
        SSAValue* value;                 // Value from that predecessor
    }* operands;
    int operand_count;
} PhiNode;

// Basic block structure
typedef struct BasicBlock {
    int id;                          // Unique identifier
    InstructionList* instructions;   // SSA instructions
    struct BasicBlock** successors;  // Outgoing edges
    int successor_count;
    int successor_capacity;
    struct BasicBlock** predecessors;// Incoming edges  
    int predecessor_count;
    int predecessor_capacity;
    char* label;                     // Optional label for debugging
    
    // SSA-specific fields
    PhiNodeList* phi_nodes;          // Phi functions at block entry
    struct BasicBlock* idom;         // Immediate dominator
    struct BasicBlock** dom_frontier;// Dominance frontier
    int dom_frontier_count;
    int dom_frontier_capacity;
    
    // For CFG construction
    bool visited;                    // For traversal algorithms
    int post_order_num;              // Post-order numbering
} BasicBlock;

// Control Flow Graph structure
typedef struct CFG {
    BasicBlock* entry;               // Entry block
    BasicBlock* exit;                // Exit block
    BasicBlock** blocks;             // All blocks
    int block_count;
    int block_capacity;
    int next_block_id;               // For generating unique IDs
    
    // Function information
    char* function_name;
    
    // Temporary storage during construction
    BasicBlock* current_loop_header; // For break statements
    BasicBlock* current_loop_exit;   // For break statements
} CFG;

// --- CFG Creation Functions ---
CFG* create_cfg(const char* function_name);
BasicBlock* create_basic_block(CFG* cfg, const char* label);
void add_block_to_cfg(CFG* cfg, BasicBlock* block);
void add_edge(BasicBlock* from, BasicBlock* to);
void remove_edge(BasicBlock* from, BasicBlock* to);
void free_cfg(CFG* cfg);
void free_basic_block(BasicBlock* block);

// --- SSA Value Creation ---
SSAValue* create_ssa_var(const char* base_name, int version);
SSAValue* create_ssa_const(int value);
SSAValue* create_ssa_temp(int temp_id);
SSAValue* copy_ssa_value(SSAValue* value);
void free_ssa_value(SSAValue* value);
char* ssa_value_to_string(SSAValue* value);

// --- SSA Instruction Creation ---
SSAInstruction* create_ssa_assign(SSAValue* dest, SSAValue* src);
SSAInstruction* create_ssa_binary_op(SSAValue* dest, TokenType op, SSAValue* left, SSAValue* right);
SSAInstruction* create_ssa_unary_op(SSAValue* dest, TokenType op, SSAValue* operand);
SSAInstruction* create_ssa_call(SSAValue* dest, const char* func_name, SSAValue** args, int arg_count);
SSAInstruction* create_ssa_return(SSAValue* value);
SSAInstruction* create_ssa_branch(SSAValue* condition, BasicBlock* true_target, BasicBlock* false_target);
SSAInstruction* create_ssa_jump(BasicBlock* target);
void free_ssa_instruction(SSAInstruction* inst);

// --- Phi Node Functions ---
PhiNode* create_phi_node(SSAValue* dest);
void add_phi_operand(PhiNode* phi, BasicBlock* block, SSAValue* value);
void free_phi_node(PhiNode* phi);

// --- Instruction List Functions ---
InstructionList* create_instruction_list();
void add_instruction(InstructionList* list, SSAInstruction* inst);
void free_instruction_list(InstructionList* list);

// --- Phi Node List Functions ---
PhiNodeList* create_phi_node_list();
void add_phi_node(PhiNodeList* list, PhiNode* phi);
void free_phi_node_list(PhiNodeList* list);

#endif // CFG_H