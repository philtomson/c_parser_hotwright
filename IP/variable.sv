//////////////////////////////////////////////////////////////////////////////////
// Company: Hotwright Inc.
// Copyright (c) 2022 
// All Rights Reserved
// Engineer: Steve Casselman
//
// Design Name:
// Module Name: variable
// Project Name:
// Target Devices:
// Tool Versions:
// Description: This is the UberLUT. It contains all the logic from if statements 
//
// Dependencies:
//
// Revision:
// Revision 0.01 - File Created 6/24/2020
// Additional Comments: By Steve Casselman
//
//////////////////////////////////////////////////////////////////////////////////

`timescale 10ns / 1ns

module variable #(
               parameter NUM_VARS = 6,
               parameter NUM_VARSEL = 2,
               parameter NUM_VARSEL_BITS = 3,
               parameter string FILENAME = "",
               parameter STANDALONE = 0
               )(
               input clk,
               input rst,
               input [NUM_VARS-1:0] variable,
               input [NUM_VARSEL_BITS-1:0] varSel,
               input uberLUT_data,
               input uberLUT_load,
               output ready,
               output lhs
               );


reg [0:0] code [(NUM_VARSEL*2**(NUM_VARS))-1:0] ;
if (STANDALONE == 1) begin : gen_standalone
initial $readmemb(FILENAME,code,0,(NUM_VARSEL*2**(NUM_VARS))-1);
end

wire [NUM_VARS+NUM_VARSEL_BITS-1:0] var_ram_address;

generate 
if (STANDALONE == 0) begin : gen_not_standalone
   reg [NUM_VARS+NUM_VARSEL_BITS-1:0] load_addr;
   always @ (posedge clk) begin
      if (rst == 1) load_addr <= 0;
      else if (uberLUT_load == 1) begin 
         code[load_addr] <= uberLUT_data;
         if (load_addr <= NUM_VARSEL*2**(NUM_VARS)) load_addr <= load_addr + 1;
      end
end

assign ready = rst?0:load_addr >= (NUM_VARSEL*2**NUM_VARS);
end
endgenerate

assign var_ram_address = {varSel,variable};
assign lhs =  code[var_ram_address];


endmodule
