/**
 * INTERRUPT Generated Driver Source File 
 * 
 * @file      interrupt.c
 *            
 * @ingroup   interruptdriver
 *            
 * @brief     This is the generated driver source file for INTERRUPT driver          
 *
 * @skipline @version   PLIB Version 1.1.4
 *            
 * @skipline  Device : dsPIC33AK512MPS512
*/

/*
© [2026] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/

// Section: Includes
#include "../interrupt.h"

// Section: Driver Interface Function Definitions

void INTERRUPT_Initialize(void)
{
    // AD2CH0: ADC 2 data channel 0 done interrupt
    // Priority: 5
    IPC22bits.AD2CH0IP = 5;
    
    // AD3CH0: ADC 3 data channel 0 done interrupt
    // Priority: 5
    IPC25bits.AD3CH0IP = 5;
    
    // T1: Timer 1 interrupt
    // Priority: 1
    IPC6bits.T1IP = 1;
    
}

void INTERRUPT_Deinitialize(void)
{
    //POR default value of priority
    IPC22bits.AD2CH0IP = 4;
    IPC25bits.AD3CH0IP = 4;
    IPC6bits.T1IP = 4;
}
