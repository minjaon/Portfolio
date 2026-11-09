`timescale 1ns / 1ps
// ============================================================
// encoder_buzzer.v : 로터리 엔코더 디코더 + 부저 사운드
// ============================================================
// ============================================================
// rotary_decoder : 로터리 엔코더(KY-040) s1/s2 -> 한 칸(detent)당 1펄스
//   실습 rotary.v 의 상태천이 표를 그대로 사용
//     CW  (+1) : 00->10, 10->11, 11->01, 01->00
//     CCW (-1) : 00->01, 01->11, 11->10, 10->00
//   개선점 : 정지(detent) 상태(REST_STATE)로 돌아올 때 누적값 부호로 방향 판정
//            -> 채터링/반쯤 돌린 경우/전원 투입 직후 오동작 방지
//   KY-040 은 풀업이라 평소 {s1,s2}=11 에서 멈춤 (REST_STATE=2'b11)
//   ※ 방향이 반대로 나오면 xdc 에서 s1/s2 핀만 바꾸면 됨
// ============================================================
module rotary_decoder #(
    parameter [1:0] REST_STATE = 2'b11
) (
    input      clk,
    input      reset,
    input      s1,
    input      s2,
    output reg cw_pulse,     // 시계방향 1칸
    output reg ccw_pulse     // 반시계방향 1칸
);
    reg [1:0]        r_prev;
    reg signed [3:0] r_acc;          // 전이 누적 (-8 ~ +7)
    wire [1:0]       w_cur = {s1, s2};

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_prev    <= REST_STATE;
            r_acc     <= 0;
            cw_pulse  <= 1'b0;
            ccw_pulse <= 1'b0;
        end else begin
            cw_pulse  <= 1'b0;
            ccw_pulse <= 1'b0;
            if (w_cur != r_prev) begin
                r_prev <= w_cur;
                if (w_cur == REST_STATE) begin
                    // detent 도착 : 2칸 이상 같은 방향으로 움직였으면 1펄스
                    // (정상 1칸 = 3번 전이 후 REST 도착 -> acc=±3, 튐 1번은 ±1 이라 무시)
                    if (r_acc >= 2)       cw_pulse  <= 1'b1;
                    else if (r_acc <= -2) ccw_pulse <= 1'b1;
                    r_acc <= 0;
                end else begin
                    case ({r_prev, w_cur})
                        4'b0010, 4'b1011, 4'b1101, 4'b0100: r_acc <= r_acc + 1;  // CW
                        4'b0001, 4'b0111, 4'b1110, 4'b1000: r_acc <= r_acc - 1;  // CCW
                        default: ;                                               // 무효(2비트 동시변화)
                    endcase
                end
            end
        end
    end
endmodule

// ============================================================
// sound_player : 수동(passive) 부저용 효과음/멜로디 시퀀서
//   - 실습 play_melody 의 "카운터로 반주기마다 토글" 방식 재사용
//   - 음표 ROM(note, 길이ms)을 1ms tick 으로 순서대로 재생
//   - 새 요청이 오면 현재 소리를 끊고 새 소리 재생
//   i_req : 1 BEEP(버튼) 2 TICK(로터리) 3 OPEN(문열림) 4 CLOSE(문닫힘)
//           5 ERROR 6 FINISH(종료 멜로디)
// ============================================================
module sound_player #(
    parameter CLK_HZ = 100_000_000
) (
    input        clk,
    input        reset,
    input        tick_ms,
    input  [2:0] i_req,
    output       o_buzzer,
    output       o_busy
);
    // ---- 음 코드 ----
    localparam N_REST = 4'd0,  N_A4 = 4'd1,  N_C5 = 4'd2,  N_D5 = 4'd3,  N_E5 = 4'd4,
               N_F5   = 4'd5,  N_G5 = 4'd6,  N_A5 = 4'd7,  N_B5 = 4'd8,  N_C6 = 4'd9,
               N_BEEP = 4'd10, N_TICK = 4'd11, N_END = 4'd15;

    // ---- 반주기 카운트 = CLK_HZ / (2*f) ----
    localparam HP_A4   = CLK_HZ / 880;    // 440.00Hz
    localparam HP_C5   = CLK_HZ / 1047;   // 523.25Hz
    localparam HP_D5   = CLK_HZ / 1175;   // 587.33Hz
    localparam HP_E5   = CLK_HZ / 1319;   // 659.26Hz
    localparam HP_F5   = CLK_HZ / 1397;   // 698.46Hz
    localparam HP_G5   = CLK_HZ / 1568;   // 783.99Hz
    localparam HP_A5   = CLK_HZ / 1760;   // 880.00Hz
    localparam HP_B5   = CLK_HZ / 1976;   // 987.77Hz
    localparam HP_C6   = CLK_HZ / 2093;   // 1046.5Hz
    localparam HP_BEEP = CLK_HZ / 4000;   // 2kHz  삑
    localparam HP_TICK = CLK_HZ / 8000;   // 4kHz  틱

    reg [2:0]  r_id;
    reg [3:0]  r_idx;
    reg [9:0]  r_cnt;
    reg        r_active;
    reg [19:0] r_tone_cnt;
    reg        r_tone;

    // ---------------- 음표 ROM : {note[3:0], dur_ms[9:0]} ----------------
    reg [13:0] w_rom;
    always @(*) begin
        w_rom = {N_END, 10'd0};
        case ({r_id, r_idx})
            // BEEP : 삑 (80ms)
            {3'd1, 4'd0}: w_rom = {N_BEEP, 10'd80};
            // TICK : 틱 (15ms)
            {3'd2, 4'd0}: w_rom = {N_TICK, 10'd15};
            // OPEN : 도-미-솔 (상승)
            {3'd3, 4'd0}: w_rom = {N_C5, 10'd100};
            {3'd3, 4'd1}: w_rom = {N_E5, 10'd100};
            {3'd3, 4'd2}: w_rom = {N_G5, 10'd180};
            // CLOSE : 솔-미-도 (하강)
            {3'd4, 4'd0}: w_rom = {N_G5, 10'd100};
            {3'd4, 4'd1}: w_rom = {N_E5, 10'd100};
            {3'd4, 4'd2}: w_rom = {N_C5, 10'd180};
            // ERROR : 낮은 라 2번 (뿌-뿌)
            {3'd5, 4'd0}: w_rom = {N_A4,   10'd150};
            {3'd5, 4'd1}: w_rom = {N_REST, 10'd60};
            {3'd5, 4'd2}: w_rom = {N_A4,   10'd150};
            // FINISH : 종료 멜로디 (FND 0 깜빡임 3회와 함께, 약 2.9초)
            {3'd6, 4'd0}:  w_rom = {N_C5,   10'd150};
            {3'd6, 4'd1}:  w_rom = {N_E5,   10'd150};
            {3'd6, 4'd2}:  w_rom = {N_G5,   10'd150};
            {3'd6, 4'd3}:  w_rom = {N_C6,   10'd300};
            {3'd6, 4'd4}:  w_rom = {N_REST, 10'd100};
            {3'd6, 4'd5}:  w_rom = {N_A5,   10'd150};
            {3'd6, 4'd6}:  w_rom = {N_F5,   10'd150};
            {3'd6, 4'd7}:  w_rom = {N_A5,   10'd150};
            {3'd6, 4'd8}:  w_rom = {N_C6,   10'd300};
            {3'd6, 4'd9}:  w_rom = {N_REST, 10'd100};
            {3'd6, 4'd10}: w_rom = {N_G5,   10'd150};
            {3'd6, 4'd11}: w_rom = {N_B5,   10'd150};
            {3'd6, 4'd12}: w_rom = {N_D5,   10'd150};
            {3'd6, 4'd13}: w_rom = {N_C6,   10'd600};
            default:       w_rom = {N_END,  10'd0};
        endcase
    end
    wire [3:0] w_note = w_rom[13:10];
    wire [9:0] w_dur  = w_rom[9:0];

    // ---------------- 시퀀서 ----------------
    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_id <= 0; r_idx <= 0; r_cnt <= 0; r_active <= 1'b0;
        end else if (i_req != 3'd0) begin            // 새 요청 -> 처음부터
            r_id <= i_req; r_idx <= 0; r_cnt <= 0; r_active <= 1'b1;
        end else if (r_active) begin
            if (w_note == N_END) r_active <= 1'b0;
            else if (tick_ms) begin
                if (r_cnt >= w_dur - 1) begin r_cnt <= 0; r_idx <= r_idx + 1; end
                else r_cnt <= r_cnt + 1;
            end
        end
    end

    // ---------------- 톤 발생기 (반주기마다 토글 -> duty 50%) ----------------
    reg [19:0] w_half;
    always @(*) begin
        case (w_note)
            N_A4: w_half = HP_A4;   N_C5: w_half = HP_C5;   N_D5: w_half = HP_D5;
            N_E5: w_half = HP_E5;   N_F5: w_half = HP_F5;   N_G5: w_half = HP_G5;
            N_A5: w_half = HP_A5;   N_B5: w_half = HP_B5;   N_C6: w_half = HP_C6;
            N_BEEP: w_half = HP_BEEP;
            N_TICK: w_half = HP_TICK;
            default: w_half = 0;
        endcase
    end
    wire w_sound = r_active && (w_half != 0);

    always @(posedge clk, posedge reset) begin
        if (reset) begin
            r_tone_cnt <= 0; r_tone <= 1'b0;
        end else if (!w_sound) begin
            r_tone_cnt <= 0; r_tone <= 1'b0;
        end else if (r_tone_cnt >= w_half - 1) begin
            r_tone_cnt <= 0; r_tone <= ~r_tone;
        end else
            r_tone_cnt <= r_tone_cnt + 1;
    end

    assign o_buzzer = r_tone;
    assign o_busy   = r_active;
endmodule
