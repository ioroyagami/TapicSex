/* LEDC (LED Controller) basic example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define TAPICSEX_TAG                  "TapicSex"
#define TAPICSEX_DRIVE_TASK_NAME      "TapicSexDriveTask"
#define TAPICSEX_KEY_TASK_NAME        "TapicSexKeyTask"
#define TAPICSEX_TASK_STACKSIZE       2048
#define TAPICSEX_TASK_PRIORITY        10
#define TAPICSEX_MODE_SWITCH          33
#define TAPICSEX_STRENGTH_SWITCH      25
#define TAPICSEX_DRIVE_OUTPUT_IO0     26
#define TAPICSEX_DRIVE_OUTPUT_IO1     27
#define TAPICSEX_DRIVE_OUTPUT_IO2     14
#define TAPICSEX_DRIVE_OUTPUT_IO3     12
#define TAPICSEX_INPUT_PIN_SEL        ((1ULL << TAPICSEX_MODE_SWITCH) || (1ULL << TAPICSEX_STRENGTH_SWITCH))
#define TAPICSEX_DRIVE_CHANNEL_NUM    4
#define TAPICSEX_DRIVE_DUTY_RES       65536
#define TAPICSEX_DRIVE_DEFAULT_DUTY   32768
#define TAPICSEX_DRIVE_FREQUENCY      5000
#define TAPICSEX_WAVE_NUM             128
#define TAPICSEX_DRIVE_NUM            20
#define TAPICSEX_INTR_FLAG_DEFAULT    0
typedef enum {
    TAPICSEX_MODE_NORMAL,
    TAPICSEX_MODE_PWM,
    TAPICSEX_MODE_WAVE,
    TAPICSEX_MODE_MAX
} TapicSexModeStatus;

typedef enum {
    TAPICSEX_STRENGTH_OFF = 0,
    TAPICSEX_STRENGTH_STEP = 1,
    TAPICSEX_STRENGTH_MIN = 1,
    TAPICSEX_STRENGTH_DEFAULT = 2,
    TAPICSEX_STRENGTH_MAX = 4
} TapicSexStrengthStatus;

typedef struct {
    TapicSexModeStatus mode;
    TapicSexStrengthStatus strength;
} TapicSexStatus;

static TapicSexStatus g_tapSexStatus = {TAPICSEX_MODE_NORMAL, TAPICSEX_STRENGTH_DEFAULT};
static xQueueHandle g_tapSexKeyQueue = NULL;
static void IRAM_ATTR TapicSexKeyHandler(void* arg);

static uint16_t g_tapSexSineTable[] = {
        0x8000,0x8647,0x8c8b,0x92c7,0x98f8,0x9f19,0xa527,0xab1f,
        0xb0fb,0xb6b9,0xbc56,0xc1cd,0xc71c,0xcc3f,0xd133,0xd5f5,
        0xda82,0xded7,0xe2f1,0xe6cf,0xea6d,0xedc9,0xf0e2,0xf3b5,
        0xf641,0xf884,0xfa7c,0xfc29,0xfd89,0xfe9c,0xff61,0xffd8,
        0xffff,0xffd8,0xff61,0xfe9c,0xfd89,0xfc29,0xfa7c,0xf884,
        0xf641,0xf3b5,0xf0e2,0xedc9,0xea6d,0xe6cf,0xe2f1,0xded7,
        0xda82,0xd5f5,0xd133,0xcc3f,0xc71c,0xc1cd,0xbc56,0xb6b9,
        0xb0fb,0xab1f,0xa527,0x9f19,0x98f8,0x92c7,0x8c8b,0x8647,
        0x8000,0x79b8,0x7374,0x6d38,0x6707,0x60e6,0x5ad8,0x54e0,
        0x4f04,0x4946,0x43a9,0x3e32,0x38e3,0x33c0,0x2ecc,0x2a0a,
        0x257d,0x2128,0x1d0e,0x1930,0x1592,0x1236,0xf1d,0xc4a,
        0x9be,0x77b,0x583,0x3d6,0x276,0x163,0x9e,0x27,
        0x0,0x27,0x9e,0x163,0x276,0x3d6,0x583,0x77b,
        0x9be,0xc4a,0xf1d,0x1236,0x1592,0x1930,0x1d0e,0x2128,
        0x257d,0x2a0a,0x2ecc,0x33c0,0x38e3,0x3e32,0x43a9,0x4946,
        0x4f04,0x54e0,0x5ad8,0x60e6,0x6707,0x6d38,0x7374,0x79b8
};
static uint16_t g_tapSexPwmTable[] = {
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
static ledc_timer_config_t g_tapSexTimer = {
        .speed_mode       = LEDC_SPEED_MODE_MAX,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = TAPICSEX_DRIVE_DUTY_RES,
        .freq_hz          = TAPICSEX_DRIVE_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
};
ledc_channel_config_t g_tapSexChannel[TAPICSEX_DRIVE_CHANNEL_NUM] = {
        {
                .speed_mode     = LEDC_SPEED_MODE_MAX,
                .channel        = LEDC_CHANNEL_0,
                .timer_sel      = LEDC_TIMER_0,
                .intr_type      = LEDC_INTR_DISABLE,
                .gpio_num       = TAPICSEX_DRIVE_OUTPUT_IO0,
                .duty           = TAPICSEX_DRIVE_DEFAULT_DUTY,
                .hpoint         = 0
        }, {
                .speed_mode     = LEDC_SPEED_MODE_MAX,
                .channel        = LEDC_CHANNEL_1,
                .timer_sel      = LEDC_TIMER_0,
                .intr_type      = LEDC_INTR_DISABLE,
                .gpio_num       = TAPICSEX_DRIVE_OUTPUT_IO1,
                .duty           = TAPICSEX_DRIVE_DEFAULT_DUTY,
                .hpoint         = 0
        }, {
                .speed_mode     = LEDC_SPEED_MODE_MAX,
                .channel        = LEDC_CHANNEL_2,
                .timer_sel      = LEDC_TIMER_0,
                .intr_type      = LEDC_INTR_DISABLE,
                .gpio_num       = TAPICSEX_DRIVE_OUTPUT_IO2,
                .duty           = TAPICSEX_DRIVE_DEFAULT_DUTY,
                .hpoint         = 0
        }, {
                .speed_mode     = LEDC_SPEED_MODE_MAX,
                .channel        = LEDC_CHANNEL_3,
                .timer_sel      = LEDC_TIMER_0,
                .intr_type      = LEDC_INTR_DISABLE,
                .gpio_num       = TAPICSEX_DRIVE_OUTPUT_IO3,
                .duty           = TAPICSEX_DRIVE_DEFAULT_DUTY,
                .hpoint         = 0
        }
};
static uint16_t g_wave = 0;
static uint16_t g_pwm = 0;

static esp_err_t TapicSexKeyInit(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = TAPICSEX_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    g_tapSexKeyQueue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_install_isr_service(TAPICSEX_INTR_FLAG_DEFAULT) != ESP_OK) {
        ESP_LOGE(TAPICSEX_TAG, "gpio_install_isr_service failed\n");
        return ESP_FAIL;
    }
    if (gpio_isr_handler_add(TAPICSEX_MODE_SWITCH, TapicSexKeyHandler, (void*) TAPICSEX_MODE_SWITCH)) {
        ESP_LOGE(TAPICSEX_TAG, "gpio_isr_handler_add %d failed\n", TAPICSEX_MODE_SWITCH);
        return ESP_FAIL;
    }
    if (gpio_isr_handler_add(TAPICSEX_STRENGTH_SWITCH, TapicSexKeyHandler, (void*) TAPICSEX_STRENGTH_SWITCH)) {
        ESP_LOGE(TAPICSEX_TAG, "gpio_isr_handler_add %d failed\n", TAPICSEX_STRENGTH_SWITCH);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t TapicSexDriveInit(void)
{
    // Prepare and then apply the LEDC PWM timer configuration

    if (ledc_timer_config(&g_tapSexTimer) != ESP_OK) {
        ESP_LOGE(TAPICSEX_TAG, "ledc timer config failed\n");
        return ESP_FAIL;
    }

    for (int i = 0; i < TAPICSEX_DRIVE_CHANNEL_NUM; i++) {
        if (ledc_channel_config(&g_tapSexChannel[i]) != ESP_OK) {
            ESP_LOGE(TAPICSEX_TAG, "ledc channle config failed\n");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static void TapicSexSetPwm(uint16_t value)
{
    for (int i = 0; i < TAPICSEX_DRIVE_CHANNEL_NUM; i++) {
        ledc_set_duty(g_tapSexChannel[i].speed_mode, g_tapSexChannel[i].channel,
                      TAPICSEX_DRIVE_DUTY_RES * value / TAPICSEX_STRENGTH_MAX - 1);
        ledc_update_duty(g_tapSexChannel[i].speed_mode, g_tapSexChannel[i].channel);
    }
}

static void TapicSexSetWave(uint16_t value)
{
    for (int i = 0; i < TAPICSEX_DRIVE_CHANNEL_NUM; i++) {
        ledc_set_duty(g_tapSexChannel[i].speed_mode, g_tapSexChannel[i].channel, value);
        ledc_update_duty(g_tapSexChannel[i].speed_mode, g_tapSexChannel[i].channel);
    }
}

static uint16_t TapicSexUpdateWave(void)
{
    g_wave = (g_wave + 1) % TAPICSEX_WAVE_NUM;
    return g_tapSexSineTable[g_wave];
}

static uint16_t TapicSexUpdatePWM(void)
{
    g_pwm = (g_pwm + 1) % TAPICSEX_DRIVE_NUM;
    return g_tapSexPwmTable[g_pwm];
}

void TapicSexDriveTask(void *args)
{
    (void)args;
    ESP_LOGD(TAPICSEX_TAG, "task run\n");
    while (1) {
        switch (g_tapSexStatus.mode) {
            case TAPICSEX_MODE_NORMAL:
                TapicSexSetPwm(g_tapSexStatus.strength);
                break;
            case TAPICSEX_MODE_PWM:
                TapicSexSetPwm(TapicSexUpdatePWM());
                break;
            case TAPICSEX_MODE_WAVE:
                TapicSexSetWave(TapicSexUpdateWave());
                break;
            default:
                break;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void TapicSexModeEvent(void)
{
    g_tapSexStatus.mode = (g_tapSexStatus.mode + 1) % TAPICSEX_MODE_MAX;
}

void TapicSexStrengthEvent(void)
{
    g_tapSexStatus.strength = g_tapSexStatus.strength + TAPICSEX_STRENGTH_STEP;
    if (g_tapSexStatus.strength > TAPICSEX_STRENGTH_MAX) {
        g_tapSexStatus.strength = TAPICSEX_STRENGTH_MIN;
    }
}

static void IRAM_ATTR TapicSexKeyHandler(void* arg)
{
    uint32_t key = (uint32_t)arg;
    xQueueSendFromISR(g_tapSexKeyQueue, &key, NULL);
}

static void TapicSexKeyTask(void* arg)
{
    uint32_t key;
    for(;;) {
        if(xQueueReceive(g_tapSexKeyQueue, &key, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", key, gpio_get_level(key));
            switch (key) {
                case TAPICSEX_MODE_SWITCH:
                    TapicSexModeEvent();
                    break;
                case TAPICSEX_STRENGTH_SWITCH:
                    TapicSexStrengthEvent();
                    break;
                default:
                    break;
            }
        }
    }
}

int TapicSexTaskInit(void)
{
    int ret = xTaskCreate(TapicSexDriveTask, TAPICSEX_DRIVE_TASK_NAME, TAPICSEX_TASK_STACKSIZE, NULL, TAPICSEX_TASK_PRIORITY,
                          NULL);
    if (ret != ESP_OK) {
        ESP_LOGD(TAPICSEX_TAG, "create failed %d", ret);
        return ret;
    }
    ret = xTaskCreate(TapicSexKeyTask, TAPICSEX_KEY_TASK_NAME, TAPICSEX_TASK_STACKSIZE, NULL, TAPICSEX_TASK_PRIORITY,
                      NULL);
    if (ret != ESP_OK) {
        ESP_LOGD(TAPICSEX_TAG, "create failed %d", ret);
        return ret;
    }
    return ESP_OK;
}

void app_main(void)
{
    if (TapicSexDriveInit() != ESP_OK) {
        ESP_LOGE(TAPICSEX_TAG, "tapSex drive init failed\n");
    }
    if (TapicSexKeyInit() != ESP_OK) {
        ESP_LOGE(TAPICSEX_TAG, "tapSex key init failed\n");
    }
    if (TapicSexTaskInit() != ESP_OK) {
        ESP_LOGE(TAPICSEX_TAG, "tapSex task init failed\n");
    }
}
