`timescale 1ns / 1ps
// ============================================================
// servo_ctrl.v : 서보(문 개폐) + DC 모터 속도(PWM) 제어
// ============================================================
// ============================================================
// servo_ctrl : SG90 서보 - 문 열기/닫기
//   50Hz(20ms) 주기, 펄스폭 1.0ms=0도 / 1.5ms=90도 / 2.0ms=180도 (과제 스펙)
//   문 닫힘 = CLOSE_US, 문 열림 = OPEN_US
//   부드럽게 열리도록 1ms 마다 펄스폭을 STEP_US 씩 목표값으로 이동
//   (1000us -> 1500us : 500ms 동안 천천히 회전)
// ============================================================
module servo_ctrl #(
    parameter CLOSE_US = 1000,   // 0도
    parameter OPEN_US  = 1500,   // 90도
    parameter STEP_US  = 1,      // 1ms 당 이동량(us)
    parameter HOLD_MS  = 1000    // 도착 후 PWM 유지 시간(ms), 이후 PWM 끔
) (
    input      clk,
    input      reset,
    input      tick_us,          // 1us tick
    input      tick_ms,          // 1ms tick
    input      i_open,           // 1: 문 열림
    output reg o_pwm,
    output [11:0] o_width_us     // 현재 펄스폭(디버그/LED용)
);
    reg [14:0] r_period_us;      // 0 ~ 19999
    reg [11:0] r_width;          // 현재 폭
    reg [11:0] r_width_latch;    // 주기 시작에서 고정 (glitch 방지)

    wire [11:0] w_target = i_open ? OPEN_US : CLOSE_US;

    // 목표 각도로 천천히 이동
    always @(posedge clk, posedge reset) begin
        if (reset) r_width <= CLOSE_US;
        else if (tick_ms) begin
            if      (r_width + STEP_US <= w_target) r_width <= r_width + STEP_US;
            else if (r_width >= w_target + STEP_US) r_width <= r_width - STEP_US;
            else                                    r_width <= w_target;
        end
    end

    // 도착 후 HOLD_MS 지나면 PWM 끄기 (서보가 계속 힘주며 떨리고 뜨거워지는 것 방지)
    // 문은 기어 마찰로 그 자리에 유지됨. 목표가 바뀌면 다시 PWM 출력
    reg [11:0] r_hold_cnt;
    reg        r_idle;
    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_hold_cnt <= 0; r_idle <= 1'b0;
        end else if (r_width != w_target) begin
            r_hold_cnt <= 0; r_idle <= 1'b0;
        end else if (tick_ms && !r_idle) begin
            if (r_hold_cnt >= HOLD_MS-1) r_idle <= 1'b1;
            else r_hold_cnt <= r_hold_cnt + 1;
        end
    end

    // 20ms 주기 PWM
    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_period_us <= 0; r_width_latch <= CLOSE_US; o_pwm <= 1'b0;
        end else if (tick_us) begin
            if (r_period_us >= 19999) begin
                r_period_us   <= 0;
                r_width_latch <= r_idle ? 12'd0 : r_width;   // idle 이면 펄스 없음
            end else
                r_period_us <= r_period_us + 1;
            o_pwm <= (r_period_us < r_width_latch);
        end
    end

    assign o_width_us = r_width;
endmodule

// ============================================================
// motor_pwm : 회전 접시 DC 모터 속도 조절 (L298N / L9110 등 드라이버)
//   1kHz PWM (1us tick x 1000), 출력 단계 1~5 -> duty 40/55/70/85/100 %
//   조리 중(i_run=1)에만 회전, in1_in2 = 10 (정방향) / 00 (정지)
//   ※ 실습 pwm_duty_control 은 10MHz PWM 이었는데 모터 드라이버(L298N)
//     스위칭 한계(수십 kHz) 때문에 1kHz 로 낮춤
// ============================================================
module motor_pwm (
    input            clk,
    input            reset,
    input            tick_us,
    input            i_run,
    input      [2:0] i_level,     // 1~5
    output reg       o_pwm,
    output     [1:0] o_in1_in2,
    output     [6:0] o_duty       // % (표시용)
);
    reg [9:0] r_cnt;              // 0 ~ 999 us
    reg [6:0] w_duty;

    always @(*) begin
        case (i_level)
            3'd1:    w_duty = 7'd40;
            3'd2:    w_duty = 7'd55;
            3'd3:    w_duty = 7'd70;
            3'd4:    w_duty = 7'd85;
            3'd5:    w_duty = 7'd100;
            default: w_duty = 7'd70;
        endcase
    end

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_cnt <= 0; o_pwm <= 1'b0;
        end else if (tick_us) begin
            r_cnt <= (r_cnt >= 999) ? 10'd0 : r_cnt + 1;
            o_pwm <= i_run && (r_cnt < w_duty * 10);
        end
    end

    assign o_in1_in2 = i_run ? 2'b10 : 2'b00;
    assign o_duty    = w_duty;
endmodule