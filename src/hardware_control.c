#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h> 
#include "hardware_control.h"

// 메인 모듈의 뮤텍스 자원을 원격 바인딩
extern pthread_mutex_t led_mutex;
extern pthread_mutex_t cds_mutex;
extern pthread_mutex_t buzzer_mutex;
extern pthread_mutex_t fnd_mutex;

// 동적 라이브러리 함수 구조 정의
typedef int (*led_func_t)(const char*);
typedef int (*led_init_func_t)(void);
typedef int (*cds_func_t)(void);
typedef int (*buzzer_func_t)(int);
typedef int (*fnd_func_t)(int);

static void *global_buzzer_handle = NULL;
static buzzer_func_t global_buzzer_play = NULL;

static void *led_handle = NULL;
static led_func_t led_control = NULL;

// FND 카운트다운 중복 실행 방지 플래그 (0: 대기중, 1: 동작중)
static int is_fnd_running = 0;

void init_led_library(void)
{
    led_handle =
        dlopen("/home/puffer/kjw_project/project_library/libled.so",
               RTLD_LAZY);

    if (!led_handle)
    {
        printf("LED Library Load Failed\n");
        return;
    }

    led_control = (led_func_t)dlsym(led_handle, "led_control");
    
    led_init_func_t init_led_pwm = (led_init_func_t)dlsym(led_handle, "init_led_pwm");
    if (init_led_pwm) {
        init_led_pwm(); 
    }
}

// 1) LED 제어 본체
void handle_led_logic(const char* command)
{
    pthread_mutex_lock(&led_mutex);
    
    if (led_control)
    {
        led_control(command);
    }

    pthread_mutex_unlock(&led_mutex);
}

int handle_cds_logic(void) {
    int result = -1;
    
    pthread_mutex_lock(&cds_mutex);
    pthread_mutex_lock(&led_mutex); // 조도센서-LED 자동연동 시 간섭 방어
    
    void *handle = dlopen("/home/puffer/kjw_project/project_library/libcds.so", RTLD_LAZY);
    if (handle) {
        cds_func_t process_cds_logic = (cds_func_t)dlsym(handle, "process_cds_logic");
        if (process_cds_logic) {
            result = process_cds_logic();
            
            if (result >= 0) {
                if (result < 180) { // 맑음 기준
                    if (led_control) led_control("off");
                } else {            // 어두움 기준
                    if (led_control) led_control("on");
                }
            }
        }
        dlclose(handle);
    }
    
    pthread_mutex_unlock(&led_mutex);
    pthread_mutex_unlock(&cds_mutex);
    
    return result;
}

// 부저 연주 전담 워커 스레드 함수
void *buzzer_play_thread(void *arg)
{
    if (!global_buzzer_handle)
    {
        global_buzzer_handle =
            dlopen("/home/puffer/kjw_project/project_library/libbuzzer.so",
                   RTLD_LAZY);

        if (!global_buzzer_handle)
        {
            printf("dlopen failed\n");
            return NULL;
        }

        global_buzzer_play =
            (buzzer_func_t)dlsym(global_buzzer_handle, "buzzer_play");
    }

    if (global_buzzer_play)
    {
        global_buzzer_play(1);
    }

    return NULL;
}

// 3) 부저 제어 본체 기본 함수
void handle_buzzer_logic(int option)
{
    if (option == 1)
    {
        pthread_t th;
        pthread_create(&th, NULL,buzzer_play_thread,NULL);
        pthread_detach(th);
    }
    else if (option == 0)
    {
        if (global_buzzer_play)
        {
            global_buzzer_play(0);
        }
    }
    else
    {
        pthread_mutex_lock(&buzzer_mutex);

        void *handle =
            dlopen("/home/puffer/kjw_project/project_library/libbuzzer.so",
                   RTLD_LAZY);

        if (handle)
        {
            buzzer_func_t buzzer_play =
                (buzzer_func_t)dlsym(handle, "buzzer_play");

            if (buzzer_play)
            {
                buzzer_play(option);
            }

            dlclose(handle);
        }

        pthread_mutex_unlock(&buzzer_mutex);
    }
}

// 4) FND 카운트다운 전용 독립 워커 스레드 
void *fnd_countdown_thread(void *arg) {
    int start_num = (int)(intptr_t)arg; 
    
    // 뮤텍스를 잡고 현재 실행 중인지 체크
    pthread_mutex_lock(&fnd_mutex); 
    if (is_fnd_running == 1) {
        // 이미 카운트다운이 돌고 있다면 대기하지 않고 즉시 탈출(중복 차단)
        pthread_mutex_unlock(&fnd_mutex);
        return NULL;
    }
    
    // 실행 중이 아니었으므로 플래그를 세우고 뮤텍스를 풀어 다른 로직의 병목 방지
    is_fnd_running = 1;
    pthread_mutex_unlock(&fnd_mutex); 
    
    // 카운트다운 구동 시작
    void *fnd_handle = dlopen("/home/puffer/kjw_project/project_library/libfnd.so", RTLD_LAZY);
    if (fnd_handle) {
        fnd_func_t fnd_countdown = (fnd_func_t)dlsym(fnd_handle, "fnd_countdown");
        if (fnd_countdown) {
            int status = fnd_countdown(start_num); // 라이브러리 내부 delay(1000)에 의해 수 초간 블로킹됨
            
            // 타이머 완료(0)에 도달하면 부저 경보 연계 작동
            if (status == 0) {
                handle_buzzer_logic(2); 
            }
        }
        dlclose(fnd_handle);
    }
    // 다시 대기 상태(0)로 전환
    pthread_mutex_lock(&fnd_mutex);
    is_fnd_running = 0;
    pthread_mutex_unlock(&fnd_mutex); 
    
    return NULL;
}