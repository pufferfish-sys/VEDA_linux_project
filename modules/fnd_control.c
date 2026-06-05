#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>

// FND 핀 설정 및 숫자 배열
static int gpiopins[4] = {4, 1, 16, 15}; 
static int number[10][4] = {             
    {0,0,0,0}, {0,0,0,1}, {0,0,1,0}, {0,0,1,1}, {0,1,0,0},
    {0,1,0,1}, {0,1,1,0}, {0,1,1,1}, {1,0,0,0}, {1,0,0,1}
};

// 모든 FND 핀을 끄는 함수 (화면 초기화용)
static void clear_fnd(void) {
    for (int i = 0; i < 4; i++) {
        digitalWrite(gpiopins[i], HIGH); //
    }
}

// 특정 숫자 하나를 FND에 출력하는 함수
static void display_number(int num) {
    if (num < 0 || num > 9) return;

    //  새로운 숫자를 켜기 전에 화면을 한 번 끕니다
    clear_fnd();
    delay(10); // 10ms 동안 완전히 꺼진 상태를 유지하여 잔상과 칩 오작동을 방지

    // 그 후 새로운 비트 신호를 쏩니다.
    for (int i = 0; i < 4; i++) {
        digitalWrite(gpiopins[i], number[num][i] ? HIGH : LOW); //
    }
}

// 외부 서버가 동적 로드로 호출할 FND 제어 함수
int fnd_countdown(int start_num) {
    // 예외 처리: 0~9 범위를 벗어나면 에러 코드(-1) 반환
    if (start_num < 0 || start_num > 9) {
        return -1;
    }

    // GPIO 핀을 출력 모드로 초기화
    for (int i = 0; i < 4; i++) {
        pinMode(gpiopins[i], OUTPUT); //
    }

    // 시작 숫자부터 0까지 1초마다 감소하며 표시
    for (int current = start_num; current >= 0; current--) {
        display_number(current); // 내부에서 알아서 [끄고 -> 10ms 대기 -> 켜기]를 수행함
        delay(1000);             // 1초 대기
    }

    // 카운트다운이 최종 종료되었으므로 FND 화면을 소등
    clear_fnd();

    return 0; 
}
