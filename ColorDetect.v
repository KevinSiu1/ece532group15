`timescale 1ns / 1ps

module ColorDetect(
    input reset_n,
    input clk,
    input enable,
    
    input [23:0] DATA_IN,
    input DATA_IN_VALID,
    input [10:0] X_VALUE,
    input [9:0] Y_VALUE,
    input [1:0] COLOR,
	input [31:0] FRAME_WAIT,
    
    output [31:0] COORDINATE,
    output READY
    );
    
    reg ready;
    reg frame_count;
    wire EN;
    integer x_sum, y_sum, counter;
    
    Internal_EN dut (
        .CLK(clk),
        .RESET(reset_n),
        .EN_IN(enable),
        .NEW_FRAME(DATA_IN_VALID && (X_VALUE == 11'b0) && (Y_VALUE == 10'b0)),
        .FRAME_WAIT(FRAME_WAIT),
        .EN_OUT(EN)
    );
        
    always @(posedge clk)
    begin
        if (!reset_n || !EN)
        begin
            x_sum <= 0;
            y_sum <= 0;
            counter <= 0;
            ready <= 1'b0;
        end        
        else if (EN && DATA_IN_VALID)
        begin
            // read pixel
            case (COLOR)
            2'b00 :     // red
                if ( DATA_IN[23:16] >= 180     // R
                    && DATA_IN[15:8] <= 80      // G
                    && DATA_IN[7:0] <= 80 )    // B
                begin     
                    x_sum <= x_sum + X_VALUE;
                    y_sum <= y_sum + Y_VALUE;
                    counter <= counter + 1;
                end
            2'b01 :     // green
                if ( DATA_IN[23:16] < 80      // R
                    && DATA_IN[15:8] > 180     // G
                    && DATA_IN[7:0] < 80 )    // B
                begin     
                    x_sum <= x_sum + X_VALUE;
                    y_sum <= y_sum + Y_VALUE;
                    counter <= counter + 1;
                end
            2'b10 :     // blue
                if ( DATA_IN[23:16] < 80      // R
                    && DATA_IN[15:8] < 80      // G
                    && DATA_IN[7:0] > 180 )   // B
                begin     
                    x_sum <= x_sum + X_VALUE;
                    y_sum <= y_sum + Y_VALUE;
                    counter <= counter + 1;
                end
            2'b11 :     // yellow
                if ( DATA_IN[23:16] > 200     // R
                    && DATA_IN[15:8] > 200     // G
                    && DATA_IN[7:0] < 80 )    // B
                begin     
                    x_sum <= x_sum + X_VALUE;
                    y_sum <= y_sum + Y_VALUE;
                    counter <= counter + 1;
                end                 
            endcase
        end
            
        // increment x and y
        // to get new pixel
        
        //FIXME: Where do we reset ready?
        if( X_VALUE == 1279 && Y_VALUE == 719 ) begin  // max, reset everything
            ready <= 1'b1;            
        end 
		
		// RESET ready, ready is high only for one clock cycle
		if (ready) begin	
			ready <= 1'b0;
		end
    end  
    
    //Is it possible to do this only once per frame?
    assign COORDINATE[31:16] = x_sum / counter;  
    assign COORDINATE[15:0] = y_sum / counter;  
    assign READY = ready;
    
endmodule

module Internal_EN(
    input CLK,
    input RESET,
    input EN_IN,
    input NEW_FRAME,
    input [31:0] FRAME_WAIT,
    output EN_OUT
    );
    
    reg en_out;
    integer frames;   
    
    always @ (posedge CLK) begin
        if (!EN_IN || !RESET) begin
            en_out <= 1'b0;
            frames <= 0;
        end
        else if (EN_IN) begin
            if (frames == FRAME_WAIT) begin
                frames <= 0;
                en_out <= 1'b1;
            end
            else if (NEW_FRAME) begin
                frames <= frames + 1;
                en_out <= 1'b0;
            end
            else begin
                en_out <= 1'b0;
            end
        end
        else begin
            en_out <= 1'b0;
        end
    end
    
    assign EN_OUT = en_out;    
endmodule

