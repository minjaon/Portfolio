/*
 * fnd.c
 * 타이머 FND (PORTC, PORTF) + 차량 상태 표시 FND (PORTD + PORTB 분할 제어 버전)
 * * 작성자 주석: 처음 보는 사람도 이해할 수 있게 최대한 쉽게 써봤어요!
 */

#include "fnd.h"
#include <avr/interrupt.h>
#include "auto_car.h"

// 함수 미리 선언 (컴퓨터에게 "이 파일 아래쪽에 이런 함수들 있으니 참고해~" 알려주는 목록)
void init_fnd(void);
void init_timer2(void);
void fnd_display(void);

// 스톱워치 상태를 나타내는 숫자들 (0=멈춤, 1=달리는중)
#define STOP 0
#define RUN  1

// 차량 현재 상태 저장 변수 (F=앞, B=뒤, L=왼, R=오른, S=정지)
// volatile: 이 변수는 인터럽트 안방(ISR)에서도 막 바뀌니까, 컴파일러한테 "마음대로 최적화(생략)하지 마"라고 경고하는 거야!
volatile char car_current_status = 'S';

// 스톱워치 시간 저장 변수들
// uint32_t: 0부터 42억까지 담을 수 있는 엄청 큰 상자야 (밀리초는 숫자가 엄청 빨리 커지니까 크게 만들어!)
volatile uint32_t sw_ms_count  = 0;  // 밀리초 카운터 (0~999가 되면 0으로 리셋)
volatile uint32_t sw_sec_count = 0;  // 초 카운터 (0~59)
volatile uint32_t sw_min_count = 0;  // 분 카운터 (0~59)

// 스톱워치가 지금 동작 중인지 아닌지 판별하는 상태 방 (처음엔 STOP인 0으로 시작!)
volatile uint8_t stopwatch = STOP;


// ===================================================
// TIMER2 오버플로우 인터럽트 (컴퓨터가 다른 일 하다가도 1ms마다 무조건 일로 순간이동해!)
// ISR = Interrupt Service Routine (인터럽트가 걸리면 실행되는 전용 함수)
// ===================================================
ISR(TIMER2_OVF_vect) {

    // 타이머 시계 내부 카운터를 다시 194로 채워넣어. (256방짜리 방인데 194부터 세기 시작하면 딱 1ms 뒤에 다시 터지거든!)
    TCNT2 = 194;

    // FND 화면 갱신 함수를 불러와! (1ms마다 한 자릿수씩 엄청 빠르게 번갈아 켜서 잔상을 만드는 비결이야)
    fnd_display();

    // 만약 스톱워치가 RUN(1) 상태라면? 이제 시계를 재야지!
    if (stopwatch == RUN) {

        sw_ms_count++;  // 1ms마다 밀리초 숫자를 1씩 더해줘

        // 1000ms가 차오르면? 드디어 1초가 된 거야!
        if (sw_ms_count >= 1000) {
            sw_ms_count = 0;  // 밀리초는 다시 0으로 청소하고
            sw_sec_count++;   // 초를 1 늘려줘

            // 60초가 다 차면? 1분이 된 거지!
            if (sw_sec_count >= 60) {
                sw_sec_count = 0;  // 초는 다시 0으로 청소하고
                sw_min_count++;    // 분을 1 늘려줘

                // 60분이 되면? 계기판이 터지면 안 되니까 다시 0분으로! (최대 59분 59초까지만 표기 가능)
                if (sw_min_count >= 60) {
                    sw_min_count = 0;
                }
            }
        }
    }
}


// ===================================================
// FND 초기화 함수: 부품들이 꽂힌 구멍(핀)들을 출력 모드로 세팅해주는 곳
// ===================================================
void init_fnd(void) {

    // ── 타이머 FND 초기화 (PORTC, PORTF 쪽) ──

    FND_DATA_DDR = 0xff; // PORTC의 8개 핀을 전부 '출력 모드(0xFF)'로 설정해서 폰트 데이터를 보낼 준비를 해

    // Digit 선택 핀(D1~D4)인 PORTF의 4,5,6,7번 핀을 출력 모드로 켜줘!
    FND_DIGIT_DDR |= (1 << FND_DIGIT_D1) | (1 << FND_DIGIT_D2)
                   | (1 << FND_DIGIT_D3) | (1 << FND_DIGIT_D4);

    // 캐소드 FND는 핀에 High(1) 전기를 주면 꺼져! 그러니까 처음엔 다 꺼지도록 High(1)로 초기화해둔 거야.
    FND_DIGIT_PORT |= (1 << FND_DIGIT_D1) | (1 << FND_DIGIT_D2)
                    | (1 << FND_DIGIT_D3) | (1 << FND_DIGIT_D4);


    // ── 상태 표시 FND 초기화 (PORTD 상위 4비트 + PORTB 하위 4비트) ──

    // PORTD의 PD4, PD5, PD6, PD7 핀만 출력 모드로 세팅해 (PD0~PD3은 다른 용도라 건드리면 큰일 나!)
    // |= 를 쓰는 이유: 원래 들어있던 다른 핀 설정을 지우지 않고, 내가 원하는 핀만 '쏙' 1로 만들기 위해서야!
    DDRD |= (1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7);

    // PORTB의 PB0, PB1, PB2, PB3 핀만 출력 모드로 세팅해 (PB4~PB7은 바퀴 굴리는 모터 PWM 전용선이라 절대 절대 건드리면 안 돼!)
    DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB3);

    // 상태 FND의 4개 자릿수를 고르는 스위치선은 PORTA의 4, 5, 6, 7번 핀으로 쓰겠다고 선언!
    DDRA |= (1 << PA4) | (1 << PA5) | (1 << PA6) | (1 << PA7);

    // 상태 FND 자릿수 스위치도 처음에는 다 꺼두자 (High = OFF)
    PORTA |= (1 << PA4) | (1 << PA5) | (1 << PA6) | (1 << PA7);
}


// ===================================================
// FND 디스플레이 함수: 숫자가 눈에 보이게 전기를 컨트롤하는 핵심 함수
// ===================================================
void fnd_display(void) {

    // ── 숫자 폰트 테이블 (0~9) ──
    // 캐소드 FND는 전기를 끊어야(LOW=0) 불이 들어오는 청개구리 속성이 있어. 
    // 그래서 원래 16진수 폰트 앞에 ~(NOT 연산자)를 붙여서 비트를 완전히 뒤집어 버린 거야!
    // 예: 0 모양은 0xC0인데, ~0xC0을 하면 0x3F가 되어서 핀에 알맞게 불이 켜져.
    uint8_t fnd_font[10];
    fnd_font[0] = ~0xC0;  // 0 모양
    fnd_font[1] = ~0xF9;  // 1 모양
    fnd_font[2] = ~0xA4;  // 2 모양
    fnd_font[3] = ~0xB0;  // 3 모양
    fnd_font[4] = ~0x99;  // 4 모양
    fnd_font[5] = ~0x92;  // 5 모양
    fnd_font[6] = ~0x82;  // 6 모양
    fnd_font[7] = ~0xD8;  // 7 모양
    fnd_font[8] = ~0x80;  // 8 모양
    fnd_font[9] = ~0x90;  // 9 모양

    // ── 차량 상태 글자(알파벳) 폰트 결정 ──
    uint8_t status_font; // 글자 모양 비트를 담을 임시 주머니

    // 현재 자동차 상태 문자가 무엇이냐에 따라 특수 모양 비트를 주머니에 넣어줘
    if (car_current_status == 'F') {
        status_font = ~0x8E;  // 디스플레이에 'F' 모양이 나오도록 뒤집은 비트
    } else if (car_current_status == 'B') {
        status_font = ~0x83;  // 'b' 모양 (뒤로가기)
    } else if (car_current_status == 'L') {
        status_font = ~0xC7;  // 'L' 모양 (왼쪽)
    } else if (car_current_status == 'R') {
        status_font = ~0xAF;  // 'r' 모양 (오른쪽)
    } else {
        status_font = ~0x92;  // 정지했거나 모르면 'S' 모양(Stop)을 띄우자!
    }


    // ── 상태 폰트를 PORTD와 PORTB 핀에 맞게 반반 쪼개기!! (이 코드의 백미 ⭐️) ──
    // 하나의 글자 패턴(status_font)은 원래 8개 비트(0~7번)가 한 몸이야.
    // 근데 하드웨어 연결을 보니, 0~3번 비트는 PORTD에 연결됐고, 4~7번 비트는 PORTB에 찢어져서 연결됐대!
    // 그래서 컴퓨터한테 이 비트들을 일일이 검사해서 쪼개 담으라고 지시하는 노가다 영역이야.

    uint8_t pd_data = 0;  // PORTD의 상위 4칸에 채워 넣을 임시 변수
    uint8_t pb_data = 0;  // PORTB의 하위 4칸에 채워 넣을 임시 변수

    // status_font의 0번 비트(A세그먼트)가 1인지 검사해서(&), 참이면 pd_data의 7번 비트(PD7) 자리에 1을 켜줘!
    if (status_font & (1 << 0)) pd_data |= (1 << 7);

    // 1번 비트(B세그먼트)가 1이면 -> pd_data의 6번 비트(PD6) 자리에 1을 켜줘!
    if (status_font & (1 << 1)) pd_data |= (1 << 6);

    // 2번 비트(C세그먼트)가 1이면 -> pd_data의 5번 비트(PD5) 자리에 1을 켜줘!
    if (status_font & (1 << 2)) pd_data |= (1 << 5);

    // 3번 비트(D세그먼트)가 1이면 -> pd_data의 4번 비트(PD4) 자리에 1을 켜줘!
    if (status_font & (1 << 3)) pd_data |= (1 << 4);

    // 4번 비트(E세그먼트)가 1이면 -> 이번엔 pb_data의 3번 비트(PB3) 자리에 1을 켜줘!
    if (status_font & (1 << 4)) pb_data |= (1 << 3);

    // 5번 비트(F세그먼트)가 1이면 -> pb_data의 2번 비트(PB2) 자리에 1을 켜줘!
    if (status_font & (1 << 5)) pb_data |= (1 << 2);

    // 6번 비트(G세그먼트)가 1이면 -> pb_data의 1번 비트(PB1) 자리에 1을 켜줘!
    if (status_font & (1 << 6)) pb_data |= (1 << 1);

    // 7번 비트(DP세그먼트)가 1이면 -> pb_data의 0번 비트(PB0) 자리에 1을 켜줘!
    if (status_font & (1 << 7)) pb_data |= (1 << 0);


    // ── 어떤 자릿수를 선택해서 불을 켤지 결정하는 스위치 변수 ──
    // static: 이 함수가 종료되어도 이 방에 들어있는 숫자(0,1,2,3)는 메모리에 그대로 박혀서 유지돼!
    static int digit_select = 0;


    // ── 잔상 제거: 자릿수를 다음 칸으로 넘기기 전에 원래 켜져 있던 모든 불을 팍! 꺼버려 ──
    // 이걸 안 해주면 이전 칸에 남아있던 숫자의 유령(잔상)이 다음 칸에 겹쳐 보여서 글자가 흐릿하고 번져 보여!
    FND_DIGIT_PORT |= (1 << FND_DIGIT_D1) | (1 << FND_DIGIT_D2)
                    | (1 << FND_DIGIT_D3) | (1 << FND_DIGIT_D4); // 타이머 FND 올킬(OFF)
    PORTA |= (1 << PA4) | (1 << PA5) | (1 << PA6) | (1 << PA7);   // 상태 FND 올킬(OFF)


    // ── 쪼개놓은 상태 폰트 데이터를 진짜 포트 구멍으로 밀어 넣기! ──
    // (PORTD & 0x0F): PORTD의 하위 4비트는 원래 쓰던 기능(통신 등)이 있을지 모르니 그대로 보호하고(&), 상위 4비트 자리에만 pd_data를 결합(|)해서 덮어씌워!
    PORTD = (PORTD & 0x0F) | pd_data;

    // (PORTB & 0xF0): PORTB의 상위 4비트(모터 축 제어선)는 절대 건드리지 말고 보존한 채, 하위 4비트에만 pb_data를 결합(|)해!
    PORTB = (PORTB & 0xF0) | pb_data;


    // ── 다이내믹 제어: digit_select(0~3) 순서에 따라 타이머 숫자와 상태 칸을 동시 가동! ──
    if (digit_select == 0) {
        // 1. 분의 십의 자리 데이터(예: 50분이면 5)를 계산해서 세그먼트 데이터 포트에 쏴줘.
        FND_DATA_PORT = fnd_font[(sw_min_count / 10) % 6];
        FND_DIGIT_PORT &= ~(1 << FND_DIGIT_D1);  // 타이머 FND의 첫 번째 자릿수 스위치 ON (Low=ON)
        PORTA &= ~(1 << PA7);                    // 상태 FND의 첫 번째 자릿수 스위치도 같이 세트로 ON!

    } else if (digit_select == 1) {
        // 2. 분의 일의 자리 데이터를 넣는데, 중간에 시와 분을 구분하는 점(소수점 DP)도 같이 켜지라고 | 0x80 (최상위 비트 1)을 더해준 거야!
        FND_DATA_PORT = fnd_font[sw_min_count % 10] | 0x80;
        FND_DIGIT_PORT &= ~(1 << FND_DIGIT_D2);  // 두 번째 자릿수 스위치 ON
        PORTA &= ~(1 << PA6);                    // 상태 FND 두 번째 자릿수 ON

    } else if (digit_select == 2) {
        // 3. 초의 십의 자리 데이터를 구해서 넣어줘.
        FND_DATA_PORT = fnd_font[(sw_sec_count / 10) % 6];
        FND_DIGIT_PORT &= ~(1 << FND_DIGIT_D3);  // 세 번째 자릿수 스위치 ON
        PORTA &= ~(1 << PA5);                    // 상태 FND 세 번째 자릿수 ON

    } else {
        // 4. digit_select가 3일 때 실행되는 마지막 일의 자리 초 출력 단계야.
        FND_DATA_PORT = fnd_font[sw_sec_count % 10];
        FND_DIGIT_PORT &= ~(1 << FND_DIGIT_D4);  // 네 번째 자릿수 스위치 ON
        PORTA &= ~(1 << PA4);                    // 상태 FND 네 번째 자릿수 ON
    }

    // 다음번 1ms 후에 인터럽트가 실행될 때는 다음 칸(자릿수)을 켜야 하니까 숫자를 1 늘려줘.
    // % 4 를 해주면 0 -> 1 -> 2 -> 3 까지 갔다가 4가 되는 순간 다시 0으로 예쁘게 굴러떨어져! 고리형 순환 구조지.
    digit_select = (digit_select + 1) % 4;
}


// ===================================================
// TIMER2 초기화 함수: 메인 컴퓨터한테 "너 이제부터 정밀 시계 가동해!" 명령하는 곳
// ===================================================
void init_timer2(void) {

    // TCCR2 레지스터의 CS22 비트를 고정으로 1로 만들어.
    // 메인 CPU 속도(16MHz)를 64로 나눈 속도(프리스케일러)로 내부 타이머 시계를 째깍거리게 세팅한 거야.
    TCCR2 |= (1 << CS22);

    // 시계 시작 숫자를 194로 채워넣어줘. (256이 되면 펑 터지는 오버플로우 시계인데 194부터 세니까 딱 1ms 주기가 나와!)
    TCNT2 = 194;

    // TIMSK 레지스터의 TOIE2 비트를 탁 켜서 "타이머2가 오버플로우(펑 터질 때)될 때마다 아까 저 위에 ISR 인터럽트 방으로 순간이동해라!" 하고 최종 승인 스위치를 올려주는 거야.
    TIMSK |= (1 << TOIE2);
}