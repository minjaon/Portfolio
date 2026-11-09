`timescale 1ns / 1ps
// ============================================================
// debouncer : 실습 btn_debounce10ms.v 의 debouncer 모듈 그대로 재사용
//   noisy_btn 이 DEBOUNCE_LIMIT 클럭 동안 안정되면 clean_btn 갱신
//   100MHz 에서 999_999 -> 10ms, 199_999 -> 2ms
//   (※ 기존 btn_debounce10ms.v 를 같이 프로젝트에 넣으면 모듈명이 중복되니
//      이 파일만 추가하세요)
// ============================================================
module debouncer #(parameter DEBOUNCE_LIMIT = 20'd999_999) (
    input      clk,
    input      reset,
    input      noisy_btn,
    output reg clean_btn
);
    reg [19:0] r_count;
    reg        r_btn_state;

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            r_count     <= 0;
            r_btn_state <= 0;
            clean_btn   <= 0;
        end else if (noisy_btn == r_btn_state) begin   // 안정 상태
            r_count <= 0;
        end else begin
            if (r_count < DEBOUNCE_LIMIT)              // 변했지만 아직 불안정
                r_count <= r_count + 1;
            else begin                                 // 충분히 유지됨
                r_btn_state <= noisy_btn;
                clean_btn   <= noisy_btn;
                r_count     <= 0;
            end
        end
    end
endmodule
