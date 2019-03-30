/*
 * configuration.c
 *
 *  Created on: 27 mars 2019
 *      Author: Invite
 */


#include "configuration.h"
#include "serial.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern HAL_Serial_Handler com;

static float config[CONFIGURATION_COUNT];

static float default_config[CONFIGURATION_COUNT] =
{
	600.0, //XKP
	10.0, //XKI
	0.0, //XKD
	0.1, //WKP
	0.004, //WKI
	0.0, //WKD
	0.34, //XSPEED_LEARNING
	205, //XSPEED_LEARNING
	439.0, //WT1
	480.0, //WT2
	890.0, //WUT1
	930.0, //WUT2
	0.026, //WD
};

void configuration_init()
{
	// set configuration to factory settings
	memcpy(config,default_config,CONFIGURATION_COUNT*sizeof(float));

}

void configuration_load()
{
	// try to load configuration from flash
	// TODO
}


void configuration_save()
{
	// try to save configuration to flash
	//TODO
}

float configuration_get_ptr(size_t index)
{
	return config[index];
}

void configuration_parse_cli(char in)
{
	#define COMMAND_MAX_SIZE 256
	static char command[COMMAND_MAX_SIZE];
	static size_t position = 0;
	// 0) fill buffer
	command[position++] = in;
	// 1) end of line or full
	if( (position==COMMAND_MAX_SIZE-1) || (in=='\n') )
	{
		command[position] = 0; // null terminated string
		char * token = strtok(command," \n\r");
		if(strcmp(token,"get")==0)
		{
			HAL_Serial_Print(&com,"get ");
			for(size_t index=0;index<CONFIGURATION_COUNT;++index)
			{
				HAL_Serial_Print(&com,"%d ",(int32_t)(config[index]*1000.0)); // apply x1000 factor to see decimals
				HAL_Delay(1);
			}
			HAL_Serial_Print(&com,"\r\n");
		}
		else if(strcmp(token,"set")==0)
		{
			token = strtok(NULL," \r\n");
			size_t index = 0;
			while( token != NULL )
			{
				config[index++] = (float)(atoi(token))/1000.0;
				token = strtok(NULL," \r\n");
			}
			HAL_Serial_Print(&com,"\r\n  UDPATED to:");
			for(size_t index=0;index<CONFIGURATION_COUNT;++index)
			{
				HAL_Serial_Print(&com,"%d ",(int32_t)(config[index]*1000.0)); // apply x1000 factor to see decimals
				HAL_Delay(1);
			}
			HAL_Serial_Print(&com,"\r\n");
		}
		else if(strcmp(token,"save")==0)
		{
			configuration_save();
			HAL_Serial_Print(&com,"\r\n  SAVED to:");
			for(size_t index=0;index<CONFIGURATION_COUNT;++index)
			{
				HAL_Serial_Print(&com,"%d ",(int32_t)(config[index]*1000.0)); // apply x1000 factor to see decimals
				HAL_Delay(1);
			}
			HAL_Serial_Print(&com,"\r\n");
		}
		else if(strcmp(token,"load")==0)
		{
			configuration_load();
			HAL_Serial_Print(&com,"\r\n  LOADED to:");
			for(size_t index=0;index<CONFIGURATION_COUNT;++index)
			{
				HAL_Serial_Print(&com,"%d ",(int32_t)(config[index]*1000.0)); // apply x1000 factor to see decimals
				HAL_Delay(1);
			}
			HAL_Serial_Print(&com,"\r\n");
		}
		// reset parser
		position = 0; // reset position
		memset(command,0,COMMAND_MAX_SIZE);
	}

}


//#define CONFIGURATION_FLASH_ADDR ((uint32_t*)(0x080E0000))
//void configuration_save_to_flash()
//{

//	HAL_FLASH_Unlock();
//
//	// erasing
//	FLASH_EraseInitTypeDef erase = {
//			FLASH_TYPEERASE_SECTORS,
//			FLASH_BANK_1,
//			FLASH_SECTOR_7,
//			1,
//			FLASH_VOLTAGE_RANGE_3
//	};
//	uint32_t error = 0;
//	HAL_FLASHEx_Erase(&erase,&error);
//
//	// programming 256-bit word into flash
//	{
//		uint32_t size_256bits = sizeof(configuration_data)/32;
//		uint32_t flash_address = (uint32_t)(CONFIGURATION_FLASH_ADDR);
//		uint64_t data_address = (uint64_t)( (uint64_t const *)(&config) );
//		for(uint64_t index=0; index<size_256bits; ++index)
//		{
//			HAL_FLASH_Program( FLASH_TYPEPROGRAM_FLASHWORD, flash_address, data_address );
//			flash_address += 32;
//			data_address += 32;
//		}
//	}
//
//	HAL_FLASH_Lock();
//}
//void configuration_load_from_flash()
//{
//	// read back
//	{
//		uint32_t size_64bits = sizeof(configuration_data)/8;
//		uint64_t * ptr = (uint64_t *)(&config);
//		for(uint32_t index=0; index<size_64bits; ++index)
//		{
//			*(ptr+index) = *((uint64_t *)CONFIGURATION_FLASH_ADDR+index);
//		}
//	}
//}
