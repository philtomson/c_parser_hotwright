//////////////////////////////////////////////////////////////////////////////////
// Company: HotWright Inc.
// Copyright (c) 2022 
// All rights reserved
// Engineer: Steve Casselman 
// Create Date: 09/13/2021 03:27:34 PM
// Design Name: 
// Module Name: switch
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: finds the offset into the a table. 
// The table has the addresses of the case statements code. 
// jadr adds an offset into the table to handel mutiple switches
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////
`timescale 10ns / 1ns


module switch #(parameter ADR_BUS_WIDTH = 8, 
                parameter SWITCH_MEM_BITS = 8, 
                parameter SWITCH_MEM_WORDS = 256,
                parameter string FILENAME = "",
                parameter STANDALONE = 0
                )(
    input [ADR_BUS_WIDTH - 1:0] switch_tdata,
    input switch_tvalid,
    input [ADR_BUS_WIDTH - 1:0] jadr,
    input [SWITCH_MEM_BITS - 1:0] switch_offset_adr,
    input switch_active,
    input clk,
    input rst,
    output ready,
    output [ADR_BUS_WIDTH - 1:0] switch_adr
    );
    
     
    reg [ADR_BUS_WIDTH-1:0] switch_mem [SWITCH_MEM_WORDS - 1:0];
    if (STANDALONE == 1) begin : gen_standalone
    initial $readmemh(FILENAME,switch_mem, 0,SWITCH_MEM_WORDS -1); 
    end

    wire [$clog2(SWITCH_MEM_WORDS)-1:0] memadr;

    generate 

    if (STANDALONE == 0) begin : gen_not_standalone
    reg [$clog2(SWITCH_MEM_WORDS)-1:0] address_1;
    always @ (posedge clk) begin
       if (rst == 1) address_1 <= 0;
       else if (switch_tvalid  == 1) begin
               switch_mem[address_1] <= switch_tdata;
               if (address_1 < (SWITCH_MEM_WORDS - 1)) address_1 <= address_1 + 1;
            end
    end
   assign ready = rst?0:address_1 >= SWITCH_MEM_WORDS;
   end
   endgenerate

   assign memadr = {jadr,switch_offset_adr};

   assign switch_adr = switch_active?switch_mem[memadr]:0; 
   
endmodule
