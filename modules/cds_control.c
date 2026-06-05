#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define LED_PIN 0 // LED 

int process_cds_logic(void) {
    int fd;
    int a2dChannel = 0;
    int trash, a2dVal;
    //int threshold = 180;

    fd = wiringPiI2CSetupInterface("/dev/i2c-1", 0x48);
    if (fd < 0) return -1;

    wiringPiI2CWrite(fd, 0x40 | a2dChannel);
    trash = wiringPiI2CRead(fd);
    a2dVal = wiringPiI2CRead(fd);


    printf("[CDS CONTROL] CDS Value: %d\n", a2dVal);
    return a2dVal;
}