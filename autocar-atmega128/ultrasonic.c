/*
 * ultrasonic.c
 * 3채널 초음파 순차 측정 방식 (간섭 제거 버전)
 */ 

#include "ultrasonic.h"
#include <stdio.h>

extern volatile int ultrasonic_check_time;

extern volatile int dist_left;
extern volatile int dist_front;
extern volatile int dist_right;

// 현재 어떤 센서를 측정할지 나타내는 상태 변수 (0:좌, 1:정면, 2:우)
volatile uint8_t sensor_selector = 0; 

// 좌측 초음파 INT4 (PE4)
ISR(INT4_vect)
{
	if (PINE & (1 << PE4)) {
		TCNT3 = 0;
	} else {
		dist_left = (TCNT3 * 1000000.0 * 1024 / F_CPU) / 58;
	}
}

// 정면 초음파 INT5 (PE5)
ISR(INT5_vect)
{
	if (PINE & (1 << PE5)) {
		TCNT3 = 0;
	} else {
		dist_front = (TCNT3 * 1000000.0 * 1024 / F_CPU) / 58;
	}
}

// 우측 초음파 INT6 (PE6)
ISR(INT6_vect)
{
	if (PINE & (1 << PE6)) {
		TCNT3 = 0;
	} else {
		dist_right = (TCNT3 * 1000000.0 * 1024 / F_CPU) / 58;
	}
}

// [함수 1] 초음파 센서 쓰기 전에 초기 세팅해주는 함수!
void init_ultrasonic(void){
	// 1. 발사 핀(PA0,1,2)을 '출력 모드(1)'로 바꿉니다.
	DDRA |= (1 << DDA0) | (1 << DDA1) | (1 << DDA2);
	
	// 2. 메아리 핀(PE4,5,6)을 '입력 모드(0)'로 바꿉니다.
	DDRE &= ~((1 << DDE4) | (1 << DDE5) | (1 << DDE6));

	// 3. 메아리가 출발할 때(상승)랑 돌아올 때(하강) 둘 다 인터럽트가 터지도록 레지스터(EICRB) 설정을 켜는 거야.
	EICRB |= (1 << ISC60) | (1 << ISC50) | (1 << ISC40);
	EICRB &= ~((1 << ISC61) | (1 << ISC51) | (1 << ISC41));

	// 4. 시간 측정용 시계(타이머3)의 속도(분주비 1024)를 설정해서 째깍째깍 흐르게 만들어.
	TCCR3B |= (1 << CS32) | (1 << CS30);

	// 5. 드디어 외부 인터럽트 스위치 4, 5, 6번을 진짜로 활성화(On) 해줍니다!
	EIMSK |= (1 << INT6) | (1 << INT5) | (1 << INT4);
}

// [함수 2] "너 초음파 소리 한 번 발사해!" 하고 신호를 주는 트리거 함수야
void make_trigger_selective(uint8_t pin){
	PORTA &= ~(1 << pin); // 일단 안전하게 전기를 껐다가
	_delay_us(2);         // 2마이크로초 대기
	PORTA |= (1 << pin);  // 전기를 팍 켜서 펄스 신호를 만들어!
	_delay_us(15);        // 초음파 센서 규격에 맞게 15마이크로초 동안 유지했다가
	PORTA &= ~(1 << pin); // 다시 전기를 꺼주면 센서가 "앗 쐈다!" 하고 소리를 뿜어내게 돼.
}

// [함수 3] 메인 루프에서 계속 실행되면서 3개 센서를 조율하는 핵심 컨트롤러야!
void ultrasonic_processing(void){
	// 소리끼리 뒤섞여서 벽인 줄 착각하는 문제를 막으려고 "60ms 시차"를 두고 순서대로 실행해!
	if(ultrasonic_check_time >= 60)
	{
		ultrasonic_check_time = 0; // 60ms 지났으니 다시 시간 카운터는 0으로 리셋!
		
		// 선택 변수가 0이면? 이번엔 왼쪽 센서 차례!
		if(sensor_selector == 0)
		{
			make_trigger_selective(0); // 왼쪽(PA0) 초음파 발사!
			sensor_selector = 1;       // 다음번 60ms 뒤에는 정면 센서를 켜라고 예약해둠
		}
		// 선택 변수가 1이면? 이번엔 정면 센서 차례!
		else if(sensor_selector == 1)
		{
			make_trigger_selective(1); // 정면(PA1) 초음파 발사!
			sensor_selector = 2;       // 다음번에는 오른쪽 센서 켜라고 예약!
		}
		// 선택 변수가 2이면? 이번엔 오른쪽 센서 차례!
		else if(sensor_selector == 2)
		{
			make_trigger_selective(2); // 오른쪽(PA2) 초음파 발사!
			
			// 왼쪽, 정면, 오른쪽 한 바퀴 다 돌았으니까 컴퓨터 모니터 시리얼 창에 현재 거리들을 예쁘게 출력해줘!
			printf("L:%3dcm | F:%3dcm | R:%3dcm\r\n", dist_left, dist_front, dist_right);
			
			sensor_selector = 0;       // 다 돌았으니 다시 왼쪽 센서(0번) 순서로 리셋해줍니다 ㅎㅎ
		}
	}
}