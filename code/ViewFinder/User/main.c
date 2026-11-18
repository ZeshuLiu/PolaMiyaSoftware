/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/05/20
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "Board/PIN.h"
#include "Communication/ControllerLink.h"
#include "Display/FinderDisplay.h"
#include "Drivers/LSM6DS3TRC/lsm6ds3trc.h"
#include "Drivers/OPT4001/opt4001.h"
#include "Drivers/SDM50/sdm50.h"
#include "Events/AppEvents.h"
#include "debug.h"

int main(void)
{
    SystemCoreClockUpdate();
    BoardPins_Init();
    Delay_Init();
    ControllerLink_Init();
    FinderDisplay_Init();
    AppEvents_Init();
    SDM50_Init();
    OPT4001_Init();
    LSM6DS3TRC_Init();

    while (1)
    {
        const uint32_t events = AppEvents_Wait();
        const uint32_t now = AppEvents_NowMs();
        LSM6DS3TRC_Service(events, now);
        OPT4001_Service(events, now);
        FinderDisplay_Service(events, now);
    }
}
