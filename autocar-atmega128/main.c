/*
 * 06.DCMOTOR_PWM_CONTROL
 *
 * Created: 2026-06-15 오전 11:36:45
 * Author : user
 */ 

#define F_CPU 16000000UL     // CPU 클럭 속도를 16MHz로 설정
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>   // 인터럽트(sei, ISR 등) 기능을 쓰기 위한 헤더
#include <stdio.h>           // printf 기능을 쓰기 위한 헤더
#include "auto_car.h"
#include "fnd.h"
#include "def.h"

// ── 장애물 회피 기준 거리 설정 ──
#define OBSTACLE_DIST   10   // 정면/측면 장애물 판단 거리를 10cm로 타이트하게 축소! (10cm 이내로 오면 벽으로 인식)
#define CORNER_DIST     12   // 양옆 코너에 완전히 꼈다고 판단하는 기준 거리를 12cm로 설정!

// ── 외부(extern) 파일에 만들어 둔 함수 목록 불러오기 ──
// 다른 파일(.c)에 구현된 초기화 및 주행 함수들을 가져와서 쓰겠다고 컴파일러에게 통보하는 거야.
extern void init_auto_car(void);
extern void init_uart0(void);
extern void UART0_transmit(uint8_t data);
extern void pc_command_processing(void);
extern void init_ultrasonic(void);
extern void ultrasonic_processing(void);
extern int get_auto_button(void);
extern void set_auto_led(int state);
extern void init_timer1_pwm(void);
extern void init_motor_driver(void);
extern void init_button(void);
extern void init_uart1(void);
extern void init_fnd(void);
extern void init_timer2(void);

extern void forward(int speed);
extern void backward(int speed);
extern void turn_left(int speed);
extern void turn_right(int speed);
extern void stop(void);

// 다른 파일에서 인터럽트로 계속 바뀌는 전역 변수들 가져오기
extern volatile uint8_t bt_data;    // 블루투스로 수신된 문자 데이터 ('F', 'B' 등)
extern volatile uint8_t stopwatch;  // 스톱워치/주행 상태 (STOP=0, RUN=1)

// 스마트카의 상태(모드)를 나타내는 숫자 명찰들
#define MANUAL_MODE     0  // 수동 주행 모드
#define AUTO_MODE       1  // 자율 주행 모드
#define AUTO_MODE_CHECK 2  // 자율 주행 체크 상태
#define DISTANCE_CHECK  3  // 거리 체크 상태

// 모드별로 실행할 함수들의 이름 표기(원형 선언)
void manual_mode(void);
void auto_mode(void);
void auto_mode_check(void);
void distance_check(void);

// printf가 모니터 화면이 아니라 UART0(시리얼 모니터)로 글자를 뿜어내도록 연결 통로를 개설하는 설정!
FILE OUTPUT = FDEV_SETUP_STREAM(UART0_transmit, NULL, _FDEV_SETUP_WRITE);

// ── 전역 변수 영역 ──
volatile uint32_t msec_count = 0;       // 전체 흘러간 시간(ms) 카운터
volatile int ultrasonic_check_time = 0; // 초음파 주기 계산용 ms 카운터 (60ms마다 리셋)
volatile int dist_left = 100;           // 왼쪽 초음파 거리 (기본값 100cm)
volatile int dist_front = 100;          // 정면 초음파 거리
volatile int dist_right = 100;          // 오른쪽 초음파 거리

int func_state = MANUAL_MODE;           // 현재 스마트카의 동작 상태 변수 (처음엔 수동 모드)
static int obstacle_limit = 20;         // 거리가 튈 때 완충 역할을 해줄 히스테리시스 기준 변수

// ── 함수 포인터 배열 (이 코드의 테크닉! ⭐️) ──
// pfunc[0]()을 실행하면 manual_mode가 실행되고, pfunc[1]()을 실행하면 auto_mode가 실행되도록
// 함수들을 마치 배열의 데이터처럼 예쁘게 묶어놓은 리스트야. 코드 줄 수를 엄청나게 줄여줘!
void (*pfunc[]) () = 
{
    manual_mode,
    auto_mode,
    auto_mode_check,
    distance_check
};

// ===================================================
// 1) 수동 주행 모드: 블루투스로 들어온 문자(bt_data)에 따라 조종자가 시키는 대로 움직여!
// ===================================================
void manual_mode(void)
{
	switch(bt_data) {
		case 'F': case 'f':
		forward(500);   // 앞선 전진 (속도 500)
		break;
		case 'B': case 'b':
		backward(500);  // 후진
		break;
		case 'L': case 'l':
		turn_left(700); // 제자리 좌회전 (바퀴가 꼬이니까 속도를 조금 더 높였네!)
		break;
		case 'R': case 'r':
		turn_right(700);// 제자리 우회전
		break;
		case 'S': case 's':
		stop();         // 정지
		break;
		default:
		break;
	}
}

// ===================================================
// 2) 자율 주행 모드: 3방향 초음파 센서 값을 보고 인공지능(?)처럼 벽을 피해 달리는 핵심 알고리즘!
// ===================================================
void auto_mode(void)
{
	static uint32_t stuck_start = 0;  // 구석에 끼여서 막히기 시작한 시점의 시간 기록용 방
	static uint8_t  stuck = 0;        // 지금 끼여있는 상태인가? (0=정상, 1=끼임)

	// 컴퓨터 시리얼 모니터로 왼쪽/정면/오른쪽 거리를 실시간 출력해서 확인하기
	printf("L:%d M:%d R:%d\n", dist_left, dist_front, dist_right);

	// ── 끼임(Stuck) 감지 단계 ──
	// 세 방향 중 한 곳이라도 10cm(OBSTACLE_DIST) 이하로 좁혀지면 obstacle(장애물 발생)은 참(1)이 돼.
	uint8_t obstacle = (dist_front <= OBSTACLE_DIST) || (dist_left <= OBSTACLE_DIST) || (dist_right <= OBSTACLE_DIST);

	if (obstacle)
	{
		if (!stuck) // 방금 막 막히기 시작한 순간이라면?
		{
			stuck       = 1;
			stuck_start = msec_count;  // 그 순간의 시간을 stuck_start 변수에 박제해 둬!
		}
	}
	else
	{
		stuck = 0;  // 장애물에서 벗어났다면 탈출 타이머를 리셋!
	}

	// ── 갇힘 감지 처리: 2초 이상 차가 못 나가고 낑낑대고 있다면? ──
	if (stuck && (msec_count - stuck_start >= 2000))
	{
		backward(600); // 탈출을 위해 강력하게 600 속도로 빽(후진)!

		if (msec_count - stuck_start >= 3000)
		{
			stuck = 0;  // 후진을 1초 동안(3000ms - 2000ms) 수행했으니, 다시 전방을 판단할 수 있게 탈출 모드 리셋!
		}
		return;  // 탈출 후진 중일 때는 아래에 있는 일반 회피 조건문을 무시하고 함수를 끝내버려!
	}

	// ── 일반 장애물 회피 조건문 (스마트카의 반사 신경) ──
	if (dist_left <= CORNER_DIST && dist_right <= CORNER_DIST)
	{
		backward(600); // 1. 양옆이 다 코너 벽으로 막혔다면 갈 곳이 없으니 일단 후진!
	}
	else if (dist_front <= OBSTACLE_DIST && dist_left <= OBSTACLE_DIST)
	{
		turn_right(600); // 2. 앞이랑 왼쪽이 막혔으면 오른쪽으로 꺾어야지!
	}
	else if (dist_front <= OBSTACLE_DIST && dist_right <= OBSTACLE_DIST)
	{
		turn_left(600);  // 3. 앞이랑 오른쪽이 막혔으면 왼쪽으로 꺾자!
	}
	else if (dist_front <= OBSTACLE_DIST)
	{
		// 4. 정면만 막힌 경우: 왼쪽 공간과 오른쪽 공간의 거리를 비교해서 더 넓은(거리가 큰) 쪽으로 핸들을 꺾어!
		if (dist_left > dist_right)
		turn_left(600);
		else
		turn_right(600);
	}
	else if (dist_left <= OBSTACLE_DIST)
	{
		turn_right(600); // 5. 주행 중 왼쪽 벽이 너무 가까워지면 오른쪽으로 튕겨나가기
	}
	else if (dist_right <= OBSTACLE_DIST)
	{
		turn_left(600);  // 6. 오른쪽 벽이 너무 가까워지면 왼쪽으로 튕겨나가기
	}
	else
	{
		forward(500);    // 7. 사방이 뻥 뚫렸다면 기분 좋게 400 속도로 순항(직진)하기!
	}
}

// 다음 모드로 자연스럽게 연결해주는 징검다리 함수들
void auto_mode_check(void)
{
	func_state = AUTO_MODE;
}

void distance_check(void)
{
	ultrasonic_processing();
	func_state = AUTO_MODE;
}
// 1ms 타이머 인터럽트
// ===================================================
// TIMER0 오버플로우 인터럽트 서비스 루틴 (정확히 1ms마다 실행되는 내부 심장박동)
// ===================================================
ISR(TIMER0_OVF_vect)
{
	TCNT0 = 6;    // 256방 중 6부터 세기 시작하니까 250카운트(250 x 4us = 1ms) 주기 완성!
	msec_count++;           // 스마트카 가동 후 누적 ms 계속 증가
	ultrasonic_check_time++; // 초음파 측정용 카운터도 같이 1ms마다 증가
}

// ===================================================
// 메인 함수: 프로그램이 시작되면 가장 먼저 실행되는 본부
// ===================================================
int main(void)
{
	// 모든 하드웨어 모듈 및 센서, 포트 초기화 군단 출격!
	init_led();
	init_timer0();
	init_uart0();
	init_uart1();
	init_button();
	init_motor_driver();
	init_timer1_pwm();
	init_ultrasonic();
	init_auto_car();
	init_fnd();
	init_timer2();
	
	stdout = &OUTPUT;   // printf 함수가 우리 스마트카의 UART0 포트로 전송되도록 표준 출력을 연결!
	sei();              // 전체 인터럽트 스위치 ON! (이제부터 1ms 타이머랑 FND가 동작하기 시작해)
	
	while(1)
	{
		// [1단계] 자율주행 전환 버튼(PG1 구멍)이 눌렸는지 검사!
		if (get_auto_button()) {
			if (stopwatch == STOP) { // 수동 모드였다면?
				stopwatch = RUN;     // 상태를 RUN(자율주행 가동)으로 바꾸고,
				set_auto_led(1);            // 자율주행 중임을 알리는 LED를 켜고,
				func_state = AUTO_MODE;     // 동작 함수를 자율주행 함수로 교체해!
				} else {                 // 이미 자율주행 중이었다면?
				stopwatch = STOP;    // 상태를 수동으로 전환하고,
				set_auto_led(0);            // 자율주행 LED는 끄고,
				stop();                     // 혹시 벽으로 돌진 중이었을지 모르니 모터부터 즉시 정지!
				func_state = MANUAL_MODE;   // 수동 조종 모드로 복귀시켜.
			}
		}

		// [2단계] 초음파 센서는 너무 자주 읽으면 에코 신호가 교란돼!
		// 그래서 딱 60ms 주기가 쌓일 때마다 트리거를 날려 순차적으로 3방향 거리를 측정해오는 거야.
		if (ultrasonic_check_time >= 60) {
			ultrasonic_processing();
			ultrasonic_check_time = 0; // 측정했으니 다시 0ms부터 세기 위해 초기화!
		}

		// [3단계] 현재 상태에 맞춰 함수 포인터 배열을 실행하는 최종 분기점!
		if (stopwatch == RUN)
		{
			pfunc[func_state](); // stopwatch가 RUN(1)이면 대개 func_state가 1(AUTO_MODE)이므로 auto_mode()가 무한 반복 실행돼!
		}
		else
		{
			pc_command_processing(); // stopwatch가 STOP(0)이면 블루투스 명령어를 해석하는 루틴을 돌린 후,
			pfunc[func_state]();    // func_state가 0(MANUAL_MODE)이므로 manual_mode() 함수를 무한 실행하면서 조종기 신호를 기다려!
		}
	}
}

// ===================================================
// TIMER0 설정 함수: 1ms 단위를 만들어내기 위한 하드웨어 세팅
// ===================================================
void init_timer0(void)
{
	TCNT0 = 6;
	TCCR0 &= ~((1 << CS02) | (1 << CS01) | (1 << CS00));
	TCCR0 |= (1 << CS02);   // 프리스케일러(분주비)를 64로 설정! (16MHz / 64 = 250kHz -> 1카운트당 4us 소요)
	TIMSK |= (1 << TOIE0);  // 타이머0이 넘쳐 흐를 때 인터럽트를 터뜨리겠다는 허가령 활성화!
}