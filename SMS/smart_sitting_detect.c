#include <hi_task.h>
#include <string.h>
#include <hi_wifi_api.h>
#include <hi_mux.h>
#include <hi_io.h>
#include <hi_gpio.h>
#include "iot_config.h"
#include "iot_log.h"
#include "iot_main.h"
#include "iot_profile.h"
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "datahandle.h"
#include "peripheral_control.h"

#define WAITCONNECT (35000)
extern hi_void Transimission(hi_void);

hi_u16 running = HI_TRUE;
hi_u16 sitting = HI_FALSE;

static hi_void Task(hi_void)
{
    Transimission();
    hi_sleep(WAITCONNECT);
    
    DepthProcess();
    PeripheralControl();
    OledReminder();
}

hi_void SMS(hi_void)
{
    osThreadAttr_t attr;
    
    attr.name = "APPTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024;
    attr.priority = osPriorityNormal; 

    if (osThreadNew(Task, NULL, &attr) == NULL)
    {
        printf("[App] Failed to create Task!\n");
    }
    else
    {
        printf("[App] success to create Task!\n");
    }
}

SYS_RUN(SMS);
