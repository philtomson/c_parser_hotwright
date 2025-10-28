//////////////////////////////////////////////////////////////////////////////////
// Company: Hotwright Inc 
// All Rights Reserved 
// Copyright (c) 2022
// Engineer: Steve Casselman 
// 
// Create Date: 05/27/2021 03:47:21 PM
// Design Name: 
// Module Name: control
// Project Name: hotstate 
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

module control #(parameter NUM_TIMERS = 1) (
    input [NUM_TIMERS-1:0] timer_done,
    input [NUM_TIMERS-1:0] timer_sel,
    input varible,
    input var_or_timer,
    input branch,
    input forced_jmp,
    input sub,
    input rtn,
    input clk,
    input rst,
    input interrupt,
    output jadr,
    output reg fired,
    output sub_push,
    output sub_pop
    );

    
    
    genvar i;
    wire [NUM_TIMERS-1:0] timern_true;
    wire timer_true;
    wire var_true;

    reg interrupt_r;
    generate 

    for (i = 0; i < NUM_TIMERS; i= i+1) begin
      assign timern_true[i] = timer_done[i] & timer_sel[i];
    end
         
    endgenerate
    
    always @(posedge clk) begin
    interrupt_r <= interrupt;
    if (rst==1) begin interrupt_r <= 0; fired <= 0; end
    else if (interrupt == 1 && interrupt_r == 0 ) begin fired <= 1; end
    else fired <= 0;
    end
    
    
   assign timer_true = (|timern_true)&var_or_timer;
   assign var_true = varible & ~var_or_timer;
   assign jadr = (((~var_true & ~timer_true)|sub) & branch) | forced_jmp;
   assign sub_push = (sub&branch) | fired;
   assign sub_pop = rtn;
    
    
endmodule
