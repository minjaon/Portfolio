#include "pwm.h"
#include <util/delay.h>

#define MOTOR_PWM_DDR             DDRB
#define MOTOR_DIRECTION_PORT      PORTF
#define MOTOR_DIRECTION_PORT_DDR  DDRF

// FND에 상태를 보내기 위한 외부 전역 변수 공유 선언
extern volatile char car_current_status;

void init_timer1_pwm(void)
{
	TCCR1B |= (1 << CS11) | (1 << CS10); // 분주비 64
	TCCR1A |= (1 << WGM11);
	TCCR1B |= (1 << WGM13) | (1 << WGM12); // 모드 14 (Fast PWM)
	
	TCCR1A |= (1 << COM1A1);  // PB5 (왼쪽 바퀴)
	TCCR1A |= (1 << COM1B1);  // PB6 (오른쪽 바퀴)
	
	ICR1 = 0x3ff;  // 1023 TOP 값
}

void init_motor_driver(void)
{
	MOTOR_PWM_DDR |= (1 << 5) | (1 << 6);
	MOTOR_DIRECTION_PORT_DDR |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	stop();
}



void forward(int speed)
{
	car_current_status = 'F'; // FND 상태 변경
	MOTOR_DIRECTION_PORT &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
	MOTOR_DIRECTION_PORT |= (1 << 3) | (1 << 1);
	OCR1A = speed;
	OCR1B = speed;
}

void backward(int speed)
{
		car_current_status = 'B'; // FND 상태 변경
		MOTOR_DIRECTION_PORT &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
		MOTOR_DIRECTION_PORT |= (1 << 2) | (1 << 0);
		OCR1A = speed;
		OCR1B = speed;
}

void turn_left(int speed)
{
	car_current_status = 'L'; // FND 상태 변경
	MOTOR_DIRECTION_PORT &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
	MOTOR_DIRECTION_PORT |= (1 << 2) | (1 << 1);
	OCR1A = speed;
	OCR1B = speed;

}

void turn_right(int speed)
{
	car_current_status = 'R'; // FND 상태 변경
	MOTOR_DIRECTION_PORT &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
	MOTOR_DIRECTION_PORT |= (1 << 3) | (1 << 0);
	OCR1A = speed;
	OCR1B = speed;
}

void stop(void)
{
	car_current_status = 'S'; // FND 상태 변경
	MOTOR_DIRECTION_PORT |= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
	OCR1A = 0;
	OCR1B = 0;
}