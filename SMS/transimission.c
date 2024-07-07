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
#include <stdlib.h>
#include "datahandle.h"
#include "peripheral_control.h"

/* attribute initiative to report */
#define TAKE_THE_INITIATIVE_TO_REPORT
/* oc request id */
#define CN_COMMADN_INDEX "commands/request_id="
#define CN_IOT_TASK_STACKSIZE 0x1000
#define CN_IOT_TASK_PRIOR 25
#define MAX_DISTANCE (150)
#define ONE_SECOND (1000)
#define ONE_MINUTE (60000)

extern hi_u16 sitting;

typedef void (*FnMsgCallBack)(hi_gpio_value val);

typedef struct FunctionCallback
{
    hi_bool stop;
    hi_u32 conLost;
    hi_u32 queueID;
    hi_u32 iotTaskID;
    FnMsgCallBack msgCallBack;
} FunctionCallback;
FunctionCallback g_functinoCallback;

static hi_void MyMsgRcvCallBack(int qos, const char *topic, const char *payload)
{
   
}


// update sitting state
hi_void updateState(hi_u16 sitting)
{
    if (sitting) // sitting is unhealthy
    {
        IotSendMsg(0, "$oc/devices/6659c9747dbfd46fabbe3a0b_ToHi3861/user/state", "off");
    }
    else
    {
        IotSendMsg(0, "$oc/devices/6659c9747dbfd46fabbe3a0b_ToHi3861/user/state", "on");
    }
}


static hi_void *TransimissionTask(const char *arg)
{
    WifiStaReadyWait();
    cJsonInit();
    IoTMain();

    /* 云端下发回调 */
    IoTSetMsgCallback(MyMsgRcvCallBack);
    /* 主动上报 */
#ifdef TAKE_THE_INITIATIVE_TO_REPORT
    while (1)
    {
        updateState(sitting);   //update sitting position state
        hi_sleep(ONE_MINUTE);
#endif
        hi_sleep(ONE_SECOND);
    }
    return NULL;
}

hi_void Transimission(hi_void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();

    attr.name = "Transimission";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = CN_IOT_TASK_STACKSIZE;
    attr.priority = CN_IOT_TASK_PRIOR;

    if (osThreadNew((osThreadFunc_t)TransimissionTask, NULL, &attr) == NULL)
    {
        printf("[mqtt] Falied to create Transimission!\n");
    }
}

