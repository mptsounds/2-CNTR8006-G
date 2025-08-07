/**
  ******************************************************************************
  * @file           : userInput.h

  * @brief          : header file for user input system used in menus
  * @author         : Allan Smith (Conestoga College)
  * @date           : 4-06-2024

  ******************************************************************************
  */

#ifndef INC_USERINPUT_H_
#define INC_USERINPUT_H_

#include "stm32f4xx_hal.h"

#define READ_TIMEOUT_SHORT  10  //short amount of time to wait for data on UART Rx
#define READ_TIMEOUT_LONG  5000 //long amount of time to wait for data on UART Rx

#define LENGTH_OF_INPUT_ARRAY	100	// the length of the rx buffer used when getting input from UART

char GetCharFromUART2( void ); // VCP


#endif /* INC_USERINPUT_H_ */
