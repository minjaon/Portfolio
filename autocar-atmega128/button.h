/*
 * button.h
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

// --- 기존 PORTD 버튼 정의 ---
#define  BUTTON_DDR  DDRD
#define  BUTTON_PIN  PIND
#define  BUTTON0PIN 4
#define  BUTTON1PIN 5
#define  BUTTON2PIN 6
#define  BUTTON3PIN 7

#define  BUTTON0    0
#define  BUTTON1    1
#define  BUTTON2    2
#define  BUTTON3    3

// --- 💡 추가: PG1 스위치 정의 ---
#define  BUTTON4PIN 1        // PG1 핀 번호
#define  BUTTON4    4        // 가상 인덱스
#define  BUTTON_NUMBER 5     // 버튼 개수를 4개에서 5개로 늘립니다.

// 상태 정의
#define  BUTTON_PRESS   1
#define  BUTTON_RELEASE 0

// Active-Low 스위치(PG1)를 위한 별도 매크로 추가
#define  IS_BUTTON4_PRESSED (!(PING & (1 << BUTTON4PIN)))