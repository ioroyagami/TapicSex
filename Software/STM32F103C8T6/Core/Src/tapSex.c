//
// Created by lii on 2022/4/10.
//
#include "tapSex.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tapSexDef.h"
#include "cmsis_os2.h"
#include "stm32f1xx_hal.h"
#include "main.h"
#include "tapSexLog.h"

#define TAPSEX_TASK_NAME          "TapSexDriveTask"
#define TAPSEX_TASK_STACKSIZE     1024
#define TAPSEX_TASK_PRIORITY      2
#define TAPSEX_POWER_SWITCH       GPIO_PIN_12
#define TAPSEX_MODE_SWITCH        GPIO_PIN_13
#define TAPSEX_STRENGTH_SWITCH    GPIO_PIN_14
#define TAPSEX_PWM_CHANNEL_NUM    4
#define TAPSEX_PWM_PULSE_MAX      65536
#define TAPSEX_WAVE_NUM           128
#define TAPSEX_PWM_NUM            20

typedef enum {
    TAPSEX_POWER_OFF,
    TAPSEX_POWER_ON
} TapSexPowerStatus;

typedef enum {
    TAPSEX_MODE_NORMAL,
    TAPSEX_MODE_PWM,
    TAPSEX_MODE_WAVE,
    TAPSEX_MODE_MAX
} TapSexModeStatus;

typedef enum {
    TAPSEX_STRENGTH_OFF = 0,
    TAPSEX_STRENGTH_STEP = 1,
    TAPSEX_STRENGTH_MIN = 1,
    TAPSEX_STRENGTH_DEFAULT = 2,
    TAPSEX_STRENGTH_MAX = 4
} TapSexStrengthStatus;

typedef struct {
    TapSexPowerStatus power;
    TapSexModeStatus mode;
    TapSexStrengthStatus strength;
} TapSexStatus;

TaskHandle_t g_tapSexTaskHandle = NULL;
static TapSexStatus g_tapSexStatus = {TAPSEX_POWER_OFF, TAPSEX_MODE_NORMAL, TAPSEX_STRENGTH_DEFAULT};
static TIM_HandleTypeDef g_htim;
static uint32_t g_channelGroup[TAPSEX_PWM_CHANNEL_NUM] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
void TapSexPowerEvent(void)
{
    g_tapSexStatus.power = !g_tapSexStatus.power;
}

void TapSexModeEvent(void)
{
    g_tapSexStatus.mode = (g_tapSexStatus.mode + 1) % TAPSEX_MODE_MAX;
}

void TapSexStrengthEvent(void)
{
    g_tapSexStatus.strength = g_tapSexStatus.strength + TAPSEX_STRENGTH_STEP;
    if (g_tapSexStatus.strength > TAPSEX_STRENGTH_MAX) {
        g_tapSexStatus.strength = TAPSEX_STRENGTH_MIN;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t mode)
{
    TAPSEX_LOG_INFO("GPIO exit get %d", mode);
    switch (mode) {
        case TAPSEX_POWER_SWITCH:
            TapSexPowerEvent();
            break;
        case TAPSEX_MODE_SWITCH:
            TapSexModeEvent();
            break;
        case TAPSEX_STRENGTH_SWITCH:
            TapSexStrengthEvent();
            break;
        default:
            break;
    }
}

static void TapSexSetPwm(uint16_t value)
{
    TIM_OC_InitTypeDef sConfigOC;
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = TAPSEX_PWM_PULSE_MAX * value / TAPSEX_STRENGTH_MAX;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    for (int i = 0; i < TAPSEX_PWM_CHANNEL_NUM; i++) {
        HAL_TIM_PWM_ConfigChannel(&g_htim, &sConfigOC, g_channelGroup[i]);
        HAL_TIM_PWM_Start(&g_htim, g_channelGroup[i]);
    }
}

static void TapSexSetWave(uint16_t value)
{
    TIM_OC_InitTypeDef sConfigOC;
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = value;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    for (int i = 0; i < TAPSEX_PWM_CHANNEL_NUM; i++) {
        HAL_TIM_PWM_ConfigChannel(&g_htim, &sConfigOC, g_channelGroup[i]);
        HAL_TIM_PWM_Start(&g_htim, g_channelGroup[i]);
    }
}

uint16_t TapSexUpdateWave(void)
{
    static uint16_t wave = 0;
    wave = (wave + 1) % TAPSEX_WAVE_NUM;
    return tapSexSineTable[wave];
}

uint16_t TapSexUpdatePWM(void)
{
    static uint16_t pwm = 0;
    pwm = (pwm + 1) % TAPSEX_PWM_NUM;
    return tapSexPwmTable[pwm];
}

void TapSexDriveTask(void *args)
{
    UNUSED(args);
    TAPSEX_LOG_ERROR("task run\n");
    while (1) {
        if (g_tapSexStatus.power == TAPSEX_POWER_OFF) {
            TapSexSetPwm(TAPSEX_STRENGTH_OFF);
        } else {
            switch (g_tapSexStatus.mode) {
                case TAPSEX_MODE_NORMAL:
                    TapSexSetPwm(g_tapSexStatus.strength);
                    break;
                case TAPSEX_MODE_PWM:
                    TapSexSetPwm(TapSexUpdatePWM());
                    break;
                case TAPSEX_MODE_WAVE:
                    TapSexSetWave(TapSexUpdateWave());
                    break;
                default:
                    break;
            }
        }
        osDelay(50);
    }
}

int TapSexTaskInit(void)
{
    int ret = xTaskCreate(TapSexDriveTask, TAPSEX_TASK_NAME, TAPSEX_TASK_STACKSIZE, NULL, TAPSEX_TASK_PRIORITY,
                          &g_tapSexTaskHandle);
    if (ret != TAPSEX_OK) {
        printf("%s create failed %d", __FUNCTION__, ret);
        return ret;
    }
    return TAPSEX_OK;
}

void TapSexInit(void)
{
    g_htim = GetTimHandle();
}

