/*
 * debounce.h
 *
 *  Created on: Dec 28, 2016
 *      Author: rhofer
 */

#ifndef DEBOUNCE_H_
#define DEBOUNCE_H_

void deBounceInit(int16_t pin, char port, int8_t mode);
GPIO_PinState deBounceReadPin(int16_t pin, char port, int8_t stableInterval); /* u can still use the same func declaration
						in the .c file, but using "GPIO_PinState" at the start is better for readability */

#endif /* DEBOUNCE_H_ */
