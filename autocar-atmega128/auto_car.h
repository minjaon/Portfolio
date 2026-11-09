/*
 * auto_car.h
 * 수정본: 실제 하드웨어 연결 정보 반영
 */

// MCU(칩)의 메인 시계 속도를 16MHz(16000000)로 설정하겠다는 뜻이야. 지연 함수(_delay_ms) 쓸 때 필수지!
#define F_CPU 16000000UL 

#include <avr/io.h>        // AVR 입출력 포트(PORT, DDR) 정의를 가져와
#include <util/delay.h>     // 잠깐 시간 때우기용 시간 지연 함수(_delay_ms)를 쓰려고 가져와
#include <avr/interrupt.h>  // 타이머나 인터럽트 기능 기능을 켜려고 가져와
#include <stdio.h>         // 기본적인 표준 입출력 함수 쓰려고 가져와

// ========== 버튼 (PG1) ==========
#define BTN_DDR    DDRG    // 버튼이 연결된 포트G의 방향 설정(DDR) 명찰
#define BTN_PIN    PING    // 버튼의 현재 찔러넣어지는 전압 상태(0 또는 1)를 읽는 명찰
#define BTN_BIT    1       // PORTG의 1번 핀(PG1)을 버튼으로 쓰겠어!
#define BUTTON_PRESS   1   // 코딩할 때 헷갈리지 않게 '버튼 눌림'을 숫자 1로 약속한 거야
#define BUTTON_RELEASE 0   // '버튼 안 눌림'을 숫자 0으로 약속한 거야

// ========== 자율주행 LED (PG2, PG3) ==========
#define AUTO_LED_DDR   DDRG     // 자율주행 전용 LED들의 방향 설정 명찰이야
#define AUTO_LED_PORT  PORTG    // LED에 불을 켜고(1) 끄는(0) 제어 포트 명찰
#define AUTO_LED_BIT2  2        // PORTG의 2번 핀(PG2)에 LED 하나!
#define AUTO_LED_BIT3  3        // PORTG의 3번 핀(PG3)에 LED 또 하나!

// ========== 모터 드라이버 (PF0~3, PB5~6) ==========
#define MOTOR_DDR      DDRF    // 바퀴 모터 방향 제어 핀들이 모여있는 포트F 방향 명찰
#define MOTOR_PORT     PORTF   // 바퀴 모터에 실제로 전기를 쏴주는 포트F 출력 명찰
// 주석: 모터 제어칩(L298 등)의 IN1~IN4 핀이 메인보드 PF0~PF3에 꽂혀있다는 뜻!

// ========== FND 및 기타 핀 정의 ==========
#define FND_DIGIT_DDR  DDRF    // 타이머 FND 자릿수 결정하는 포트F의 방향 세팅
#define FND_SEG_DDR_A  DDRC    // 상태 FND의 상위 4개 핀(A,B,C,D)용 포트C 세팅 (위에서 말한 포트D랑 역할이 겹칠 수 있으니 참고!)
#define FND_SEG_DDR_B  DDRD    // 상태 FND의 하위 핀용 세팅 명찰