/*
 * main.h
 *
 *  Created on: Dec 22, 2022
 *      Author: abduy
 */

#ifndef APP_MAIN_H_
#define APP_MAIN_H_

#include "../Mcal/UART/Static/inc/UART.h"
#include "../Mcal/DIO/Static/inc/DIO.h"
#include "../Mcal/PORT/Static/inc/PORT.h"
#include "../Mcal/IntCtrl/Static/inc/IntCtrl.h"
#include "../Mcal/GPT/Static/inc/GPT.h"
#include "../Mcal/SysTick/Static/inc/SysTick.h"

void App_Init(void);
void transmit(void);
void Send_Msg(char* msg);
uint8* Rcv_Msg(UART_ChannelType my_uart);


#endif /* APP_MAIN_H_ */