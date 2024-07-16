#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_task.h"
#include "iot_gpio.h"
#include "hi_gpio.h"
#include "iot_errno.h"
#include "oled_ssd1306.h"
#include "smart_sitting_detect.h"
#include "hi_uart.h"
#include "iot_uart.h"
#include "hi_io.h"
#include "hi_types.h"

#define REMEINDING (500U)
#define CMDSIZE (10)
#define SENDINTERVAL (10)

enum
{
    NORMAL,
    WRONGSITTING,
    TOOLONG,
};

enum
{
    ADD,
    MINUS,
};

extern hi_u16 running;
extern hi_u16 sitting;
extern hi_u16 sedentariness;
osTimerId_t five_seconds_tid;

hi_void remind_Finish(hi_void)
{
    sedentariness = HI_FALSE;
    hi_gpio_set_ouput_val(HI_GPIO_IDX_2, 1);
}

void Uart1GpioInit(void)
{
    IoTGpioInit(HI_GPIO_IDX_0);
    // 设置GPIO0的管脚复用关系为UART1_TX Set the pin reuse relationship of GPIO0 to UART1_ TX
    hi_io_set_func(HI_UART_IDX_0, HI_IO_FUNC_GPIO_0_UART1_TXD);
    IoTGpioInit(HI_GPIO_IDX_1);
    // 设置GPIO1的管脚复用关系为UART1_RX Set the pin reuse relationship of GPIO1 to UART1_ RX
    hi_io_set_func(HI_UART_IDX_1, HI_IO_FUNC_GPIO_1_UART1_RXD);

    IoTGpioInit(HI_GPIO_IDX_2);
    IoTGpioSetDir(HI_GPIO_IDX_2, HI_GPIO_DIR_OUT);
    hi_gpio_set_ouput_val(HI_GPIO_IDX_2, 1);
}

void Uart1Config(void)
{
    uint32_t ret;
    /* 初始化UART配置，波特率 9600，数据bit为8,停止位1，奇偶校验为NONE */
    /* Initialize UART configuration, baud rate is 9600, data bit is 8, stop bit is 1, parity is NONE */
    IotUartAttribute uart_attr = {
        .baudRate = 9600,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    ret = IoTUartInit(HI_UART_IDX_1, &uart_attr);
    if (ret != IOT_SUCCESS)
    {
        printf("Init Uart1 Falied Error No : %d\n", ret);
        return;
    }
}

static void sendCmd(hi_u32 cmd)
{
    char des[CMDSIZE];
    sprintf(des, "%x", cmd);
    hi_uart_write(HI_UART_IDX_1, des, 2);
    hi_sleep(SENDINTERVAL);
}

// initialise volume to 20
void setVol(void)
{
    sendCmd(0xAA);
    sendCmd(0x02);
    sendCmd(0x00);
    sendCmd(0xAC);
}

void adjustVol(hi_u16 dir, hi_u16 val)
{
    // add 0xAA, 0x14, 0x00, 0xBE,
    // minus 0xAA, 0x15, 0x00, 0xBF,
}

// play the first voice reminder
void playone(void)
{
    sendCmd(0xAA);
    sendCmd(0x07);
    sendCmd(0x02);
    sendCmd(0x00);
    sendCmd(0x01);
    sendCmd(0xB4);
}

hi_void reminder(uint16_t type)
{
    switch (type)
    {
    case NORMAL:
        OledShowString(0, 2, "Your sitting are healthy", 1);
        break;
    case WRONGSITTING:
        OledShowString(0, 0, "Please adjust your sitting position", 1);
        break;
    case TOOLONG:
        OledShowString(0, 0, "Standing up and moving around for a while", 1);
        playone();
        hi_gpio_set_ouput_val(HI_GPIO_IDX_2, 0);
        osTimerStart(five_seconds_tid, REMEINDING); // keep this remind 5 seconds
        break;
    default:
        OledShowString(0, 2, "Your sitting are healthy", 1);
        break;
    }
}

static hi_void OledRemindering(hi_void)
{
    hi_u16 last, cur;

    last = NORMAL;
    five_seconds_tid = osTimerNew(remind_Finish, osTimerPeriodic, NULL, NULL);
    setVol();
    while (1)
    {
        if (running)
        {

            if (sedentariness)
            {
                cur = TOOLONG;
            }
            else if (sitting)
            {
                cur = WRONGSITTING;
            }
            else
            {
                cur = NORMAL;
            }
            if (cur != last)
            {
                OledFillScreen(0); // clear screen to change displayed sentences
            }
            reminder(cur);
            last = cur;
        }
        else
        {
            OledFillScreen(0);
        }
        hi_sleep(TASKSLEEP);
    }
}

static hi_void ReminderTask(hi_void *arg)
{
    (void)arg;

    OledInit();
    OledFillScreen(0);

    Uart1GpioInit();
    Uart1Config();

    OledRemindering();
}

void Reminder(void)
{
    osThreadAttr_t attr;
    attr.name = "OledmentTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 4096;
    attr.priority = 30;

    if (osThreadNew(ReminderTask, NULL, &attr) == NULL)
    {
        printf("[OledDemo] Falied to create OledmentTask!\n");
    }
}
