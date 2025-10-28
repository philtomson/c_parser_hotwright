module test_gen
    #(parameter OPERATION_TYPE = 0)
    (
        input  logic [31:0] a,
        input  logic [31:0] b,
        output logic [63:0] z
    );
    generate 
        if (OPERATION_TYPE == 0) begin
        wire foo;
        end
    endgenerate
    wire [63:0] zz;

    generate
        always @ (a,b) begin
        if (OPERATION_TYPE == 0) begin
            assign foo = a[0] & b[0];
            z <= {foo, 63'b0};
        end
        else if (OPERATION_TYPE == 1) begin
            z <= a - b;
        end
        else if (OPERATION_TYPE == 2) begin
            z <= (a << 1) + b; // 2a+b
        end
        else begin
            z <= b - a;
        end
        end
    endgenerate
endmodule
