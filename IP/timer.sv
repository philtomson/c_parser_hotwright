//////////////////////////////////////////////////////////////////////////////////
// Company: HotWright Inc.
// Engineer: Steve Casseslman
// Copyright (c) 2022
// All Rights Reserved 
// Create Date: 05/27/2021 04:07:58 PM
// Design Name: 
// Module Name: timer
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
 
module timer #(parameter NUM_TIMERS = 1, 
               parameter TIMER_WIDTH = 8, 
               parameter TIM_MEM_WORDS = 1, 
               parameter TIM_MEM_ADR_WIDTH = 1,
               parameter string FILENAME = "",
               parameter STANDALONE = 0
               )(
    input [TIMER_WIDTH-1:0] timer_data,
    input [TIM_MEM_ADR_WIDTH-1:0] timer_mem_adr,
    input [NUM_TIMERS-1:0] timer_ld,
    input [NUM_TIMERS-1:0] timer_sel,
    input tim_tvalid,
    input clk,
    input rst,
    output ready, 
    output [NUM_TIMERS-1:0] timer_done 
    );
    
    reg [TIMER_WIDTH-1:0] timer_mem [TIM_MEM_WORDS -1:0];
    reg [TIMER_WIDTH-1:0] count [NUM_TIMERS-1:0];
    if (STANDALONE == 1) begin : gen_standalone
       initial $readmemh(FILENAME,timer_mem, 0,TIM_MEM_WORDS -1); 
    end

generate
if(STANDALONE == 0) begin : gen_not_standalone
    reg [TIM_MEM_ADR_WIDTH-1:0] address_1;

    always @ (posedge clk) begin 
       if (rst == 1) address_1 <= 0;
       else if (tim_tvalid  == 1) begin 
               timer_mem[address_1] <= timer_data;
               if (address_1 < TIM_MEM_WORDS) address_1 <= address_1 + 1;
            end
    end
   
   assign ready = rst?0:address_1 >= TIM_MEM_WORDS;
   end
endgenerate 
  
    generate 
    genvar i; 

    // here we generate the timers 
    for(i=0;i<NUM_TIMERS;i = i + 1)
    begin

    
    always @(posedge clk) begin
    if(rst == 1)  count[i] <= 0; 
    
    else if (timer_ld[i] == 1 && timer_sel[i] == 1) count[i] <= timer_mem[timer_mem_adr]; 
 
    else if (timer_sel[i] == 1 && count[i] > 0 ) count[i] <= count[i] - 1;
    end

    assign timer_done[i] = (count[i] == 0 && timer_sel[i] == 1 && timer_ld[i] == 0);

    end
    endgenerate    
        
endmodule
