/*
 * uart0.c
 * * 설명: PC(시리얼 모니터)나 조종기로부터 명령어를 받아서 
 * 차량을 앞, 뒤, 좌, 우로 움직이게 하는 핵심 통신 제어실이야!
 */
#include "uart0.h"
#include <stdio.h>

// 이 파일 안에서 쓸 미니 함수들의 이름표(프로토타입) 미리 선언하기
void init_uart0();
void UART0_transmit(uint8_t data);
void pc_command_processing(); // ⭐️ main.c와 스펠링 맞춤! (m 하나짜리로 통일해서 에러를 잡았지!)

// ── [원형 큐 주머니 세팅] ──
// 데이터가 들어오는 위치(rear)와 데이터가 나가는 위치(front)를 0으로 초기화해.
volatile int rear = 0;
volatile int front = 0;
// 문자열을 최대 10개(QUEUE_SIZE)까지 쌓아둘 수 있는 2차원 버퍼 방을 개설한 거야.
volatile unsigned char rx_buff[QUEUE_SIZE][QUEUE_LENGTH];

// 다른 파일(.c)에 들어있는 주행 제어 변수와 모터 구동 함수들을 빌려올게!
extern int func_state;
extern void forward(int speed);
extern void backward(int speed);
extern void turn_left(int speed);
extern void turn_right(int speed);
extern void stop();

// =========================================================================
// [1] 시리얼 수신 인터럽트 서비스 루틴 (글자가 들어오면 자동 소환되는 자동 응답기!)
// =========================================================================
ISR(USART0_RX_vect)
{
    volatile uint8_t data;
    volatile static int i = 0; // 한 문장 안에서 글자 수가 몇 번째인지 세어주는 방 (static이라 유지됨)

    data = UDR0; // 하드웨어 수신 레지스터(UDR0)에 도착한 글자를 얼른 내 data 주머니에 복사해!

    // ── 만약 엔터 키('\n' 또는 '\r')를 쳐서 한 문장이 끝났다면? ──
    if (data == '\n' || data == '\r')
    {
        // 큐(Queue)가 꽉 찼는지(Full) 먼저 검사해. 꽉 찼으면 더 이상 저장 안 하고 튕겨냄!
        if ((rear + 1) % QUEUE_SIZE == front % QUEUE_SIZE)
        {
            return;
        }
        
        rx_buff[rear][i] = '\0'; // 문장의 맨 끝이라는 걸 컴퓨터에게 알리기 위해 NULL문자('\0')를 콕 박아줘.
        i = 0;                   // 한 문장이 끝났으니 다음 문장을 위해 글자 수 카운터는 0으로 리셋!
        rear = (rear + 1) % QUEUE_SIZE; // 중요! 원형 모양으로 돌기 때문에 9번 칸 다음은 다시 0번 칸으로 가게 해.
    }
    // ── 아직 엔터를 안 치고 글자가 계속 들어오는 중이라면? ──
    else
    {
        // 여기도 마찬가지로 주머니가 꽉 찼는지 체크!
        if ((rear + 1) % QUEUE_SIZE == front % QUEUE_SIZE)
        {
            return;
        }
        rx_buff[rear][i++] = data; // 방금 받은 글자를 한 글자씩 차곡차곡 채우고 i(다음 칸)를 1 늘려줘.
    }
}

// =========================================================================
// [2] UART0 초기화 함수: 통신 속도 및 규칙 정하기
// =========================================================================
void init_uart0(void){
    UBRR0H = 0x00;
    UBRR0L = 207;  // 통신 속도(Baud Rate)를 9600bps로 깔아주는 마법의 하드웨어 세팅 값!
    UCSR0A |= 1 << U2X0; // 조금 더 안정적으로 데이터를 받기 위해 통신 속도를 2배속 모드로 설정!
    
    // RXEN0(수신 허가), TXEN0(송신 허가), 그리고 글자가 들어오면 인터럽트를 터뜨려라(RXCIE0) 스위치 켜기!
    UCSR0B |= 1 << RXEN0 | 1 << TXEN0 | 1 << RXCIE0;
}

// =========================================================================
// [3] UART0 송신 함수: 내가 다른 컴퓨터(PC)에 글자 1바이트를 뿜어낼 때 씀
// =========================================================================
void UART0_transmit(uint8_t data)
{
    // 하드웨어 송신 버퍼(UDRE0)가 텅 빌 때까지 잠시 무한루프 돌며 기다려줘. (앞에 보낸 글자가 나갈 시간을 줌)
    while(!(UCSR0A & (1 << UDRE0)));
    UDR0 = data; // 버퍼가 비었으면 전송 레지스터에 데이터를 탁 던져서 밖으로 전송!
}

// =========================================================================
// [4] 명령어 해석 및 처리기: 큐에 쌓인 명령어를 꺼내서 차를 조종하는 브레인! 🧠
// =========================================================================
void pc_command_processing()
{
    // front(꺼낼 위치)와 rear(저장된 위치)가 다르다는 건? -> "어라? 큐에 새로 들어온 명령어 문자열이 있네!"
    if (front != rear)
    {
        // PC 시리얼 모니터 창에 "내가 지금 이 명령어를 받아서 해석 중이야~" 하고 생색내며 보여주기!
        printf("cmd: %s\r\n", rx_buff[front]);  

        // ── ⭐️ [문자열 비교 알고리즘] 들어온 한 글자가 뭔지 비교해서 바퀴를 굴리자! ──
        // (const char *)로 캐스팅을 완벽하게 해줘서 strncmp 함수가 에러 없이 잘 작동해!
        
        if (strncmp((const char *)rx_buff[front], "f", 1) == 0) {
            forward(500);   // 'f'가 들어오면 500 속도로 씩씩하게 전진!
        }
        else if (strncmp((const char *)rx_buff[front], "b", 1) == 0) {
            backward(500);  // 'b'가 들어오면 500 속도로 조심조심 후진!
        }
        else if (strncmp((const char *)rx_buff[front], "l", 1) == 0) {
            turn_left(700); // 'l'이 들어오면 바퀴 힘을 좀 더 줘서(700) 제자리 좌회전!
        }
        else if (strncmp((const char *)rx_buff[front], "r", 1) == 0) {
            turn_right(700);// 'r'이 들어오면 파워풀하게(700) 제자리 우회전!
        }
        else if (strncmp((const char *)rx_buff[front], "s", 1) == 0) {
            stop();         // 's'가 들어오면 위험하니까 즉시 브레이크(정지)!
        }
        else {
            printf("unknown cmd\r\n"); // 지정된 단어 외에 엉뚱한 타자가 들어오면 모른다고 출력해.
        }

        // 현재 칸에 든 명령어를 완벽하게 처리했으니, 큐 주머니의 다음 칸(front)을 가리키도록 이동!
        front = (front + 1) % QUEUE_SIZE;
    }
}