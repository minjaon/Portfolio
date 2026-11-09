
#include "pwm.h"
#include <util/delay.h>
#include "auto_car.h"
#include <stdint.h>
void set_auto_led(int state);
int get_auto_button(void);
void init_auto_car(void);
extern volatile uint32_t msec_count;
#include "pwm.h"
#include "fnd.h"

void auto_drive_control(void);
// FND 파일 혹은 초음파 파일에서 선언된 주행 상태 변수 가져오기
extern volatile uint8_t stopwatch;
#define RUN  1
#define STOP 0


// ultrasonic.c의 진짜 거리 변수들을 외부에서 참조
extern volatile int dist_left;
extern volatile int dist_front;
extern volatile int dist_right;
extern volatile uint8_t stopwatch;


// 주행 상태 정의
static uint8_t car_state = 0;
#define STATE_FORWARD  0
#define STATE_EVADE    1
// 초음파 센서 측정 결과 변수가 있다고 가정 (단위: cm)
extern volatile int distance_center;
extern volatile int distance_left;
extern volatile int distance_right;

// [함수 1] 자동차 시스템의 첫 시동을 걸어주는 초기화 함수야! (main 시작할 때 딱 한 번 실행해)
void init_auto_car(void)
{
	// 버튼 핀(PG1)을 '입력 모드(0)'로 만들어줘야 전기 신호를 읽을 수 있어! (&~ 연산으로 1번 비트만 0으로 만듦)
	BTN_DDR &= ~(1 << BTN_BIT);

	// 자율주행 표시용 LED 핀 2개(PG2, PG3)를 '출력 모드(1)'로 세팅해줘 (| 연산으로 비트를 1로 만듦)
	AUTO_LED_DDR |= (1 << AUTO_LED_BIT2);
	AUTO_LED_DDR |= (1 << AUTO_LED_BIT3);
	
	// 처음에 시동 켰을 땐 LED가 꺼져있어야 하니까 전기를 끊어줘(0으로 만듦)
	AUTO_LED_PORT &= ~(1 << AUTO_LED_BIT2);
	AUTO_LED_PORT &= ~(1 << AUTO_LED_BIT3);
}

// [함수 2] 버튼이 제대로 눌렸는지 확인하는 함수야 (디바운싱 기능 포함)
// 디바운싱이 뭐냐면, 사람이 손으로 버튼을 누르면 전기 신호가 '파르르' 떨리면서 여러 번 누른 걸로 오작동해.
// 그걸 막으려고 "최소 20ms 동안 꾹 눌려있었는지 가려내는" 안전장치 코드야!
int get_auto_button(void)
{
	static unsigned char stable = BUTTON_RELEASE;   // 흔들림 없이 확실하게 안정된 버튼 상태 저장 방
	static unsigned char last_raw = BUTTON_RELEASE; // 바로 직전에 측정했던 날것 그대로의 버튼 상태 방
	static uint32_t change_t = 0;                  // 버튼 상태가 바뀐 시점의 시간 기록 방

	// 현재 포트G의 1번 핀(PG1)에 전기 신호가 들어왔는지 확인해서 날것(raw) 변수에 1 또는 0을 넣어
	unsigned char raw = (BTN_PIN & (1 << BTN_BIT)) ? BUTTON_PRESS : BUTTON_RELEASE;

	// 만약 지금 읽은 버튼 상태가 직전 상태랑 다르다면? (사람이 손을 대기 시작했다는 뜻!)
	if (raw != last_raw)
	{
		last_raw = raw;       // 직전 상태를 새로 업데이트하고
		change_t = msec_count; // 상태가 바뀐 '현재 시간'을 타이머에서 가져와 기록해둬
	}

	// "상태가 바뀐 지 20ms 이상 지났고" AND "확정된 안정이 현재 상태랑 다르면" -> 진짜 버튼 상황이 변한 거야!
	if ((msec_count - change_t) >= 20 && raw != stable)
	{
		stable = raw; // 이제야 비로소 "버튼 상태가 제대로 바뀌었군!" 하고 확정(stable) 지어줘.
		
		// 만약 확정된 상태가 '손을 뗀 순간(RELEASE)' 이라면?
		if (stable == BUTTON_RELEASE)
		return 1;  // "버튼을 꾹 눌렀다가 뗀 순간"을 최종 감지해서 메인 코드한테 '1(딸깍 성공)'을 던져줘!
	}

	return 0; // 버튼이 안 눌렸거나 파르르 떨리는 중이면 그냥 0을 돌려줘
}

// [함수 3] 자율주행 LED를 켜고 끄는 리모컨 함수야
// 외부에서 set_auto_led(1); 하면 켜지고, set_auto_led(0); 하면 꺼져서 편해!
void set_auto_led(int state)
{
	if (state) { // 전달받은 state가 1(참)이면?
		AUTO_LED_PORT |= (1 << AUTO_LED_BIT2); // PG2 핀에 전기 줘서 LED ON!
		AUTO_LED_PORT |= (1 << AUTO_LED_BIT3); // PG3 핀에 전기 줘서 LED ON!
		} else {     // 전달받은 state가 0(거짓)이면?
		AUTO_LED_PORT &= ~(1 << AUTO_LED_BIT2); // PG2 핀 전기 끊어서 LED OFF!
		AUTO_LED_PORT &= ~(1 << AUTO_LED_BIT3); // PG3 핀 전기 끊어서 LED OFF!
	}
}