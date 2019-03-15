/*
 * controller.c
 *
 *  Created on: 1 mars 2019
 *      Author: Invite
 */

// TODO : do LEFT and RIGHT turn

// TODO : replace Tick timer (ms) with a an accurate timebase timer (us) and tune controller period (833Hz), or use HW external interupt from gyro
// TODO : pulse LED when changing action
// TODO : check position error visually and adjust wheel diameter (compensation ratio)

// TODO : tune x speed PID (Kp and Ki)
// TODO : tune w speed filter and PID (Kp and Ki)

// TODO : log absolute distance
// TODO : trim/set absolute distance when changing state in order to preserve position error

// TODO : filter x_speed using EWMA and high alpha (0.5)
// TODO : use unfiltered x speed error for Ki
// TODO : use filtered x speed error for Kp and Kd

#include "controller.h"
#include "serial.h"
#include "motor.h"
#include "encoder.h"
#include "pid.h"
#include "datalogger.h"
#include "math.h"
#include "imu.h"

// globals
extern HAL_Serial_Handler com;

// constants
#define MAX_ACCELERATION 5.0 		// m/s-2
#define MAX_DECELERATION 3.0		// m/s-2
#define X_SPEED_LEARNING_RUN 0.4 	// m/s
#define DIST_START 0.09 			// m
#define DIST_RUN_1 0.18 			// m
#define DIST_STOP 0.09 				// m

// speed
#define X_SPEED_KP 600.0
#define X_SPEED_KI 10.0
#define X_SPEED_KD 0.0

// rotation
#define W_SPEED_KP 0.1
#define W_SPEED_KI 0.0
#define W_SPEED_KD 0.0

// ENUM

typedef enum {
	ACTION_IDLE,
	ACTION_START,
	ACTION_RUN_1,
	ACTION_TURN_RIGHT,
	ACTION_TURN_LEFT,
	ACTION_STOP,
	ACTION_CTR
} action_t;

// STRUCTURES DEFINITIONS

typedef struct  {
	// controller fsm
	uint32_t actions_nb; // index of current action in the scenario array
	uint32_t time;
	uint32_t sub_action_state;
	uint32_t gyro_state;

	// forward speed
	float x_speed_target;
	float x_speed_setpoint;
	float x_speed_current;
	float x_speed_error;
	float x_speed_pwm;

	pid_context_t x_speed_pid;

	// rotation speed
	float w_speed_target;
	float w_speed_setpoint;
	float w_speed_current;
	float w_speed_error;
	float w_speed_pwm;

	pid_context_t w_speed_pid;

} controller_t;

// GLOBAL VARIABLES

static action_t actions_scenario[] = {
	ACTION_START,
	ACTION_RUN_1,
	ACTION_RUN_1,
	ACTION_RUN_1,
	ACTION_RUN_1,
	ACTION_STOP,
	ACTION_IDLE
  };

static controller_t ctx;

// PUBLIC FUNCTIONS

uint32_t controller_init () // return GYRO ERROR (ZERO is GYRO OK)
{
	// reset controller fsm
	ctx.actions_nb = 0;
	ctx.time = 0;
	ctx.sub_action_state = 0;

	// forward speed
	ctx.x_speed_target = 0;
	ctx.x_speed_current = 0;
	ctx.x_speed_setpoint = 0;
	ctx.x_speed_error = 0;
	ctx.x_speed_pwm = 0;
	pid_init(&ctx.x_speed_pid, X_SPEED_KP, X_SPEED_KI, X_SPEED_KD);

	// rotation speed
	ctx.w_speed_target = 0;
	ctx.w_speed_current = 0;
	ctx.w_speed_setpoint = 0;
	ctx.w_speed_error = 0;
	ctx.w_speed_pwm = 0;
	pid_init(&ctx.w_speed_pid, W_SPEED_KP, W_SPEED_KI, W_SPEED_KD);

	motor_init();
	encoder_init();
	ctx.gyro_state = gyro_init();

	HAL_DataLogger_Init(7, // number of fields
			1,  // size in bytes of each field
			1, 	// size in bytes of each field
			4, 	// size in bytes of each field
			4, 	// size in bytes of each field
			4, 	// size in bytes of each field
			4, 	// size in bytes of each field
			1 	// size in bytes of each field
	);

	return ctx.gyro_state;
}

void controller_start(){
	// reset controller fsm
	ctx.actions_nb = 0;
	ctx.time = HAL_GetTick();
	ctx.sub_action_state = 0;

	// forward speed
	ctx.x_speed_target = 0;
	ctx.x_speed_current = 0;
	ctx.x_speed_error = 0;
	ctx.x_speed_setpoint = 0;
	ctx.x_speed_pwm = 0;
	pid_reset(&ctx.x_speed_pid);

	// rotation speed
	ctx.w_speed_target = 0;
	ctx.w_speed_current = 0;
	ctx.w_speed_setpoint = 0;
	ctx.w_speed_error = 0;
	ctx.w_speed_pwm = 0;
	pid_reset(&ctx.w_speed_pid);

	encoder_reset();

	HAL_DataLogger_Clear();
}

void controller_stop(){
	// reset controller fsm
	ctx.actions_nb = 0;
	ctx.time = HAL_GetTick();
	ctx.sub_action_state = 0;

	// forward speed
	ctx.x_speed_target = 0;
	ctx.x_speed_current = 0;
	ctx.x_speed_setpoint = 0;
	ctx.x_speed_error = 0;
	ctx.x_speed_pwm = 0;
	pid_reset(&ctx.x_speed_pid);

	// rotation speed
	ctx.w_speed_target = 0;
	ctx.w_speed_current = 0;
	ctx.w_speed_setpoint = 0;
	ctx.w_speed_error = 0;
	ctx.w_speed_pwm = 0;
	pid_reset(&ctx.w_speed_pid);

	encoder_reset();

	motor_speed_left(0);
	motor_speed_right(0);
}

void controller_fsm(); // forward declaration

void controller_update(){
	// cadence a 1ms
	uint32_t time_temp = HAL_GetTick();
	if(time_temp > ctx.time)
	{
		ctx.time = time_temp;

		// sensor update
		encoder_update();
		gyro_update();

		// motor control update
		controller_fsm();

		// data logger
		HAL_DataLogger_Record(7, 						// number of fields
				(int32_t)(ctx.actions_nb), 				// integer value of each field
				(int32_t)(ctx.sub_action_state),		// integer value of each field
				(int32_t)(ctx.w_speed_target * 1000.0),	// integer value of each field
				(int32_t)(ctx.w_speed_setpoint * 1000.0),	// integer value of each field
				(int32_t)(ctx.w_speed_current * 10.0),	// integer value of each field
				(int32_t)(ctx.w_speed_error * 10.0),	// integer value of each field
				(int32_t)(ctx.w_speed_pwm)				// integer value of each field
		);
		/*
		static uint32_t counter=0;
		if(counter++%10==0)
		{
			HAL_Serial_Print(&com,"GYRO %d\r\n",(int32_t)(gyro_get_dps()*1000));
		}
		*/
	}
}

bool controller_is_end(){
	return actions_scenario[ctx.actions_nb] == ACTION_IDLE;
}

// PRIVATE FUNCTIONS

void controller_fsm()
{
	switch(actions_scenario[ctx.actions_nb])
	{
	case ACTION_IDLE :
	{
		// forward speed
		ctx.x_speed_target = 0;
		ctx.x_speed_setpoint = 0;
		ctx.x_speed_current = 0;
		ctx.x_speed_error = 0;
		ctx.x_speed_pwm = 0;

		// rotation speed
		ctx.w_speed_target = 0;
		ctx.w_speed_setpoint = 0;
		ctx.w_speed_current = 0;
		ctx.w_speed_error = 0;
		ctx.w_speed_pwm = 0;

		motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
		motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

	}
	break;

	case ACTION_START :
	{
		// forward speed
		ctx.x_speed_target = X_SPEED_LEARNING_RUN;
		ctx.x_speed_setpoint = next_speed(ctx.x_speed_target, MAX_ACCELERATION, MAX_DECELERATION, 0.001, ctx.x_speed_setpoint);
		ctx.x_speed_current = ((encoder_get_delta_left() + encoder_get_delta_right()) / 2.0) / 0.001;
		ctx.x_speed_error = ctx.x_speed_setpoint - ctx.x_speed_current;
		ctx.x_speed_pwm = pid_output(&ctx.x_speed_pid, ctx.x_speed_error);

		// rotation speed
		ctx.w_speed_target = 0;
		ctx.w_speed_setpoint = 0;
		ctx.w_speed_current = gyro_get_dps();
		ctx.w_speed_error = ctx.w_speed_setpoint - ctx.w_speed_current;
		ctx.w_speed_pwm = pid_output(&ctx.w_speed_pid, ctx.w_speed_error);

		motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
		motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

		float dist = encoder_get_absolute();
		if(dist > DIST_START)
		{
			++ctx.actions_nb;
			ctx.sub_action_state = 0;

			encoder_reset();
			// TODO : positionner distance au d�but du mouvement (remaining distance)
			// TODO : remplacer reset par incr�mentation de la distance pour conserver l'�ventuelle erreur de position
		}

	}
	break;

	case ACTION_RUN_1 :
	{
		// forward speed
		ctx.x_speed_target = X_SPEED_LEARNING_RUN;
		ctx.x_speed_setpoint = next_speed(ctx.x_speed_target, MAX_ACCELERATION, MAX_DECELERATION, 0.001, ctx.x_speed_setpoint);
		ctx.x_speed_current = ((encoder_get_delta_left() + encoder_get_delta_right()) / 2.0) / 0.001;
		ctx.x_speed_error = ctx.x_speed_setpoint - ctx.x_speed_current;
		ctx.x_speed_pwm = pid_output(&ctx.x_speed_pid, ctx.x_speed_error);

		// rotation speed
		ctx.w_speed_target = 0;
		ctx.w_speed_setpoint = 0;
		ctx.w_speed_current = gyro_get_dps();
		ctx.w_speed_error = ctx.w_speed_setpoint - ctx.w_speed_current;
		ctx.w_speed_pwm = pid_output(&ctx.w_speed_pid, ctx.w_speed_error);

		motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
		motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

		float dist = encoder_get_absolute();
		if(dist > DIST_RUN_1)
		{
			++ctx.actions_nb;
			ctx.sub_action_state = 0;

			encoder_reset();
		}
	}
	break;

	case ACTION_TURN_RIGHT :
	{
		// TODO :

	}
	break;

	case ACTION_TURN_LEFT :
	{
		// TODO :

	}
	break;

	case ACTION_STOP :
	{
		switch (ctx.sub_action_state) {
			case 0 :
			{
				// forward speed
				ctx.x_speed_target = X_SPEED_LEARNING_RUN;
				ctx.x_speed_setpoint = next_speed(ctx.x_speed_target, MAX_ACCELERATION, MAX_DECELERATION, 0.001, ctx.x_speed_setpoint);
				ctx.x_speed_current = ((encoder_get_delta_left() + encoder_get_delta_right()) / 2.0) / 0.001;
				ctx.x_speed_error = ctx.x_speed_setpoint - ctx.x_speed_current;
				ctx.x_speed_pwm = pid_output(&ctx.x_speed_pid, ctx.x_speed_error);

				// rotation speed
				ctx.w_speed_target = 0;
				ctx.w_speed_setpoint = 0;
				ctx.w_speed_current = gyro_get_dps();
				ctx.w_speed_error = ctx.w_speed_setpoint - ctx.w_speed_current;
				ctx.w_speed_pwm = pid_output(&ctx.w_speed_pid, ctx.w_speed_error);

				motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
				motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

				if(have_to_break(0, ctx.x_speed_setpoint, DIST_STOP-encoder_get_absolute(), MAX_DECELERATION))
				{
					++ctx.sub_action_state;
				}
			}
				break;
			case 1 :
			{
				// forward speed
				ctx.x_speed_target = 0;
				ctx.x_speed_setpoint = next_speed(ctx.x_speed_target, MAX_ACCELERATION, MAX_DECELERATION, 0.001, ctx.x_speed_setpoint);
				ctx.x_speed_current = ((encoder_get_delta_left() + encoder_get_delta_right()) / 2.0) / 0.001;
				ctx.x_speed_error = ctx.x_speed_setpoint - ctx.x_speed_current;
				ctx.x_speed_pwm = pid_output(&ctx.x_speed_pid, ctx.x_speed_error);

				// rotation speed
				ctx.w_speed_target = 0;
				ctx.w_speed_setpoint = 0;
				ctx.w_speed_current = gyro_get_dps();
				ctx.w_speed_error = ctx.w_speed_setpoint - ctx.w_speed_current;
				ctx.w_speed_pwm = pid_output(&ctx.w_speed_pid, ctx.w_speed_error);

				motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
				motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

				if(ctx.x_speed_setpoint==ctx.x_speed_target)
				{
					++ctx.sub_action_state;
				}

			}
				break;
			case 2 :
			{
				// forward speed
				ctx.x_speed_target = 0;
				ctx.x_speed_setpoint = 0;
				ctx.x_speed_current = 0;
				ctx.x_speed_error = 0;
				ctx.x_speed_pwm = 0;

				// rotation speed
				ctx.w_speed_target = 0;
				ctx.w_speed_setpoint = 0;
				ctx.w_speed_current = 0;
				ctx.w_speed_error = 0;
				ctx.w_speed_pwm = 0;

				motor_speed_left(ctx.x_speed_pwm - ctx.w_speed_pwm);
				motor_speed_right(ctx.x_speed_pwm + ctx.w_speed_pwm);

				++ctx.actions_nb;
				ctx.sub_action_state = 0;

				encoder_reset();
			}
			break;
		}

	}
	break;

	case ACTION_CTR :
	{
		// nop
	}
	break;

	}
}
