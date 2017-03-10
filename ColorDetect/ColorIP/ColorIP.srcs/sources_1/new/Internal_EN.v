`timescale 1ns / 1ps

module Internal_EN(
    input CLK,
    input RESET,
    input EN_EXT,
    input [15:0] INTERVAL,
    output EN_INT,
    output ALL_DONE
    );
    
    reg en_int, all_done;
    integer frames, cycles;   
    
    always @ (negedge RESET)
    begin
        en_int <= 1'b0;
        all_done <= 1'b0;
        frames <= 0;
        cycles <= 0;
    end
    
    always @ (EN_EXT == 1'b0)
    begin    
        en_int <= 1'b0;
        all_done <= 1'b0;
        frames <= 0;
        cycles <= 0;
    end
    
    always @ (posedge CLK) begin
        if (EN_EXT && !all_done) begin
            if (cycles == INTERVAL - 1'b1) begin   // the last clk cycle before the next time intervel, lower the en signal and rise it at the next cycle
                en_int <= 1'b0;
                cycles <= cycles + 1;
            end
            else if (cycles == INTERVAL) begin
                cycles <= 0;
                en_int <= 1'b1;
                if (frames == 9) begin  // all ten frames are processed, one movement is done, now wait for further instruction
                    all_done <= 1'b1;
                end
                else begin
                    frames <= frames + 1;
                end 
            end
            else begin 
                cycles <= cycles + 1;
                en_int <= 1'b1;
            end        
        end
    end
    
    assign ALL_DONE = all_done;
    assign EN_INT = en_int;
    
endmodule
