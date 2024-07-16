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
#include "hi_mem.h"

/* oc request id */
#define CN_COMMADN_INDEX "commands/request_id="
#define CN_IOT_TASK_STACKSIZE 0x1000
#define CN_IOT_TASK_PRIOR 25
#define MAX_DISTANCE (150)
#define ONE_SECOND (1000)
#define ONE_MINUTE (60000)
#define STATESIZE (25)
#define TIMER_DELAY_60S (6000U)

hi_u16 health_secs; // count the number of seconds per minute in which user sit healthy
hi_u16 existence_secs;
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
hi_void updateState(hi_void)
{
    char *state;
    state = hi_malloc(0, STATESIZE);
    sprintf(state, "healthy:%d;exist:%d", health_secs, existence_secs);

    IotSendMsg(0, "$oc/devices/6659c9747dbfd46fabbe3a0b_ToHi3861/user/state", state);
    health_secs = 0;
    existence_secs = 0;
}

hi_void updateCallback(hi_void)
{
    updateState();
}

static hi_void *TransimissionTask(const char *arg)
{
    WifiStaReadyWait();
    cJsonInit();
    IoTMain();

    /* 云端下发回调 */
    IoTSetMsgCallback(MyMsgRcvCallBack);

    osTimerId_t update_tid = osTimerNew(updateCallback, osTimerPeriodic, NULL, NULL);

    // update sitting position state
    while (1)
    {
        osTimerStart(update_tid, TIMER_DELAY_60S);
        hi_sleep(ONE_MINUTE);
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
