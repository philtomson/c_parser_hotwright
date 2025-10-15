`ifndef MICROCODE_PARAMS_VH
`define MICROCODE_PARAMS_VH

localparam STATE_WIDTH = 3;
localparam MASK_WIDTH = 3;
localparam JADR_WIDTH = 1;
localparam VARSEL_WIDTH = 2;
localparam TIMERSEL_WIDTH = 1;
localparam TIMERLD_WIDTH = 1;
localparam SWITCH_SEL_WIDTH = 2;
localparam SWITCH_ADR_WIDTH = 1;
localparam STATE_CAPTURE_WIDTH = 1;
localparam VAR_OR_TIMER_WIDTH = 1;
localparam BRANCH_WIDTH = 1;
localparam FORCED_JMP_WIDTH = 1;
localparam SUB_WIDTH = 1;
localparam RTN_WIDTH = 1;

localparam INSTR_WIDTH = STATE_WIDTH + MASK_WIDTH + JADR_WIDTH + VARSEL_WIDTH + 
                         TIMERSEL_WIDTH + TIMERLD_WIDTH + SWITCH_SEL_WIDTH + SWITCH_ADR_WIDTH + 
                         STATE_CAPTURE_WIDTH + VAR_OR_TIMER_WIDTH + BRANCH_WIDTH + FORCED_JMP_WIDTH + 
                         SUB_WIDTH + RTN_WIDTH;

`endif // MICROCODE_PARAMS_VH
