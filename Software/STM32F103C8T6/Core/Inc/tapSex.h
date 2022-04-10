//
// Created by lii on 2022/4/10.
//

#ifndef STM32F103C8T6_TAPSEX_H
#define STM32F103C8T6_TAPSEX_H

#include "stm32f1xx_hal.h"

int TapSexTaskInit(void);
void TapSexSetHtim3(TIM_HandleTypeDef htim);
void TapSexSetHtim4(TIM_HandleTypeDef htim);

#endif //STM32F103C8T6_TAPSEX_H
