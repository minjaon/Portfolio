/*
 * fnd.h
 *
 * Created: 2026-06-12 오전 10:46:08
 * Author: kccistc
 */ 

#define F_CPU 16000000UL // 이 프로젝트 헤더를 쓰는 모든 곳의 메인 클럭 속도는 16MHz라고 정의해둠!
#include <avr/io.h>
#include <util/delay.h>


// 타이머용 4자리 FND의 자릿수 선택(Digit) 제어선이 포트F의 4,5,6,7번 구멍에 연결되어 있다고 명찰 붙이기!
#define FND_DIGIT_DDR  DDRF
#define FND_DIGIT_PORT PORTF
#define FND_DIGIT_D1   4  // PF4
#define FND_DIGIT_D2   5  // PF5
#define FND_DIGIT_D3   6  // PF6
#define FND_DIGIT_D4   7  // PF7

// FND의 각 LED 날개(A~DP)들을 일제히 제어하는 데이터선 8 가닥은 PORTC 전체에 통째로 꽂혀있다고 선언!
#define FND_DATA_DDR  DDRC
#define FND_DATA_PORT PORTC

// 가독성을 위한 스톱워치 상태용 명찰 세트
#define STOP 0
#define RUN  1
#define RESET 2

// [주의 문구]: 회로 설계상 PORTD도 함께 비트를 찢어 쓰기 때문에 fnd_display 함수 내부에서 가공 처리가 들어갔음을 알려주는 친절한 힌트 주석이야!

// 외부에 있는 main.c 같은 다른 파일들이 "나 fnd 파일에 들어있는 함수들 좀 빌려 쓸게!" 할 수 있도록 통행증(함수 원형 선언)을 발행해 놓은 서류단이야.
void init_fnd(void);
void fnd_display(void);
void init_timer2(void);