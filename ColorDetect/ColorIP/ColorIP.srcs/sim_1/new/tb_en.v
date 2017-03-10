`timescale 1ns / 1ps

module tb_en( );    
    reg reset;
    reg enable;
    reg sys_clk;
    wire sub_enable;
    wire all_done;
    
    Internal_EN dut
    (   .CLK(sys_clk),
        .RESET(reset),
        .EN_EXT(enable),
        .INTERVEL(16'b01010),
        .EN_INT(sub_enable),
        .ALL_DONE(all_done)
    );
    
    always #5 sys_clk = ~sys_clk;
    
    initial begin
        sys_clk = 1'b0;
        reset = 1'b1;
        enable = 1'b0;
        #8 enable = 1'b1;
        #1500 enable = 1'b0;
    end
    
endmodule

