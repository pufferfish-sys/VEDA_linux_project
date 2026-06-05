#include <wiringPi.h>
#include <softTone.h>
#include <stdio.h>

#define SPKR 6   // 부저 핀 번호 

#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988

#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_REST 0 

volatile int buzzer_stop_flag = 0;

static int mario_main_notes[] = {
  NOTE_E6, NOTE_E6, NOTE_REST, NOTE_E6, NOTE_REST, NOTE_C6, NOTE_E6, NOTE_REST, NOTE_G6, NOTE_REST, NOTE_REST, NOTE_REST, NOTE_G5, NOTE_REST, NOTE_REST, NOTE_REST, 
  NOTE_C6, NOTE_REST, NOTE_REST, NOTE_G5, NOTE_REST, NOTE_REST, NOTE_E5, NOTE_REST, NOTE_REST, NOTE_A5, NOTE_REST, NOTE_B5, NOTE_REST, NOTE_AS5, NOTE_A5, NOTE_REST, 
  NOTE_G5, NOTE_E6, NOTE_G6, NOTE_A6, NOTE_REST, NOTE_F6, NOTE_G6, NOTE_REST, NOTE_E6, NOTE_REST, NOTE_C6, NOTE_D6, NOTE_B5, NOTE_REST, NOTE_REST,
  NOTE_C6, NOTE_REST, NOTE_REST, NOTE_G5, NOTE_REST, NOTE_REST, NOTE_E5, NOTE_REST, NOTE_REST, NOTE_A5, NOTE_REST, NOTE_B5, NOTE_REST, NOTE_AS5, NOTE_A5, NOTE_REST, 
  NOTE_G5, NOTE_E6, NOTE_G6, NOTE_A6, NOTE_REST, NOTE_F6, NOTE_G6, NOTE_REST, NOTE_E6, NOTE_REST, NOTE_C6, NOTE_D6, NOTE_B5, NOTE_REST, NOTE_REST
};

static int mario_main_durations[] = {
  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 
  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 
  9, 9, 9, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
  9, 9, 9, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
};

// 외부 제어 서버 연결 및 연주용 메인 함수
int buzzer_play(int cmd) {
    softToneCreate(SPKR);

    if (cmd == 0) {
        // [명령 0]: 즉각 정지 인터럽트
        buzzer_stop_flag = 1; 
        softToneWrite(SPKR, 0); 
        return 0;
    }

    if (cmd == 1) {
        buzzer_stop_flag = 0; 
        int total_notes = sizeof(mario_main_notes) / sizeof(mario_main_notes[0]);
        
        for (int i = 0; i < total_notes; i++) {
            if (buzzer_stop_flag == 1) break;

            int note_duration = 1000 / mario_main_durations[i];
            softToneWrite(SPKR, mario_main_notes[i]);
            
            int target_delay = note_duration * 1.65;
            int elapsed = 0;
            
            while (elapsed < target_delay) {
                if (buzzer_stop_flag == 1) {
                    softToneWrite(SPKR, 0);
                    return 0; 
                }
                
                if (elapsed >= note_duration) {
                    softToneWrite(SPKR, 0);
                }

                delay(10); 
                elapsed += 10;
            }
            
            if (buzzer_stop_flag == 1) break;
            softToneWrite(SPKR, 0);
        }
        softToneWrite(SPKR, 0); 
    } 
    else if (cmd == 2) {
        buzzer_stop_flag = 0; 
        softToneWrite(SPKR, 1047); 
        int total_alarm_time = 1500;
        int alarm_elapsed = 0;
        while (alarm_elapsed < total_alarm_time) {
            if (buzzer_stop_flag == 1) break;
            delay(10);
            alarm_elapsed += 10;
        }
        softToneWrite(SPKR, 0);    
    }

    return 0;
}