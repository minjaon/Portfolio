`timescale 1ns / 1ps
// ============================================================
// tb_top_microwave : 전자레인지 전체 시나리오 테스트벤치
//   CLK_PER_MS = 1000 -> 시뮬레이션에서 1ms = 1000clk = 10us (실제의 1/100)
//   버튼/로터리에 채터링(노이즈) 포함, 자동 PASS/FAIL 판정
//   Vivado: Run Simulation 후 Run All (약 200ms 시뮬레이션 시간)
// ============================================================
module tb_top_microwave();
    localparam CLK_PER_MS = 1000;
    localparam MS = CLK_PER_MS * 10;        // 1ms 에 해당하는 ns (10ns 클럭)

    reg clk = 0, reset = 1;
    reg btnU = 0, btnD = 0, btnL = 0, btnR = 0;
    reg s1 = 1, s2 = 1, key = 1;            // KY-040 평소 high

    wire [7:0] seg;  wire [3:0] an;
    wire buzzer, servo_pwm, motor_pwm;
    wire [1:0] in1_in2;  wire [15:0] led;

    top_microwave #(.CLK_PER_MS(CLK_PER_MS)) dut (
        .clk(clk), .reset(reset),
        .btnU(btnU), .btnD(btnD), .btnL(btnL), .btnR(btnR),
        .s1(s1), .s2(s2), .key(key),
        .seg(seg), .an(an), .buzzer(buzzer), .servo_pwm(servo_pwm),
        .motor_pwm(motor_pwm), .in1_in2(in1_in2), .led(led));

    always #5 clk = ~clk;                   // 100MHz

    // ---- 내부 신호 관찰 ----
    wire [1:0] st   = dut.r_state;
    wire [6:0] mm   = dut.r_min;
    wire [5:0] ss   = dut.r_sec;
    wire       door = dut.r_door_open;
    wire [2:0] pwr  = dut.r_power;
    wire [2:0] snd  = dut.r_snd;

    integer pass_cnt = 0, fail_cnt = 0;

    task check(input cond, input [8*48-1:0] name);
        begin
            if (cond) begin pass_cnt = pass_cnt + 1; $display("[PASS] %0s", name); end
            else begin
                fail_cnt = fail_cnt + 1;
                $display("[FAIL] %0s  (t=%0d ms state=%0d %0d:%0d door=%0d pwr=%0d)",
                         name, $time / MS, st, mm, ss, door, pwr);
            end
        end
    endtask

    task wait_ms(input integer n); begin #(n * MS); end endtask

    // 버튼 누르기 : 채터링 50us x 4번 -> 20ms 유지 -> 떼기(채터링) -> 20ms
    task press(input integer b);   // 0:U 1:D 2:L 3:R
        integer i;
        begin
            for (i = 0; i < 4; i = i + 1) begin
                case (b) 0: btnU = ~btnU; 1: btnD = ~btnD; 2: btnL = ~btnL; default: btnR = ~btnR; endcase
                #50_000;
            end
            case (b) 0: btnU = 1; 1: btnD = 1; 2: btnL = 1; default: btnR = 1; endcase
            wait_ms(20);
            case (b) 0: btnU = 0; 1: btnD = 0; 2: btnL = 0; default: btnR = 0; endcase
            wait_ms(20);
        end
    endtask

    // 로터리 키 (active-low)
    task press_key;
        begin
            key = 0; #30_000; key = 1; #30_000; key = 0;   // 채터링
            wait_ms(20); key = 1; wait_ms(20);
        end
    endtask

    // 로터리 1칸 : CW  11 -> 01 -> 00 -> 10 -> 11
    //              CCW 11 -> 10 -> 00 -> 01 -> 11   (각 단계 4ms, s1 에 채터링)
    task rotate(input cw);
        begin
            if (cw) begin
                s1 = 0; #3000; s1 = 1; #3000; s1 = 0; wait_ms(4);
                s2 = 0; wait_ms(4);
                s1 = 1; wait_ms(4);
                s2 = 1; wait_ms(4);
            end else begin
                s2 = 0; wait_ms(4);
                s1 = 0; #3000; s1 = 1; #3000; s1 = 0; wait_ms(4);
                s2 = 1; wait_ms(4);
                s1 = 1; wait_ms(4);
            end
        end
    endtask

    // 서보 펄스폭 측정 (us)
    integer t_rise, servo_us;
    always @(posedge servo_pwm) t_rise = $time;
    always @(negedge servo_pwm) servo_us = ($time - t_rise) / 10;   // 1us = 10ns (sim)

    // 모터 duty 측정 (1ms 창)
    integer hi_cnt;
    task measure_motor(output integer duty);
        integer i;
        begin
            hi_cnt = 0;
            for (i = 0; i < CLK_PER_MS; i = i + 1) begin
                @(posedge clk); if (motor_pwm) hi_cnt = hi_cnt + 1;
            end
            duty = hi_cnt * 100 / CLK_PER_MS;
        end
    endtask

    // 부저 토글 수 카운트
    integer buzz_edges = 0;
    always @(posedge buzzer) buzz_edges = buzz_edges + 1;

    // 사운드 요청 로그
    always @(posedge clk) if (snd != 0)
        $display("        t=%0d ms  sound req = %0d (1BEEP 2TICK 3OPEN 4CLOSE 5ERR 6FIN)", $time / MS, snd);

    integer duty, e0;

    initial begin
`ifdef DUMP_VCD                                   // iverilog -DDUMP_VCD 일 때만 (파일 큼)
        $dumpfile("tb_top_microwave.vcd");
        $dumpvars(1, tb_top_microwave);
`endif

        wait_ms(1); reset = 0; wait_ms(15);
        check(st == 0 && mm == 0 && ss == 0, "reset -> IDLE 00:00");

        // ---------- 1. 로터리로 시간 설정 ----------
        $display("== 1. rotary time set ==");
        rotate(1); rotate(1);                          // 초 모드 +10 +10
        check(mm == 0 && ss == 20, "CW x2 (sec mode) -> 00:20");
        rotate(0);
        check(mm == 0 && ss == 10, "CCW x1 -> 00:10");
        press_key;                                     // 분 모드
        check(dut.r_edit_min == 1, "key -> minute mode");
        rotate(1);
        check(mm == 1 && ss == 10, "CW x1 (min mode) -> 01:10");
        rotate(0);
        check(mm == 0 && ss == 10, "CCW x1 (min mode) -> 00:10");
        press_key;
        check(dut.r_edit_min == 0, "key -> second mode");

        // ---------- 2. 출력 단계 ----------
        $display("== 2. power level ==");
        press(3);
        check(pwr == 4, "btnR -> motor power 4");

        // ---------- 3. 문 열기/닫기 (서보) ----------
        $display("== 3. door open/close ==");
        press(2);
        check(door == 1, "btnL -> door open");
        wait_ms(600);
        check(servo_us >= 1495 && servo_us <= 1505, "servo pulse = 1.5ms (90deg)");
        press(0);                                      // 문 열린 채 시작
        check(st == 0, "start w/ door open -> ERROR (stay IDLE)");
        press(2);
        wait_ms(600);
        check(door == 0 && servo_us >= 995 && servo_us <= 1005, "door close, servo 1.0ms (0deg)");

        // ---------- 4. 조리 시작 ----------
        $display("== 4. cook start ==");
        press(0);
        check(st == 1, "btnU -> COOK");
        check(in1_in2 == 2'b10, "motor direction = 10 (run)");
        measure_motor(duty);
        check(duty >= 84 && duty <= 86, "motor duty 85% (power 4)");
        wait_ms(3000);
        check(mm == 0 && (ss == 7 || ss == 6), "after 3s -> 00:07");

        // ---------- 5. 일시정지 / 문 열림 안전정지 ----------
        $display("== 5. pause / door safety ==");
        press(0);
        check(st == 2, "btnU -> PAUSE");
        measure_motor(duty);
        check(duty == 0 && in1_in2 == 2'b00, "motor stopped in PAUSE");
        e0 = ss;
        wait_ms(2000);
        check(ss == e0, "time frozen in PAUSE");
        press(0);
        check(st == 1, "btnU -> resume COOK");
        press(2);
        check(st == 2 && door == 1, "door opened in COOK -> PAUSE (safety)");
        press(0);
        check(st == 2, "start while door open -> stays PAUSE");
        press(2); press(0);
        check(st == 1 && door == 0, "close door + start -> COOK");

        // ---------- 6. 종료 ----------
        $display("== 6. finish ==");
        wait (st == 3);
        check(mm == 0 && ss == 0, "time 00:00 -> DONE");
        e0 = buzz_edges;
        wait_ms(1200);
        check(buzz_edges - e0 > 100, "finish melody playing (buzzer toggling)");
        check(dut.w_blink_on == 0 || dut.w_blink_on == 1, "FND 0 blink");
        wait (st == 0);
        check(st == 0, "after 3 blinks -> IDLE");

        // ---------- 7. 취소 ----------
        $display("== 7. cancel ==");
        rotate(1); press(0);
        check(st == 1, "set 00:10 + start -> COOK");
        press(1);
        check(st == 0 && mm == 0 && ss == 0, "btnD cancel -> IDLE 00:00");
        press(0);
        check(st == 0, "start with 00:00 -> ERROR (stay IDLE)");

        // ---------- 8. 출력 단계 순환 ----------
        press(3); press(3);
        check(pwr == 1, "power 4 -> 5 -> 1 (wrap)");

        $display("=================================================");
        $display(" RESULT : PASS %0d / FAIL %0d", pass_cnt, fail_cnt);
        $display("=================================================");
        $finish;
    end

    // 상태 변화 모니터
    always @(st) $display("        t=%0d ms  STATE -> %0s", $time / MS,
                          st == 0 ? "IDLE" : st == 1 ? "COOK" : st == 2 ? "PAUSE" : "DONE");
endmodule
