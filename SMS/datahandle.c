#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

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
#include "smart_sitting_detect.h"

#define CN_TASK_PRIOR 28
#define CN_TASK_STACKSIZE (1024 * 4)

#define MINIMUM_DISTANCE (0.5)
#define UPANDDOWN (3)
#define LEFTANDRIGHT (2)
#define TOLERANCE (4)

hi_float depth_data[ROWS][COLS];
hi_float threshold;
hi_u16 transimit_finish = HI_FALSE;
extern hi_u16 beepState;
extern hi_float head;
extern hi_u16 sitting;

osMutexAttr_t attrm = {0};
osMutexId_t mid;

// receive data
hi_void dataTransmit(char *buf)
{
    if (strstr(buf, "*"))
    {
        char *temp;
        float sum;
        sum = 0;

        float **raw = (float **)malloc(ROWS * sizeof(float *));
        for (int i = 0; i < ROWS; i++)
        {
            raw[i] = (float *)malloc(COLS * sizeof(float));
        }
        raw[0][0] = strtof(buf + 12, &temp); // extra 12 byte
        sum += raw[0][0];
        for (int j = 1; j < 30; j++)
        {
            raw[0][j] = strtof(temp + 1, &temp);
            sum += raw[0][j];
        }
        for (int i = 1; i < 29; i++)
        {
            for (int j = 0; j < 40; j++)
            {
                raw[i][j] = strtof(temp + 1, &temp);
                sum += raw[i][j];
            }
        }

        if (transimit_finish == HI_FALSE)
        {
            if (osMutexAcquire(mid, 100) == osOK)
            {
                for (int i = 1; i < 30; i++)
                {
                    for (int j = 0; j < 40; j++)
                    {
                        depth_data[i][j] = raw[i][j];
                    }
                }
                transimit_finish = HI_TRUE;
                osMutexRelease(mid);
            }
            threshold = sum / TOTALNUMS;
        }

        hi_sleep(200);
        if (raw[2] != NULL)
        {
            for (int i = 0; i < ROWS; i++)
            {
                free(raw[i]);
            }
            free(raw);
        }
    }
    hi_sleep(10);
}


hi_void background_Fliter()
{
    for (hi_u16 i = 0; i < ROWS; i++)
    {
        for (hi_u16 j = 0; j < COLS; j++)
        {
            if (isnan(depth_data[i][j]))
            {
                depth_data[i][j] = 0;
            }
            else if (depth_data[i][j] > threshold)
            {
                depth_data[i][j] = 0;
            }
            else if (depth_data[i][j] < MINIMUM_DISTANCE)
            {
                depth_data[i][j] = 0;
            }
        }
    }
}


//distance with head
hi_float find_Vertexs(hi_void)
{
    hi_u16 top_row = INT_MAX;
    hi_u16 top_col;
    hi_float topmost_value = 0;

    for (hi_u16 i = 0; i < ROWS; i++)
    {
        for (hi_u16 j = 0; j < COLS; j++)
        {
            if (depth_data[i][j] > 0)
            {
                if (i < top_row)
                {
                    top_row = i;
                    top_col = j;
                    topmost_value = depth_data[i][j];
                }
            }
        }
    }

    printf("[data] head:%.1f Position: (%d, %d)\n", topmost_value, top_row, top_col);

    return topmost_value;
}

// check if bend too much
hi_u16 Hunchback(hi_void)
{
    hi_float sumTop = 0, sumBottom = 0;
    hi_u16 countTop = 0, countBottom = 0;

    for (hi_u16 i = 0; i < ROWS / 2; i++)
    {
        for (hi_u16 j = 0; j < COLS; j++)
        {
            if (depth_data[i][j] != 0)
            {
                sumTop += depth_data[i][j];
                countTop++;
            }
        }
    }
    for (hi_u16 i = ROWS / 2; i < ROWS; i++)
    {
        for (hi_u16 j = 0; j < COLS; j++)
        {
            if (depth_data[i][j] != 0)
            {
                sumBottom += depth_data[i][j];
                // printf("depth %.1f\n", depth_data[i][j]);
                countBottom++;
            }
        }
    }

    // printf("sumbottom: %.1f\n", sumBottom);
    // printf("countbottom: %d\n", countBottom);
    if (countTop == 0 || countBottom == 0)
    {
        return HI_FALSE;
    }

    hi_float avgTop = sumTop / countTop;
    hi_float avgBottom = sumBottom / countBottom;
    hi_float comp = fabsf(avgTop - avgBottom);

    printf("[Data] up and down:%.1f\n", comp);
    // if difference of depth is too large
    if (comp > UPANDDOWN + TOLERANCE || comp < UPANDDOWN - TOLERANCE)
    {
        return HI_TRUE;
    }

    return HI_FALSE;
}

// check if tile too much
hi_u16 LeftRightTilt(hi_void)
{
    hi_float leftSum = 0, rightSum = 0;
    hi_u16 leftCount = 0, rightCount = 0;

    for (hi_u16 i = 0; i < ROWS; i++)
    {
        for (hi_u16 j = 0; j < COLS / 2; j++)
        {
            if (depth_data[i][j] != 0)
            {
                leftSum += depth_data[i][j];
                leftCount++;
            }
        }
    }
    for (hi_u16 i = 0; i < ROWS; i++)
    {
        for (hi_u16 j = COLS / 2; j < COLS; j++)
        {
            if (depth_data[i][j] != 0)
            {
                rightSum += depth_data[i][j];
                rightCount++;
            }
        }
    }

    if (leftCount == 0 || rightCount == 0)
        return HI_FALSE;

    hi_float leftAverage = leftSum / leftCount;
    hi_float rightAverage = rightSum / rightCount;
    hi_float comp = fabsf(leftAverage - rightAverage);

    printf("[Data] left and right:%.1f\n", comp);
    // if difference of depth is too large
    if (comp > LEFTANDRIGHT + TOLERANCE || comp < LEFTANDRIGHT - TOLERANCE)
    {
        return HI_TRUE;
    }

    return HI_FALSE;
}

hi_void judgeData(hi_void)
{
    head = find_Vertexs();

    if (Hunchback() || LeftRightTilt())
    {
        sitting = HI_TRUE;
    }
    else
    {
        sitting = HI_FALSE;
    }
}

static hi_void *ProcessEntry(const char *arg)
{
    mid = osMutexNew(&attrm);

    if (mid == NULL)
    {
        printf("[Mutex] create mutex in dataHandle failed.\r\n");
    }
    while (1)
    {
        if (transimit_finish)
        {
            background_Fliter();
            // for (int i = 0; i < 30; i++)
            // {
            //     for (int j = 0; j < 30; j++)
            //     {
            //         printf("%.1f ", depth_data[i][j]);
            //     }
            //     printf("\n");
            // }
            judgeData();

            if (osMutexAcquire(mid, 100) == osOK)
            {
                transimit_finish = HI_FALSE;
                osMutexRelease(mid);
            }
        }
        hi_sleep(TASKSLEEP);
    }
}

void DepthProcess(void)
{
    osThreadAttr_t attr;

    attr.name = "DEPTHPROCESS";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = CN_TASK_STACKSIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)ProcessEntry, NULL, &attr) == NULL)
    {
        printf("[Data] Falied to create DEPTHPROCESS!\n");
    }
}
