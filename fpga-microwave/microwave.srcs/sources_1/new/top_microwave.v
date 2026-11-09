`timescale 1ns / 1ps
// ============================================================
// top_microwave.v : Basys3 전자레인지 Top (+ 메인 FSM)
//
//  [입력]  btnU 시작/일시정지   btnD 취소   btnL 문 열기/닫기
//          btnR 모터 출력단계(1~5)
//          로터리 회전 : 시간 설정 (초 모드 ±10초 / 분 모드 ±1분)
//          로터리 누름 : 분/초 설정 모드 전환
//          reset = SW15 (R2)
//  [출력]  FND, 부저(JC1), 서보(JB1), DC모터 PWM/방향(JA1~3), LED
//
//  FSM : IDLE(시간설정) -> COOK(조리) <-> PAUSE(일시정지) -> DONE(완료) -> IDLE
//
//  CLK_PER_MS : 1ms 당 클럭 수 (보드 100MHz = 100_000, 시뮬레이션 1_000)
// ============================================================
module top_microwave #(
    parameter CLK_PER_MS = 100_000,
    parameter SEC_STEP   = 10,       // 초 모드에서 로터리 1칸당 증감(초)
    parameter DONE_MS    = 3000      // 완료 표시 (0 3회 깜빡임) 시간
) (
    input         clk,
    input         reset,
    input         btnU,
    input         btnD,
    input         btnL,
    input         btnR,
    input         s1,
    input         s2,
    input         key,
    output [7:0]  seg,
    output [3:0]  an,
    output        buzzer,
    output        servo_pwm,
    output        motor_pwm,
    output [1:0]  in1_in2,
    output [15:0] led
);
    localparam CLK_PER_US = (CLK_PER_MS >= 1000) ? CLK_PER_MS / 1000 : 1;
    localparam BTN_DEB    = CLK_PER_MS * 10 - 1;   // 10ms (버튼, 로터리 키)
    localparam ROT_DEB    = CLK_PER_MS * 2  - 1;   // 2ms  (로터리 s1/s2, 실습값)

    localparam IDLE = 2'd0, COOK = 2'd1, PAUSE = 2'd2, DONE = 2'd3;
    localparam SND_NONE = 3'd0, SND_BEEP = 3'd1, SND_TICK = 3'd2, SND_OPEN = 3'd3,
               SND_CLOSE = 3'd4, SND_ERROR = 3'd5, SND_FINISH = 3'd6;

    // =========================================================
    // 1. tick (1us, 1ms)  - 실습 tick_generator 방식
    // =========================================================
    reg [23:0] r_us_cnt, r_ms_cnt;
    reg        w_tick_us, w_tick_ms;
    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_us_cnt <= 0; r_ms_cnt <= 0; w_tick_us <= 0; w_tick_ms <= 0;
        end else begin
            if (r_us_cnt >= CLK_PER_US-1) begin r_us_cnt <= 0; w_tick_us <= 1; end
            else begin r_us_cnt <= r_us_cnt + 1; w_tick_us <= 0; end
            if (r_ms_cnt >= CLK_PER_MS-1) begin r_ms_cnt <= 0; w_tick_ms <= 1; end
            else begin r_ms_cnt <= r_ms_cnt + 1; w_tick_ms <= 0; end
        end
    end

    // =========================================================
    // 2. 디바운서 (btn_debounce.v)
    // =========================================================
    wire [3:0] w_btn_clean;
    wire       w_s1, w_s2, w_key;

    debouncer #(.DEBOUNCE_LIMIT(BTN_DEB)) u_btnU_deb (.clk(clk), .reset(reset), .noisy_btn(btnU), .clean_btn(w_btn_clean[0]));
    debouncer #(.DEBOUNCE_LIMIT(BTN_DEB)) u_btnD_deb (.clk(clk), .reset(reset), .noisy_btn(btnD), .clean_btn(w_btn_clean[1]));
    debouncer #(.DEBOUNCE_LIMIT(BTN_DEB)) u_btnL_deb (.clk(clk), .reset(reset), .noisy_btn(btnL), .clean_btn(w_btn_clean[2]));
    debouncer #(.DEBOUNCE_LIMIT(BTN_DEB)) u_btnR_deb (.clk(clk), .reset(reset), .noisy_btn(btnR), .clean_btn(w_btn_clean[3]));
    debouncer #(.DEBOUNCE_LIMIT(ROT_DEB)) u_s1_deb   (.clk(clk), .reset(reset), .noisy_btn(s1),   .clean_btn(w_s1));
    debouncer #(.DEBOUNCE_LIMIT(ROT_DEB)) u_s2_deb   (.clk(clk), .reset(reset), .noisy_btn(s2),   .clean_btn(w_s2));
    debouncer #(.DEBOUNCE_LIMIT(BTN_DEB)) u_key_deb  (.clk(clk), .reset(reset), .noisy_btn(key),  .clean_btn(w_key));

    // 버튼 rising edge 검출 (실습 pwm_duty_control 방식)
    // KY-040 SW(key) 는 active-low(누르면 0) -> 반전해서 사용
    reg  [3:0] r_btn_prev;
    reg        r_key_prev;
    wire       w_key_pressed = ~w_key;
    wire [3:0] w_btn_pulse   = w_btn_clean & ~r_btn_prev;
    wire       w_key_pulse   = w_key_pressed & ~r_key_prev;
    wire w_start  = w_btn_pulse[0];   // btnU
    wire w_cancel = w_btn_pulse[1];   // btnD
    wire w_door   = w_btn_pulse[2];   // btnL
    wire w_power  = w_btn_pulse[3];   // btnR

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_btn_prev <= 4'b0;
            r_key_prev <= 1'b1;           // 리셋 직후 가짜 눌림 방지
        end else begin
            r_btn_prev <= w_btn_clean;
            r_key_prev <= w_key_pressed;
        end
    end

    // =========================================================
    // 3. 로터리 디코더 (encoder_buzzer.v)
    // =========================================================
    wire w_cw, w_ccw;
    rotary_decoder u_rotary (
        .clk(clk), .reset(reset), .s1(w_s1), .s2(w_s2),
        .cw_pulse(w_cw), .ccw_pulse(w_ccw));

    // =========================================================
    // 4. 메인 FSM
    // =========================================================
    reg [1:0]  r_state;
    reg [6:0]  r_min;        // 0~99
    reg [5:0]  r_sec;        // 0~59
    reg        r_edit_min;   // 1: 분 설정, 0: 초 설정
    reg        r_door_open;
    reg [2:0]  r_power;      // 1~5
    reg [2:0]  r_snd;        // 사운드 요청 (1클럭 펄스)
    reg [11:0] r_ms;         // 상태 내부 ms 카운터

    // ---- 시간 계산 함수 ({분,초} 13bit) ----
    function [12:0] f_inc;               // a초 증가 (a<60), 최대 99:59
        input [6:0] m; input [5:0] s; input [6:0] a;
        reg [6:0] ss; reg [7:0] mm;
        begin
            ss = s + a; mm = m;
            if (ss >= 60) begin ss = ss - 60; mm = m + 1; end
            if (mm > 99) f_inc = {7'd99, 6'd59};
            else         f_inc = {mm[6:0], ss[5:0]};
        end
    endfunction

    function [12:0] f_dec;               // a초 감소 (a<60), 최소 00:00
        input [6:0] m; input [5:0] s; input [6:0] a;
        reg [6:0] ss;
        begin
            if (s >= a)      f_dec = {m, s - a[5:0]};
            else if (m != 0) begin ss = s + 60 - a; f_dec = {m - 7'd1, ss[5:0]}; end
            else             f_dec = 13'd0;
        end
    endfunction

    wire        w_zero = (r_min == 0) && (r_sec == 0);
    wire [12:0] w_rot  = w_cw ? (r_edit_min ? ((r_min < 99) ? {r_min + 7'd1, r_sec} : {7'd99, 6'd59})
                                            : f_inc(r_min, r_sec, SEC_STEP))
                              : (r_edit_min ? ((r_min != 0) ? {r_min - 7'd1, r_sec} : 13'd0)
                                            : f_dec(r_min, r_sec, SEC_STEP));
    wire [12:0] w_dec1 = f_dec(r_min, r_sec, 7'd1);

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_state <= IDLE; r_min <= 0; r_sec <= 0; r_edit_min <= 1'b0;
            r_door_open <= 1'b0; r_power <= 3'd3; r_snd <= SND_NONE; r_ms <= 0;
        end else begin
            r_snd <= SND_NONE;

            // ---- 공통 : 모터 출력 단계 (요구 6) ----
            if (w_power) begin
                r_power <= (r_power >= 3'd5) ? 3'd1 : r_power + 3'd1;
                r_snd   <= SND_BEEP;
            end
            // ---- 공통 : 로터리 키 -> 분/초 모드 전환 (요구 1) ----
            if (w_key_pulse) begin
                r_edit_min <= ~r_edit_min;
                r_snd      <= SND_BEEP;
            end
            // ---- 공통 : 로터리 회전 -> 시간 설정 (요구 1, DONE 제외) ----
            if ((w_cw || w_ccw) && r_state != DONE) begin
                {r_min, r_sec} <= w_rot;
                r_snd          <= SND_TICK;
            end

            case (r_state)
            // ----------------------------------------------- IDLE
            IDLE: begin
                if (w_start) begin
                    if (r_door_open || w_zero) r_snd <= SND_ERROR;   // 문 열림 / 시간 0
                    else begin r_state <= COOK; r_ms <= 0; r_snd <= SND_BEEP; end
                end
                if (w_cancel) begin                                  // 설정 시간 클리어
                    r_min <= 0; r_sec <= 0; r_edit_min <= 1'b0; r_snd <= SND_BEEP;
                end
                if (w_door) begin                                    // 요구 5
                    r_door_open <= ~r_door_open;
                    r_snd <= r_door_open ? SND_CLOSE : SND_OPEN;     // 요구 2-1
                end
            end
            // ----------------------------------------------- COOK
            COOK: begin
                if (w_zero) begin                                    // 시간 종료 (요구 2-3)
                    r_state <= DONE; r_ms <= 0; r_snd <= SND_FINISH;
                end else if (w_tick_ms) begin                        // 1초마다 -1
                    if (r_ms >= 999) begin r_ms <= 0; {r_min, r_sec} <= w_dec1; end
                    else r_ms <= r_ms + 1;
                end
                if (w_start)  begin r_state <= PAUSE; r_ms <= 0; r_snd <= SND_BEEP; end
                if (w_cancel) begin r_state <= IDLE; r_min <= 0; r_sec <= 0; r_snd <= SND_BEEP; end  // 요구 4
                if (w_door) begin                                    // 조리 중 문 열림 -> 정지
                    r_door_open <= 1'b1; r_state <= PAUSE; r_ms <= 0; r_snd <= SND_OPEN;
                end
            end
            // ----------------------------------------------- PAUSE
            PAUSE: begin
                if (w_tick_ms) r_ms <= (r_ms >= 999) ? 12'd0 : r_ms + 1;   // 깜빡임용
                if (w_start) begin
                    if (r_door_open) r_snd <= SND_ERROR;
                    else if (w_zero) begin r_state <= IDLE; r_snd <= SND_ERROR; end
                    else begin r_state <= COOK; r_ms <= 0; r_snd <= SND_BEEP; end
                end
                if (w_cancel) begin r_state <= IDLE; r_min <= 0; r_sec <= 0; r_snd <= SND_BEEP; end  // 요구 4
                if (w_door) begin
                    r_door_open <= ~r_door_open;
                    r_snd <= r_door_open ? SND_CLOSE : SND_OPEN;
                end
            end
            // ----------------------------------------------- DONE
            DONE: begin
                r_min <= 0; r_sec <= 0;
                if (w_tick_ms) begin
                    if (r_ms >= DONE_MS-1) begin r_state <= IDLE; r_ms <= 0; end
                    else r_ms <= r_ms + 1;
                end
                if (w_start || w_cancel) begin r_state <= IDLE; r_snd <= SND_BEEP; end
                if (w_door) begin
                    r_door_open <= ~r_door_open; r_state <= IDLE;
                    r_snd <= r_door_open ? SND_CLOSE : SND_OPEN;
                end
            end
            endcase
        end
    end

    // ---- Moore 출력 ----
    wire w_motor_on = (r_state == COOK);
    wire w_blink_on = (r_state == PAUSE) ? (r_ms < 500) :
                      (r_state == DONE)  ? ((r_ms < 500) ||                      // 0 을 3회 ON/OFF
                                            (r_ms >= 1000 && r_ms < 1500) ||
                                            (r_ms >= 2000 && r_ms < 2500)) : 1'b1;

    // =========================================================
    // 5. 부저 (encoder_buzzer.v)
    // =========================================================
    sound_player #(.CLK_HZ(CLK_PER_MS * 1000)) u_sound (
        .clk(clk), .reset(reset), .tick_ms(w_tick_ms),
        .i_req(r_snd), .o_buzzer(buzzer), .o_busy());

    // =========================================================
    // 6. 서보(문) + DC 모터 PWM (servo_ctrl.v)
    // =========================================================
    servo_ctrl u_servo (
        .clk(clk), .reset(reset), .tick_us(w_tick_us), .tick_ms(w_tick_ms),
        .i_open(r_door_open), .o_pwm(servo_pwm), .o_width_us());

    motor_pwm u_motor (
        .clk(clk), .reset(reset), .tick_us(w_tick_us),
        .i_run(w_motor_on), .i_level(r_power),
        .o_pwm(motor_pwm), .o_in1_in2(in1_in2), .o_duty());

    // =========================================================
    // 7. FND (fnd_controller.v)
    // =========================================================
    fnd_controller u_fnd (
        .clk(clk), .reset(reset), .tick_ms(w_tick_ms),
        .i_state(r_state), .i_min(r_min), .i_sec(r_sec), .i_blink_on(w_blink_on),
        .an(an), .seg(seg));

    // =========================================================
    // 8. LED
    // [15] 문 열림 [14] 모터 PWM [13] 일시정지 [12] 완료
    // [11] 분 설정모드 [10] 초 설정모드 [4:0] 모터 출력 단계
    // =========================================================
    assign led[15]  = r_door_open;
    assign led[14]  = motor_pwm;
    assign led[13]  = (r_state == PAUSE);
    assign led[12]  = (r_state == DONE);
    assign led[11]  = r_edit_min;
    assign led[10]  = ~r_edit_min;
    assign led[9:5] = 5'b0;
    assign led[4:0] = (5'b11111 >> (3'd5 - r_power));
endmodule
