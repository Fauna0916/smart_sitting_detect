#include <stdio.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_pwm.h"
#include "hi_adc.h"
#include "hi_task.h"
#include "hi_errno.h"
#include "iot_gpio.h"
#include "peripheral_control.h"
#include "smart_sitting_detect.h"

#define BUTTOM 8
#define BEEPER 9
#define RED 10
#define GREEN 11
#define BLUE 12
#define HUMAN_SENSOR 3

#define IOT_PWM_PORT_PWM0 0
#define IOT_PWM_PORT_PWM1 1
#define IOT_PWM_PORT_PWM2 2
#define IOT_PWM_PORT_PWM3 3
#define IOT_GPIO_PWM_FUNCTION 5
#define LAMP_CONTROL_FACTOR 600
#define PWM_FREQ_DIVITION 64000
#define PWM_DUTY_50 (50)
#define PWM_FREQ_4K (4000)
#define IO_FUNC_GPIO_8_GPIO 0
#define IOT_IO_PULL_UP 1
#define STACK_SIZE (4096)

#define ONE_SECOND (100u) // 100U=1s
#define FIVE_SECONDS (500u)
#define FIVE_MINUTES (30000u)
#define TIMER_DELAY_60S (6000U) // 6000u
#define HOUR (60)
#define BEEPER_CONTINUING (500)
#define HUMAN_EXIST (1000)

extern hi_u16 sitting;
extern hi_u16 running;
extern hi_u16 health_secs;
extern hi_u16 existence_secs;
hi_u16 cnt = 0;       // count an hour
hi_u16 sedentariness; // keep sitting too long
hi_float head;        // distance of head
hi_u16 beepState;     // controled by bottom
osTimerId_t beeper_tid;
osTimerId_t FIVE_SECONDS_tid;
osTimerId_t existence_tid;

static hi_void
OnButtonPressed(hi_void)
{
    beepState = HI_FALSE;
    osTimerStart(beeper_tid, FIVE_MINUTES); // stop beeper 5 minutes
}

hi_void stop_beeper_fiveMin(hi_void)
{
    beepState = HI_TRUE;
}

hi_void human_leave(hi_void)
{
    running = HI_FALSE;
    cnt = 0;
}

// count an hour
hi_void count_hour(hi_void)
{
    cnt++;
    if (cnt >= 60)
    {
        sedentariness = HI_TRUE;
        cnt = 0;
    }
    printf("cnt:%d\n", cnt);
}

hi_void count_health(hi_void)
{
    health_secs++;
    //printf("he:%d\n", health_secs);
}

hi_void count_existence(hi_void)
{
    existence_secs++;
    //printf("ex:%d\n", existence_secs);
}

static hi_void light_Init(hi_void)
{
    IoTGpioInit(RED);
    IoTGpioInit(GREEN);
    IoTGpioInit(BLUE);

    IoTGpioSetDir(RED, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(GREEN, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(BLUE, IOT_GPIO_DIR_OUT);

    hi_io_set_func(RED, IOT_GPIO_PWM_FUNCTION);
    hi_io_set_func(GREEN, IOT_GPIO_PWM_FUNCTION);
    hi_io_set_func(BLUE, IOT_GPIO_PWM_FUNCTION);

    hi_pwm_init(IOT_PWM_PORT_PWM1); // red
    hi_pwm_init(IOT_PWM_PORT_PWM2); // green
    hi_pwm_init(IOT_PWM_PORT_PWM3); // blue
}

static hi_void beeper_Init(hi_void)
{
    IoTGpioInit(BEEPER);
    hi_io_set_func(BEEPER, IOT_GPIO_PWM_FUNCTION);
    IoTGpioSetDir(BEEPER, IOT_GPIO_DIR_OUT);
    IoTPwmInit(IOT_PWM_PORT_PWM0);

    hi_io_set_func(BUTTOM, IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(BUTTOM, IOT_GPIO_DIR_IN);
    hi_io_set_pull(BUTTOM, IOT_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(BUTTOM, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnButtonPressed, NULL);
    beepState = HI_TRUE;
}

hi_void human_detect(hi_void)
{
    hi_u16 humanSensor = 0;

    hi_adc_read(HUMAN_SENSOR, &humanSensor, HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0);
    hi_sleep(TASKSLEEP);
    printf("human sensor:%d\n", humanSensor);
    if (humanSensor > HUMAN_EXIST) // human exist-1800  no-100
    {
        running = HI_TRUE;
        if (!osTimerIsRunning(existence_tid))
        {

            osTimerStart(existence_tid, ONE_SECOND);
        }
        osTimerStop(FIVE_SECONDS_tid);
    }
    else
    {
        osTimerStop(existence_tid);
        if (!osTimerIsRunning(FIVE_SECONDS_tid)) // human not exist and a timer task finished
        {
            // printf("\ntimer start\n");
            osTimerStart(FIVE_SECONDS_tid, FIVE_SECONDS); // make sure human leave
        }
    }
}

static hi_void PeripheralControlTask(hi_void *arg)
{
    (hi_void) arg;

    light_Init();
    beeper_Init();

    osTimerId_t sedentariness_tid = osTimerNew(count_hour, osTimerPeriodic, NULL, NULL);
    osTimerId_t health_tid = osTimerNew(count_health, osTimerPeriodic, NULL, NULL);
    hi_u16 duty = 0;

    while (1)
    {
        human_detect();
        if (running)
        {
            duty = LAMP_CONTROL_FACTOR * (hi_u16)head;
            hi_pwm_start(IOT_PWM_PORT_PWM1, duty, PWM_FREQ_DIVITION);
            hi_pwm_start(IOT_PWM_PORT_PWM2, duty, PWM_FREQ_DIVITION);
            hi_pwm_start(IOT_PWM_PORT_PWM3, duty, PWM_FREQ_DIVITION);
            printf("[LED] head:%.1f\n", head);

            // printf("\nsitting:%d\n", sitting);
            // printf("beepstate:%d\n", beepState);
            if (!sitting) // health
            {
                if (!osTimerIsRunning(health_tid))
                {
                    osTimerStart(health_tid, ONE_SECOND);
                }
            }
            else
            {
                osTimerStop(health_tid);
            }
            if (sitting && beepState)
            {
                //printf("Enter beeper\n");
                IoTPwmStart(IOT_PWM_PORT_PWM0, PWM_DUTY_50, PWM_FREQ_4K);
                hi_sleep(BEEPER_CONTINUING);
                hi_pwm_stop(IOT_PWM_PORT_PWM0);
            }

            if (!osTimerIsRunning(sedentariness_tid))
            {
                osTimerStart(sedentariness_tid, TIMER_DELAY_60S);
            }
        }
        else
        {
            osTimerStop(sedentariness_tid);
            hi_pwm_stop(IOT_PWM_PORT_PWM1);
            hi_pwm_stop(IOT_PWM_PORT_PWM2);
            hi_pwm_stop(IOT_PWM_PORT_PWM3);
        }
        hi_sleep(TASKSLEEP);
    }
}

hi_void PeripheralControl(hi_void)
{
    FIVE_SECONDS_tid = osTimerNew(human_leave, osTimerPeriodic, NULL, NULL);
    beeper_tid = osTimerNew(stop_beeper_fiveMin, osTimerPeriodic, NULL, NULL);
    existence_tid = osTimerNew(count_existence, osTimerPeriodic, NULL, NULL);

    osThreadAttr_t attr;
    attr.name = "peripheralTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(PeripheralControlTask, NULL, &attr) == NULL)
    {
        printf("[PeripheralControl] Failed to create PeripheralControlTask!\n");
    }
}
