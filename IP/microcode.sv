//////////////////////////////////////////////////////////////////////////////////
// Company: HotWright Inc. 
// Copyright (c) 2022
// All Rights Reserved
// Engineer: Steve Casselman 
// 
// Create Date: 07/06/2021 12:25:35 PM
// Design Name: 
// Module Name: microcode
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////
`timescale 10ns / 1ns


 module microcode #( 
               parameter NUM_ADDRESS_LINES = 5,
               parameter NUM_WORDS = 32,
               parameter NUM_VARSEL_BITS = 3,
               parameter NUM_TIMERS = 0,
               parameter NUM_STATE_BITS = 4,
               parameter NUM_SWITCH_BITS = 1,  
               parameter NUM_CONTROL_BITS = 32,
               parameter string FILENAME = "",
               parameter STANDALONE = 0
               )(
               input [NUM_CONTROL_BITS+(2*NUM_STATE_BITS)-1:0] smdata_word,
               input [NUM_ADDRESS_LINES-1:0] address,
               input smload,
               input rst,
               input clk,
               output reg [NUM_STATE_BITS-1:0] states,
               output [NUM_ADDRESS_LINES-1:0] jadr,
               output [NUM_VARSEL_BITS-1:0] varSel,
               output [NUM_TIMERS-1:0] timerSel,
               output [NUM_TIMERS-1:0] timerLd,
               output [NUM_SWITCH_BITS-1:0] switch_sel,
               output switch_active,
               output var_or_timer,
               output branch,
               output forced_jmp,
               output sub,
               output rtn,
               output ready
    );

reg [NUM_CONTROL_BITS+(2*NUM_STATE_BITS)-1:0] code [NUM_WORDS-1:0] ;
if (STANDALONE == 1) begin : gen_standalone
initial $readmemh(FILENAME,code,0,NUM_WORDS-1);
end

wire state_capture;
wire [NUM_STATE_BITS-1:0] state_value; 
wire [NUM_STATE_BITS-1:0] transition_value;
wire [NUM_CONTROL_BITS+(2*NUM_STATE_BITS)-1:0] microcode_bits; 

wire [(NUM_CONTROL_BITS)-1:0] ctl_out;
generate
   if (STANDALONE == 0) begin : gen_not_standalone
      reg [NUM_ADDRESS_LINES-1:0] address_1;
      always @ (posedge clk) begin 
         if (rst == 1) address_1 <= 0;
         else if (smload == 1) begin 
            code[address_1] <= smdata_word;
            if (address_1 <= NUM_WORDS) address_1 <= address_1 + 1;
         end
      end
      assign ready = rst?0:address_1 >= NUM_WORDS;
   end
endgenerate
assign microcode_bits = code[address];
assign state_value = microcode_bits[NUM_STATE_BITS-1:0];
assign transition_value = microcode_bits[(2*NUM_STATE_BITS)-1:NUM_STATE_BITS];
assign ctl_out = microcode_bits[(NUM_CONTROL_BITS+(2*NUM_STATE_BITS))-1:2*NUM_STATE_BITS];

genvar i;

generate
for(i =0;i<NUM_STATE_BITS; i = i+1) 

always @(posedge clk)
if (rst == 1) states[i] = 0; else
if (state_capture&transition_value[i])
states[i] = state_value [i];


endgenerate 


assign jadr =            ctl_out[NUM_ADDRESS_LINES-1:0];
assign varSel =          ctl_out[(NUM_ADDRESS_LINES+NUM_VARSEL_BITS)-1:NUM_ADDRESS_LINES];
assign timerSel =        ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+NUM_TIMERS-1:(NUM_ADDRESS_LINES+NUM_VARSEL_BITS)];
assign timerLd =         ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS)-1:(NUM_ADDRESS_LINES+NUM_VARSEL_BITS)+NUM_TIMERS];
assign switch_sel =      ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS -1:NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS)];
assign switch_active =   ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS ]; 
assign state_capture=    ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +1];
assign var_or_timer =    ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +2];
assign branch =          ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +3];
assign forced_jmp =      ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +4];
assign sub =             ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +5];
assign rtn =             ctl_out[NUM_ADDRESS_LINES+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCH_BITS +6];



endmodule
