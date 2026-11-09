/*
 * ultrasonic.h
 */ 
#ifndef ULTRASONIC_H_
#define ULTRASONIC_H_

#define F_CPU 16000000UL     // 메인 시계 속도 16MHz 세팅!
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// ========== 초음파 발사(TRIG) 핀 세팅 ==========
#define TRIG_DDR    DDRA     // 초음파를 쏠 신호(Trig)는 포트A의 방향을 따를 거야
#define TRIG_PORT  PORTA     // 초음파를 쏘라고 전기를 주는 포트A 명찰

// ========== 초음파 메아리(ECHO) 핀 세팅 ==========
#define ECHO_DDR    DDRE     // 돌아오는 신호(Echo)를 받을 포트E의 방향 명찰
#define ECHO_PORT  PINE      // Echo 핀으로 전기 신호가 들어오는지 읽는 명찰

// 각각의 센서가 포트A의 몇 번 구멍에 꽂혀있는지 정해주는 거야
#define TRIG_L     0   // 왼쪽 초음파 발사는 PA0 번 핀!
#define TRIG_F     1   // 정면 초음파 발사는 PA1 번 핀!
#define TRIG_R     2   // 오른쪽 초음파 발사는 PA2 번 핀!

// 각각의 메아리 핀이 포트E의 몇 번 구멍에 꽂혀있는지 정해주는 거야 (외부 인터럽트 기능 사용)
#define ECHO_L     4   // 왼쪽 메아리는 PE4 (외부 인터럽트 4번 핀)
#define ECHO_F     5   // 정면 메아리는 PE5 (외부 인터럽트 5번 핀)
#define ECHO_R     6   // 오른쪽 메아리는 PE6 (외부 인터럽트 6번 핀)

// 밑에서 만들 초음파 함수들을 미리 등록(선언)해두는 서류
void init_ultrasonic(void);
void make_trigger(void);
void ultrasonic_processing(void);

#endif /* ULTRASONIC_H_ */