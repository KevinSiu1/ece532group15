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
    
    output [31:0] COORDINATE,
    output READY
    );
    
    reg ready;
    reg frame_count;
    integer x, y;
    integer x_sum, y_sum, counter;
    
    wire data_valid;
    
    always @(posedge clk)
    begin
        if (!reset_n || !enable)
        begin
            x <= 0;
            y <= 0;
            x_sum <= 0;
            y_sum <= 0;
            counter <= 0;
            ready <= 1'b0;
        end
        else if (enable && data_valid)
        begin
            // read pixel
            case (COLOR)
            2'b00 :     // red
                if ( DATA_IN[23:16] >= 180     // R
                    && DATA_IN[15:8] <= 80      // G
                    && DATA_IN[7:0] <= 80 )    // B
                begin     
                    x_sum <= x_sum + x - 1;
                    y_sum <= y_sum + y;
                    counter <= counter + 1;
                end
            2'b01 :     // green
                if ( DATA_IN[23:16] < 80      // R
                    && DATA_IN[15:8] > 180     // G
                    && DATA_IN[7:0] < 80 )    // B
                begin     
                    x_sum <= x_sum + x - 1;
                    y_sum <= y_sum + y;
                    counter <= counter + 1;
                end
            2'b10 :     // blue
                if ( DATA_IN[23:16] < 80      // R
                    && DATA_IN[15:8] < 80      // G
                    && DATA_IN[7:0] > 180 )   // B
                begin     
                    x_sum <= x_sum + x - 1;
                    y_sum <= y_sum + y;
                    counter <= counter + 1;
                end
            2'b11 :     // yellow
                if ( DATA_IN[23:16] > 200     // R
                    && DATA_IN[15:8] > 200     // G
                    && DATA_IN[7:0] < 80 )    // B
                begin     
                    x_sum <= x_sum + x - 1;
                    y_sum <= y_sum + y;
                    counter <= counter + 1;
                end                 
            endcase
        end
            
        // increment x and y
        // to get new pixel
        
        //FIXME: Where do we reset ready?
        if( x == 1276 && y == 716 ) begin  // max, reset everything
            ready <= 1'b1;            
        end 
        else if (enable && data_valid)
        begin                
            if ( x == 1276 ) begin
                x <= 0;
                y <= y + 4;  // Sample every 4th line (Is this necessary, or can we sample every line?)
            end else
                x <= x + 4;  // Sample every 4th pixel in a line
        end  
    end  
    
    assign data_valid = DATA_IN_VALID && (X_VALUE[1:0] == 2'b00) && (Y_VALUE[1:0] == 2'b00);
    
    //Is it possible to do this only once per frame?
    assign COORDINATE[31:16] = x_sum / counter; // need rounding here 
    assign COORDINATE[15:0] = y_sum / counter;  // need rounding here
    assign READY = ready;
    
endmodule
