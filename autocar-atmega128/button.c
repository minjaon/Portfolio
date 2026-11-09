/*
 * button.c
 *
 * Created: 2026-06-10 오후 1:25:20
 *  Author: user
 */ 

#include "button.h"

void init_button(void);
int get_button(int button_num, int button_pin);

// 버튼 초기화 방향설정(입력) 
void init_button(void)
{
	BUTTON_DDR &= ~(1 << BUTTON0PIN | 1 << BUTTON1PIN | 1 << BUTTON2PIN | 1 << BUTTON3PIN);
	DDRG &= ~(1 << 1); // 입력 방향
	PORTG |= (1 << 1); // 내부 풀업 저항 활성화
	// BUTTON_DDR &= 0xf0;
	// BUTTON_DDR &= 0x0f
}

// 예) BUTTON0 3
//    button을 눌렀다 떼면 : 1을 리턴
//          이 idle 상태 : 0을 리턴 

// ===================================================
// 버튼 상태 읽기 함수: 어떤 버튼이(button_num), 몇 번 핀(button_pin)에 꽂혔는지 알아내서
// 손가락으로 '딸깍!' 하고 완벽하게 눌렀다 뗐을 때 딱 한 번 1을 돌려주는 고마운 함수야.
// ===================================================
int get_button(int button_num, int button_pin)
{
	// static: 함수가 끝나도 이 배열 안의 기록들은 메모리에 그대로 살아있어!
	// 각 버튼이 방금 전까지 '눌려있던 상태(PRESS)'였는지 '떨어진 상태(RELEASE)'였는지 과거 기억을 유지하는 저장소야.
	static unsigned char button_status[BUTTON_NUMBER] =
	{
		BUTTON_RELEASE, BUTTON_RELEASE, BUTTON_RELEASE, BUTTON_RELEASE, BUTTON_RELEASE
	};
	
	int current_state; // 지금 이 순간 버튼이 전기를 뿜고 있는지 담을 주머니
	
	// ── [1단계] 버튼 읽기 (Active-Low와 Active-High 구별하기 ⭐️) ──
	// 4번 버튼(BUTTON4)은 포트G의 1번 핀(PG1)을 쓰는데, 이 녀석은 안 누르면 전기(1)가 흐르고 누르면 전기(0)가 끊기는 Active-Low 회로야.
	if (button_num == BUTTON4)
	{
		// PING의 1번 비트가 0(눌림)이면 !0이 되면서 current_state에 1(참)이 저장돼!
		// 반대로 안 눌러서 1이 들어오면 !1이 되면서 0(거짓)이 되지. 참/거짓 논리를 사람이 이해하기 편하게 뒤집은 거야.
		current_state = !(PING & (1 << button_pin));
	}
	else
	{
		// 0~3번 버튼은 누르면 전기(1)가 들어오는 평범한 회로(Active-High)야.
		// 그러니까 들어오는 비트 그대로(&) 가공해서 1이면 누름, 0이면 안 누름으로 정직하게 읽어와.
		current_state = BUTTON_PIN & (1 << button_pin);
	}

	// ── [2단계] 버튼 상태 체크 및 디바운싱(Debouncing) ──
	// 디바운싱이 왜 필요하냐면, 사람이 버튼을 누를 때 금속 접점이 아주 미세하게 수십 번 찌르르르 떨리면서(잡음) 컴퓨터는 수십 번 눌렸다고 오해하거든! 그걸 막아주는 필터링이야.

	// 조건: 지금 전기가 감지되었고(&&), 과거 기억에는 이 버튼이 떨어진 상태(RELEASE)였다면? -> "오라? 이제 막 누르기 시작했네!"
	if (current_state && button_status[button_num] == BUTTON_RELEASE)
	{
		_delay_ms(15); // 15ms 동안 잠시 대기하면서 접점의 미세한 떨림(잡음)이 끝날 때까지 기다려줘.
		button_status[button_num] = BUTTON_PRESS; // 이제 안정되었으니 과거 기억을 '누름(PRESS)' 상태로 업데이트!
		return 0; // 아직 손가락을 떼지 않았으니 0을 반환하며 잠자코 기다려.
	}
	// 조건: 과거 기억에는 '눌려있는 중(PRESS)'이었는데(&&), 지금은 손가락을 떼서 전기가 끊겼다면(!current_state)? -> "앗! 지금 뗐다!"
	else if (button_status[button_num] == BUTTON_PRESS && !current_state)
	{
		_delay_ms(15); // 뗄 때 생기는 미세한 접점 잡음도 15ms 동안 기다려서 완전히 걸러내.
		button_status[button_num] = BUTTON_RELEASE; // 기억을 다시 '떨어짐(RELEASE)'으로 원상복구!
		return 1; // 드디어 '딸깍!' 하고 완벽하게 1번 눌렀다 뗀 게 확인되었으니, "수고했어! 1번 눌렸어!" 하고 확실한 신호(1)를 날려줘!
	}
	
	// 누르고 있는 중이거나, 아예 안 누르고 있는 평화로운 상태일 때는 아무 일도 없었다는 듯이 0을 돌려줘.
	return 0;
}