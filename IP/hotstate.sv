//////////////////////////////////////////////////////////////////////////////////
// Company: Hotwright Inc.
// Copyright (c) 2022
// All Rights Reserved 
// Engineer: Steve Casselman
// 
// Create Date: 06/20/2020 02:51:51 PM
// Design Name: 
// Module Name: hotstate
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: This is the hot algorithmic state machine. The goal is to create a single cycle 
// algorithmic state machine for control of a data flow grapic  
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created 6/24/2020
// Additional Comments: By Steve Casselman
// 
//////////////////////////////////////////////////////////////////////////////////
`timescale 10ns / 1ns

module hotstate #(
parameter NUM_STATES = 8,
parameter NUM_VARSEL = 3,  
parameter NUM_VARSEL_BITS = 4,  
parameter NUM_VARS = 10,
parameter NUM_TIMERS = 1,
parameter NUM_SWITCHES = 1,
parameter SWITCH_OFFSET_BITS = 8,
parameter SWITCH_MEM_WORDS = 8,
parameter NUM_SWITCH_BITS = 2,
parameter NUM_ADR_BITS = 5,
parameter NUM_WORDS = 128,
parameter TIM_WIDTH = 32,
parameter TIM_MEM_WORDS = 2,
parameter NUM_CTL_BITS = NUM_ADR_BITS+NUM_VARSEL_BITS+(2*NUM_TIMERS) + NUM_SWITCHES + 7,
parameter SMDATA_WIDTH = (2*NUM_STATES)+NUM_CTL_BITS,
parameter STACK_DEPTH = 4,
parameter string MCFILENAME = "",
parameter string VRFILENAME = "",
parameter string TIFILENAME = "",
parameter string SWFILENAME = "",
parameter STANDALONE = 0 
)(
    input [NUM_VARS-1:0] variables,
    output [NUM_STATES-1:0] states,
    output ready,
    input clk,
    input rst,
    input hlt,
    input uberLUT_tvalid,
    input uberLUT_tdata,
    input sm_tvalid,
    input [SMDATA_WIDTH-1:0] sm_tdata,
    input tim_tvalid,
    input [TIM_WIDTH-1:0] tim_tdata,
    input [NUM_ADR_BITS-1:0] switch_tdata,
    input switch_tvalid,
    input  [SWITCH_OFFSET_BITS-1:0] switch_offset,
    output [NUM_SWITCH_BITS-1:0] switch_sel,
    output [NUM_ADR_BITS-1:0] debug_adr,
    input interrupt,
    input [NUM_ADR_BITS-1:0] interrupt_address
    );

wire  [(2*NUM_STATES)-1:0] statedata;
wire  [NUM_CTL_BITS-1:0] ctldata;
wire  lhs;
wire forced_jmp;
wire [NUM_ADR_BITS-1:0] address;
assign debug_adr = address;
wire [NUM_ADR_BITS-1:0] returnadr;
wire [NUM_ADR_BITS-1:0] jadr;
wire [NUM_ADR_BITS -1:0] switch_adr; 
wire sub_push;
wire sub_pop;
wire [NUM_VARSEL_BITS-1:0] varSel;
//generate
//if (NUM_TIMERS != 0) begin
wire [NUM_TIMERS-1:0] timer_done;
wire [NUM_TIMERS-1:0] timer_sel;
wire [NUM_TIMERS-1:0] timerLd;
//end
//endgenerate
wire var_or_timer;
wire jmpadr;
wire sub;
wire rtn;
wire branch; 
wire state_capture;
wire switch_active;
wire fired;
wire code_ready, uberLUT_ready, tim_ready, switch_ready;

assign statedata = sm_tdata[2*NUM_STATES-1:0];

assign ctldata = sm_tdata[SMDATA_WIDTH-1:2*NUM_STATES];

if(STANDALONE == 0) begin : genblk_not_standalone1
   assign ready = STANDALONE?1: code_ready & (uberLUT_ready | NUM_VARS == 0) & (tim_ready | NUM_TIMERS == 0) & (switch_ready | NUM_SWITCHES == 0);
end
else begin : genblk_standalone1
   assign ready = 1;
end
generate 
if (NUM_VARS != 0) begin : variable
variable #(.NUM_VARS(NUM_VARS),
           .NUM_VARSEL(NUM_VARSEL),
           .NUM_VARSEL_BITS(NUM_VARSEL_BITS),
           .FILENAME(VRFILENAME),
           .STANDALONE (STANDALONE)
      ) Variable (
     .variable(variables),
     .uberLUT_data(uberLUT_tdata),
     .uberLUT_load(uberLUT_tvalid),
     .varSel(varSel),
     .clk(clk),
     .ready(uberLUT_ready),
     .rst(rst),
     .lhs(lhs)
     ); 
end
else begin : gen_lhs_assign
assign lhs = 1;
end
endgenerate

next_address #(.BUS_WIDTH(NUM_ADR_BITS)) Next_addr (
     .address(address),
     .jmpadr(jadr),
     .jadr(jmpadr),
     .returnadr(returnadr),
     .switch_adr(switch_adr),
     .switch_active(switch_active),
     .ready(ready),
     .rst(rst),
     .hlt(hlt),
     .fired(fired),
     .sub_pop(sub_pop),
     .clk(clk),
     .interrupt_address(interrupt_address),
     .interrupt(interrupt),
     .nextadr(address) 
     );
      

microcode #(
      .NUM_ADDRESS_LINES(NUM_ADR_BITS), 
      .NUM_STATE_BITS(NUM_STATES),
      .NUM_CONTROL_BITS(NUM_CTL_BITS), 
      .NUM_VARSEL_BITS(NUM_VARSEL_BITS),
      .NUM_TIMERS(NUM_TIMERS?NUM_TIMERS:1),
      .NUM_SWITCH_BITS(NUM_SWITCH_BITS),
      .NUM_WORDS(NUM_WORDS),
      .FILENAME(MCFILENAME),
      .STANDALONE (STANDALONE)
      ) Microcode (
      .smdata_word(sm_tdata),
     .address(address),
     .varSel(varSel),
     .timerLd(timerLd),
     .var_or_timer(var_or_timer),
     .clk(clk),
     .rst(rst),
     .sub(sub),
     .forced_jmp(forced_jmp),
     .rtn(rtn),
     .branch(branch),
     .smload(sm_tvalid),
     .timerSel(timer_sel),
     .jadr(jadr),
     .switch_sel(switch_sel),
     .switch_active(switch_active),
     .states(states),
     .ready(code_ready)
     );

generate
if (STACK_DEPTH != 0) begin: stack
stack #(.DEPTH(STACK_DEPTH),.WIDTH(NUM_ADR_BITS)) Stack (
     .push_adr(address),
     .pop_adr(returnadr),
     .sub_push(sub_push),
     .sub_pop(sub_pop),
     .clk(clk),
     .rst(rst)
     );
end
endgenerate

control #(.NUM_TIMERS(NUM_TIMERS)) Control (
     .varible(lhs),
     .var_or_timer(var_or_timer),
     .timer_done(timer_done),
     .timer_sel(timer_sel),
     .sub(sub),
     .forced_jmp(forced_jmp),
     .rtn(rtn),
     .branch(branch),
     .jadr(jmpadr),
     .clk(clk),
     .rst(rst),
     .fired(fired),
     .interrupt(interrupt),
     .sub_push(sub_push),
     .sub_pop(sub_pop)
     ); 
 generate 
 if (NUM_TIMERS != 0) begin : timer
timer #(.NUM_TIMERS(NUM_TIMERS),
       .TIMER_WIDTH(TIM_WIDTH),
       .TIM_MEM_WORDS(TIM_MEM_WORDS),
       .TIM_MEM_ADR_WIDTH(NUM_ADR_BITS),
       .FILENAME(TIFILENAME),
       .STANDALONE (STANDALONE)
       
     ) Timers (
     .timer_data(tim_tdata),
     .timer_ld(timerLd),
     .timer_sel(timer_sel),
     .timer_mem_adr(jadr),
     .tim_tvalid(tim_tvalid),
     .clk(clk),
     .rst(rst),
     .ready(tim_ready),
     .timer_done(timer_done)
     );
     end
endgenerate
 
generate
if(NUM_SWITCHES != 0) begin : switch
switch #(.ADR_BUS_WIDTH(NUM_ADR_BITS),
         .SWITCH_MEM_BITS(SWITCH_OFFSET_BITS),
         .SWITCH_MEM_WORDS(SWITCH_MEM_WORDS),
         .FILENAME(SWFILENAME),
         .STANDALONE (STANDALONE)
         ) Switch (
.switch_tvalid (switch_tvalid),
.switch_tdata(switch_tdata),
.jadr(jadr),
.switch_offset_adr(switch_offset),
.switch_active(switch_active),
.switch_adr(switch_adr),
.clk (clk),
.rst (rst),
.ready(switch_ready)
);
end
endgenerate


     
endmodule
