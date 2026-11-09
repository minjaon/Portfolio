# 07.AUTO_CAR

초음파 센서 3채널로 벽을 회피하는 자율주행 + 블루투스 수동 조종 RC카 (ATmega128A, Bare-metal C)

---

## 🛠 Tech Stack
<img src="https://img.shields.io/badge/c-A8B9CC?style=for-the-badge&logo=c&logoColor=black"> <img src="https://img.shields.io/badge/AVR%20ATmega128A-1E1E1E?style=for-the-badge"> <img src="https://img.shields.io/badge/Atmel%20Studio-1F6FEB?style=for-the-badge">

---

## 📌 소개
ATmega128A 기반 스마트카입니다. 평소에는 블루투스(HC-06 등)로 전후진·좌우회전을 수동 조종하고, 버튼 한 번으로 좌/정면/우 3채널 초음파 센서 기반 자율주행 모드로 전환됩니다. 초음파 거리 값으로 벽 회피, 코너 끼임 감지, 갇힘 탈출(자동 후진)까지 처리하며, FND에 현재 주행 상태와 스톱워치를 동시에 표시합니다.

## ✨ 주요 기능
- **수동 조종** : 블루투스(UART1, 9600bps) 또는 PC 시리얼(UART0, 9600bps, 원형 큐 버퍼)로 `F/B/L/R/S` 명령 처리
- **자율주행** : 좌/정면/우 3방향 초음파(HC-SR04)를 60ms 주기로 순차 측정해 벽을 회피하는 알고리즘
- **코너·갇힘 감지** : 양옆이 동시에 막히면 후진, 2초 이상 갇혀 있으면 자동 탈출 후진(1초) 후 재판단
- **모터 제어** : Timer1 Fast PWM(Mode 14, TOP=1023)으로 좌우 바퀴 속도를 개별 제어, PORTF 방향 핀으로 전후진·좌우회전 구현
- **모드 전환** : 버튼(PG1, 20ms 디바운싱)으로 수동↔자율 토글, LED(PG2·PG3)로 자율 모드 여부 표시
- **듀얼 FND 디스플레이** : 4자리 FND 한 세트에 스톱워치(mm:ss)와 현재 주행 상태 문자(F/B/L/R/S)를 Timer2 1ms 인터럽트로 동시 멀티플렉싱 출력

## 🔧 하드웨어 구성
| 항목 | 핀 / 설정 |
|---|---|
| MCU | ATmega128A @ 16MHz |
| 초음파 트리거 (좌/정면/우) | PA0 / PA1 / PA2 |
| 초음파 에코 인터럽트 (좌/정면/우) | PE4(INT4) / PE5(INT5) / PE6(INT6), Timer3로 시간 측정 |
| 모터 PWM (좌/우) | PB5 / PB6 (Timer1) |
| 모터 방향 (모터 드라이버 IN1~4) | PORTF 0~3 |
| 모드 전환 버튼 | PG1 |
| 자율주행 LED | PG2, PG3 |
| PC 시리얼 / 블루투스 | UART0 (9600bps) / UART1 (9600bps) |
| FND 데이터 / 자릿수 선택 | PORTC(데이터), PORTF 4~7(타이머 자릿수) · PORTA 4~7(상태 자릿수) |

## 🎮 조작 명령어
블루투스·PC 시리얼 공통 (대소문자 무관)

| 명령 | 동작 |
|---|---|
| `F` | 전진 |
| `B` | 후진 |
| `L` | 제자리 좌회전 |
| `R` | 제자리 우회전 |
| `S` | 정지 |

## 📁 프로젝트 구조
```
07.AUTO_CAR/
├── main.c          # 모드 분기(함수 포인터 배열), Timer0 1ms 틱
├── auto_car.c/h     # 버튼·자율주행 LED 제어
├── pwm.c/h          # Timer1 PWM, 모터 방향/속도 제어
├── ultrasonic.c/h   # 3채널 초음파 트리거·에코 측정
├── uart0.c/h        # PC 시리얼 통신 + 명령 큐
├── uart1.c/h        # 블루투스 통신
├── button.c/h       # 버튼 입력
├── led.c/h          # 상태 LED
├── fnd.c/h          # 듀얼 FND 디스플레이
└── def.h            # 동작 모드 상수
```

## 🚀 빌드 & 업로드
1. Atmel Studio(Microchip Studio)에서 `07.AUTO_CAR.cproj` 열기
2. Build 실행
3. ISP/JTAG 프로그래머로 ATmega128A 보드에 업로드
