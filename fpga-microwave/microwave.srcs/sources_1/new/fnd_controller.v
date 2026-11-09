`timescale 1ns / 1ps
// ============================================================
// fnd_controller.v : 4자리 FND 표시 (Basys3, an/seg active-low)
//   seg[7:0] = {dp, g, f, e, d, c, b, a}  (실습 fnd_controller 와 같은 비트배치)
//
//   IDLE  : 설정 시간 MM.SS
//   COOK  : 1초 circle 애니메이션 <-> 1초 남은시간 번갈아 표시   (요구 3)
//   PAUSE : 남은시간 깜빡임
//   DONE  : "00.00" 3회 ON/OFF                                    (요구 2-3)
//
//   실습 fnd_digit_select 는 posedge tick 을 클럭으로 썼는데
//   여기서는 clk + tick enable 방식으로 바꿔 클럭 도메인을 하나로 통일
// ============================================================
module fnd_controller #(
    parameter PHASE_MS = 1000,    // circle / 시간 전환 주기
    parameter ANIM_MS  = 83       // circle 한 칸 이동 (12칸 = 약 1초)
) (
    input            clk,
    input            reset,
    input            tick_ms,
    input      [1:0] i_state,     // 0 IDLE 1 COOK 2 PAUSE 3 DONE
    input      [6:0] i_min,
    input      [5:0] i_sec,
    input            i_blink_on,
    output reg [3:0] an,
    output reg [7:0] seg
);
    localparam IDLE = 2'd0, COOK = 2'd1, PAUSE = 2'd2, DONE = 2'd3;
    localparam [7:0] C_BLANK = 8'hFF;

    function [7:0] f_digit;       // 0~9 -> 7seg (실습 표 그대로)
        input [3:0] v;
        case (v)
            4'd0: f_digit = 8'b11000000;  4'd1: f_digit = 8'b11111001;
            4'd2: f_digit = 8'b10100100;  4'd3: f_digit = 8'b10110000;
            4'd4: f_digit = 8'b10011001;  4'd5: f_digit = 8'b10010010;
            4'd6: f_digit = 8'b10000010;  4'd7: f_digit = 8'b11111000;
            4'd8: f_digit = 8'b10000000;  4'd9: f_digit = 8'b10010000;
            default: f_digit = 8'hFF;
        endcase
    endfunction

    // ---------------- 타이밍 (scan / phase / anim) ----------------
    reg [1:0] r_sel;
    reg [9:0] r_phase_cnt;
    reg       r_phase;            // 0: 시간, 1: circle
    reg [6:0] r_anim_cnt;
    reg [3:0] r_pos;              // circle 위치 0~11

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_sel <= 0; r_phase_cnt <= 0; r_phase <= 0; r_anim_cnt <= 0; r_pos <= 0;
        end else if (tick_ms) begin
            r_sel <= r_sel + 1;                               // 1ms 마다 자리 이동
            if (i_state != COOK) begin
                r_phase_cnt <= 0; r_phase <= 0; r_anim_cnt <= 0; r_pos <= 0;
            end else begin
                if (r_phase_cnt >= PHASE_MS-1) begin r_phase_cnt <= 0; r_phase <= ~r_phase; end
                else r_phase_cnt <= r_phase_cnt + 1;
                if (r_anim_cnt >= ANIM_MS-1) begin
                    r_anim_cnt <= 0;
                    r_pos <= (r_pos >= 11) ? 4'd0 : r_pos + 1;
                end else r_anim_cnt <= r_anim_cnt + 1;
            end
        end
    end

    // ---------------- circle 패턴 ----------------
    // 외곽 12칸 (d3=왼쪽 자리) : d3a d2a d1a d0a d0b d0c d0d d1d d2d d3d d3e d3f
    // 머리+꼬리 3칸이 시계방향으로 회전
    reg [7:0] c3, c2, c1, c0;
    integer k;
    reg [3:0] p;
    always @(*) begin
        c3 = 8'hFF; c2 = 8'hFF; c1 = 8'hFF; c0 = 8'hFF;
        for (k = 0; k < 3; k = k + 1) begin
            p = (r_pos >= k) ? (r_pos - k) : (r_pos + 12 - k);
            case (p)
                4'd0:  c3[0] = 1'b0;   // a
                4'd1:  c2[0] = 1'b0;
                4'd2:  c1[0] = 1'b0;
                4'd3:  c0[0] = 1'b0;
                4'd4:  c0[1] = 1'b0;   // b
                4'd5:  c0[2] = 1'b0;   // c
                4'd6:  c0[3] = 1'b0;   // d
                4'd7:  c1[3] = 1'b0;
                4'd8:  c2[3] = 1'b0;
                4'd9:  c3[3] = 1'b0;
                4'd10: c3[4] = 1'b0;   // e
                4'd11: c3[5] = 1'b0;   // f
                default: ;
            endcase
        end
    end

    // ---------------- 시간 패턴 MM.SS ----------------
    wire [7:0] t3 = f_digit(i_min / 10);
    wire [7:0] t2 = f_digit(i_min % 10) & 8'b0111_1111;   // dp ON (분.초 구분)
    wire [7:0] t1 = f_digit(i_sec / 10);
    wire [7:0] t0 = f_digit(i_sec % 10);

    // ---------------- 표시 선택 ----------------
    reg [7:0] d3, d2, d1, d0;
    always @(*) begin
        case (i_state)
            DONE:  if (i_blink_on) begin d3 = f_digit(0); d2 = f_digit(0) & 8'h7F; d1 = f_digit(0); d0 = f_digit(0); end
                   else            begin d3 = C_BLANK; d2 = C_BLANK; d1 = C_BLANK; d0 = C_BLANK; end
            COOK:  if (r_phase)    begin d3 = c3; d2 = c2; d1 = c1; d0 = c0; end
                   else            begin d3 = t3; d2 = t2; d1 = t1; d0 = t0; end
            PAUSE: if (i_blink_on) begin d3 = t3; d2 = t2; d1 = t1; d0 = t0; end
                   else            begin d3 = C_BLANK; d2 = C_BLANK; d1 = C_BLANK; d0 = C_BLANK; end
            default:               begin d3 = t3; d2 = t2; d1 = t1; d0 = t0; end
        endcase
    end

    // ---------------- 자리 스캔 ----------------
    always @(*) begin
        case (r_sel)
            2'd0: begin an = 4'b1110; seg = d0; end
            2'd1: begin an = 4'b1101; seg = d1; end
            2'd2: begin an = 4'b1011; seg = d2; end
            default: begin an = 4'b0111; seg = d3; end
        endcase
    end
endmodule
