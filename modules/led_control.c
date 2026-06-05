#include <stdio.h>
#include <string.h>
#include <wiringPi.h>
#include <softPwm.h>

#define LED_PIN 0

// 초기화 전용 함수
int init_led_pwm(void) {
    pinMode(LED_PIN, OUTPUT);
    softPwmCreate(LED_PIN, 0, 100);
    return 0;
}

int led_control(const char *cmd) {
    if (strcmp(cmd, "off") == 0) {
        softPwmWrite(LED_PIN, 0);
        printf("[LED CONTROL] LED OFF\n");
    }
    else if (strcmp(cmd, "on") == 0 || strcmp(cmd, "max") == 0) {
        softPwmWrite(LED_PIN, 100);
        printf("[LED CONTROL] LED ON (MAX)\n");
    }
    else if (strcmp(cmd, "mid") == 0) {
        softPwmWrite(LED_PIN, 40);
        printf("[LED CONTROL] LED ON (MID)\n");
    }
    else if (strcmp(cmd, "min") == 0) {
        softPwmWrite(LED_PIN, 10);
        printf("[LED CONTROL] LED ON (MIN)\n");
    }
    else {
        printf("[LED CONTROL] 알 수 없는 명령: %s\n", cmd);
        return -1;
    }
    return 0;
}