/*
 * battery.h
 *
 *  Created on: 25 d�c. 2015
 *      Author: Patrick
 */

#ifndef APPLICATION_USER_HAL_BATTERY_H_
#define APPLICATION_USER_HAL_BATTERY_H_


/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "personnalisation.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HAL settings ------------------------------------------------------------------*/

enum VBATT_ID
{
	VBATT = 0,
	VBATT_BIS,
	VBATT_COUNT
};

static float const hal_battery_voltage_hw_ration[VBATT_COUNT] = {
		VOLTAGE_RATIO, // calibrated, measured on board
		VOLTAGE_RATIO
};

static float const hal_battery_voltage_low_threshold[VBATT_COUNT] = {
	6.4, // V
	6.4
};

/* HAL Public Data ------------------------------------------------------------------*/


/* HAL Functions ------------------------------------------------------------------*/

void HAL_Battery_Init(void);

bool HAL_Battery_Is_Low(int id);

float HAL_Battery_Get(int id); // volt


#endif /* APPLICATION_USER_HAL_BATTERY_H_ */
