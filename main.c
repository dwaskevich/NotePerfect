﻿/******************************************************************************
* File Name: Main.c
*
* Version 1.1
*
* Description:
* This file contains the main function for the voltage Display test.
*
* Note:
*
* Code tested with:
* PSoC Creator: 3.0
* Device Tested With: CY8C5868AXI-LP035
* Compiler    : ARMGCC 4.4.1, ARM RVDS Generic, ARM MDK Generic
*
********************************************************************************
* Copyright (2013), Cypress Semiconductor Corporation. All Rights Reserved.
********************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress)
* and is protected by and subject to worldwide patent protection (United
* States and foreign), United States copyright laws and international treaty
* provisions. Cypress hereby grants to licensee a personal, non-exclusive,
* non-transferable license to copy, use, modify, create derivative works of,
* and compile the Cypress Source Code and derivative works for the sole
* purpose of creating custom software in support of licensee product to be
* used only in conjunction with a Cypress integrated circuit as specified in
* the applicable agreement. Any reproduction, modification, translation,
* compilation, or representation of this software except as specified above 
* is prohibited without the express written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH 
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the 
* materials described herein. Cypress does not assume any liability arising out 
* of the application or use of any product or circuit described herein. Cypress 
* does not authorize its products for use as critical components in life-support 
* systems where a malfunction or failure may reasonably be expected to result in 
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of 
* such use and in doing so indemnifies Cypress against all charges. 
*
* Use of this Software may be limited by and subject to the applicable Cypress
* software license agreement. 
*******************************************************************************/

/******************************************************************************
*                           THEORY OF OPERATION
* This project demonstrates how ADC is used to read the input voltage at 
* it's input and display it on the LCD.
* 
* The Potentiometer is connected to the input of the DelSig ADC. ADC is 
* configured with 20 bit of resolution to measure the input voltage with 
* higher accuracy. Moving average filter of 128 samples is applied to the ADC
* conversion result before displaying the result in micro volts on the LCD. 
*
* Hardware connection on the Kit
* Potentiometer - PORT 6[5] 
* LCD - PORT 2[0..6]
*******************************************************************************/
#include <device.h>
#include "stdio.h"
#include "stdlib.h"

#define POT (0u)
#define CV  (1u)

/* Number of samples to be taken before averaging the ADC value */
#define MAX_SAMPLE                  ((uint8)128)

/* Threshold value to reset the filter for sharp change in signal */
#define SIGNAL_SLOPE                1000

/* Number of shifts for calculating the sum and average of MAX_SAMPLE */
#define DIV                         7

/* Number of "NotePerfect" control voltages per volt */
#define NUMBER_NOTES_PER_VOLT       (12u)

/* Maximum control voltage */
#define MAX_CONTROL_VOLTAGE         (5u)

/* Total number of "NotePerfect" quantized steps/values (60) */
#define NUMBER_NOTE_PERFECT_STEPS (NUMBER_NOTES_PER_VOLT * MAX_CONTROL_VOLTAGE)

/* NotePerfect step size (in mV) */
#define NOTEPERFECT_STEP_SIZE_MV    ((MAX_CONTROL_VOLTAGE * 1000) / NUMBER_NOTE_PERFECT_STEPS)

/* NotePerfect step number lookup table */
uint16_t PWM_Lookup[] = {0, 83, 167, 250, 333, 417, 500, 583, 667, 750, 833, 917, 1000,
                            1083, 1167, 1250, 1333, 1417, 1500, 1583, 1667, 1750, 1833, 1917, 2000,
                            2083, 2167, 2250, 2333, 2417, 2500, 2583, 2667, 2750, 2833, 2917, 3000,
                            3083, 3167, 3250, 3333, 3417, 3500, 3583, 3667, 3750, 3833, 3917, 4000,
                            4083, 4167, 4250, 4333, 4417, 4500, 4583, 4667, 4750, 4833, 4917, 5000};

int main(void)
{
    uint8 i;
    
    /* Array to store ADC count for moving average filter */
    int32 adcCounts[MAX_SAMPLE] = {0};
    
    /* Variable to hold ADC conversion result */
    int32 result = 0;
    
    /* Variable to store accumulated sample for filter array */
    int32 sum = 0;
    
    /* Variable for testing sharp change in signal slope */
    int16 diff = 0;
    
    /* Variable to hold the result in milli volts converted from filtered 
     * ADC counts */
    int32 milliVolts = 0;
	
    /* Variable to hold the moving average filtered value */
    int32 averageCounts = 0;
	
    /* Index variable to work on the filter array */
    uint8 index = 0;
    
    /* Variables to calculate and hold NotePerfect DAC value */
    uint32 notePerfectValue = 0;
    uint32 previousNotePerfectValue = 0;
    uint32 remainder = 0;
    
    /* Character array to hold the micro volts*/
    char displayStr[15] = {'\0'};
    
    /* variable/flag to hold status of input selection (Pot or CV) */
    uint8_t toggleFlag = 0;

    CYGlobalIntEnable;

    /* Start ADC and start conversion */
    ADC_Start();
    ADC_StartConvert();

    /* Start LCD and set position */
    LCD_Start();
    LCD_Position(0,0);
    LCD_PrintString("Pot      Step=");

    /* Print mV unit on the LCD */
    LCD_Position(1,4);
    LCD_PutChar('m');
    LCD_PutChar('V');
    LCD_Position(1,8);
    LCD_PrintString("DAC=");
    
    /* Read one sample from the ADC and initialize the filter */
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
    result = ADC_GetResult32();
    
    for(i = 0; i < MAX_SAMPLE; i++)
    {
        adcCounts[i] = result;
    }
    
    /* Store sum of 128 samples*/
    sum = result << DIV;
    
    /* Average count is equal to one single sample for first ADC reading */
    averageCounts = result;
    
    /* start Opamp and PWM */
    Opamp_Start();
    PWM_Start();
    
    /* initialize indicator LEDs */
    LED3_Write(toggleFlag);
    LED4_Write(~toggleFlag);
    
    /* start and initialize Analog input Mux to POT as the source */
    myMux_Start();
    myMux_FastSelect(POT);
    
    while(1)
    {
        /* user interface stuff ... switch between input sources (on-board Pot vs CV input) */
        if(SW2_Read() == 0) /* check user switch */
        {
            toggleFlag ^= 1; /* toggle flag if switch is pushed and update LEDs */
            LED3_Write(toggleFlag);
            LED4_Write(~toggleFlag);
            
            if(POT == toggleFlag) /* housekeeping for Pot selection */
            {
                LCD_Position(0,0);
                LCD_PrintString("Pot");
                myMux_FastSelect(POT); /* select the onboard Potentiometer as input source */
            }
            if(CV == toggleFlag) /* housekeeping for CV selection */
            {
                LCD_Position(0,0);
                LCD_PrintString("CV ");
                myMux_FastSelect(CV); /* select CV as input source */
            }
            while(0 == SW2_Read()) /* lazy man's debounce ... just wait here until switch is released */
            ;
            CyDelay(200); /* delay for some arbitrarily long debounce time */
        }
        
        /* start the ADC conversion and wait for the result */
        ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
        result = ADC_GetResult32();
        
        diff = abs(averageCounts - result); /* calculate instantaneous difference to determine if abrupt change has happened */

        /* If sharp change in the signal then reset the filter with the new signal value */
        if(diff > SIGNAL_SLOPE)
        {
            /* fill the filter array with current value */
            for(i = 0; i < MAX_SAMPLE; i++)
            {
                adcCounts[i] = result;
            }
            
            /* Store sum of 128 samples*/
            sum = result << DIV;
    
            /* Average count is equal to new sample */
            averageCounts = result;
            index = 0;
        }
        
        /* Get moving average */
        else
        {
            /* Remove the oldest element and add new sample to sum and get the average */
            sum = sum - adcCounts[index];
            sum = sum + result;
            averageCounts = sum >> DIV;
            
            /* Remove the oldest sample and store new sample */
            adcCounts[index] = result;
            index++;
            if (index == MAX_SAMPLE)
            {
                index = 0;
            }
        }
        
        /* convert ADC counts to milliVolts */
        milliVolts = ADC_CountsTo_mVolts(averageCounts);
        
        /* NotePerfect magic happens here */
        notePerfectValue = (milliVolts / NOTEPERFECT_STEP_SIZE_MV); /* calculate integer step number */
        remainder = milliVolts % NOTEPERFECT_STEP_SIZE_MV; /* use modulo to get remainder */
        if(remainder >= NOTEPERFECT_STEP_SIZE_MV/2) /* determine if round-up is necessary */
            notePerfectValue += 1;
        
        if(notePerfectValue != previousNotePerfectValue) /* only spend time if notePerfect step value has changed */
        {
            PWM_WriteCompare(PWM_Lookup[notePerfectValue]); /* lookup PWM compare value and update PWM */
            
            /* display housekeeping */
            sprintf(displayStr,"%4d", PWM_Lookup[notePerfectValue]);
            LCD_Position(1,12);
            LCD_PrintString(displayStr);   
            
            /* Convert notePerfectValue to string and display on the LCD */
            sprintf(displayStr,"%2ld", notePerfectValue);
            LCD_Position(0,14);
            LCD_PrintString(displayStr);
            
            previousNotePerfectValue = notePerfectValue;
        }
            
        /* Convert milli volts to string and display on the LCD */
        sprintf(displayStr,"%4ld",milliVolts);
        LCD_Position(1,0);
        LCD_PrintString(displayStr);
    }	
}

/* [] END OF FILE */

