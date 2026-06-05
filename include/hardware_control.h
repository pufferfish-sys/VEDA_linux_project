#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

// 동적 라이브러리 함수 포인터 타입 정의
typedef int (*led_func_t)(const char*);
typedef int (*cds_func_t)(void);
typedef int (*buzzer_func_t)(int);
typedef int (*fnd_func_t)(int);

// 💡 메인 서버에서 호출할 하드웨어 전담 제어 함수 선언
void init_led_library(void);
void handle_led_logic(const char* command);
int handle_cds_logic(void);
void handle_buzzer_logic(int option);
void *fnd_countdown_thread(void *arg); // FND는 독립 쓰레드용

#endif // HARDWARE_CONTROL_H
