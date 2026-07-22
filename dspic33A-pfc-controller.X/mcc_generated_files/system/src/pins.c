/**
 * PINS Generated Driver Source File 
 * 
 * @file      pins.c
 *            
 * @ingroup   pinsdriver
 *            
 * @brief     This is the generated driver source file for PINS driver.
 *
 * @skipline @version   PLIB Version 1.0.5
 *
 * @skipline  Device : dsPIC33AK512MPS506
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
#include <xc.h>
#include <stddef.h>
#include "../pins.h"

// Section: File specific functions
static void (*SW0_RC3_InterruptHandler)(void) = NULL;

// Section: Driver Interface Function Definitions
void PINS_Initialize(void)
{
    /****************************************************************************
     * Setting the Output Latch SFR(s)
     ***************************************************************************/
    LATA = 0x0000UL;
    LATB = 0x0000UL;
    LATC = 0x0000UL;
    LATD = 0x0000UL;

    /****************************************************************************
     * Setting the GPIO Direction SFR(s)
     ***************************************************************************/
    TRISA = 0x0FFDUL;
    TRISB = 0x0FFEUL;
    TRISC = 0x0EFFUL;
    TRISD = 0x01FAUL;


    /****************************************************************************
     * Setting the Weak Pull Up and Weak Pull Down SFR(s)
     ***************************************************************************/
    CNPUA = 0x0000UL;
    CNPUB = 0x0000UL;
    CNPUC = 0x0008UL;
    CNPUD = 0x0000UL;
    CNPDA = 0x0000UL;
    CNPDB = 0x0000UL;
    CNPDC = 0x0000UL;
    CNPDD = 0x0000UL;


    /****************************************************************************
     * Setting the Open Drain SFR(s)
     ***************************************************************************/
    ODCA = 0x0000UL;
    ODCB = 0x0000UL;
    ODCC = 0x0000UL;
    ODCD = 0x0000UL;


    /****************************************************************************
     * Setting the Analog/Digital Configuration SFR(s)
     ***************************************************************************/
    ANSELA = 0x0FFFUL;
    ANSELB = 0x0FFFUL;
    /*******************************************************************************
    * Interrupt On Change: negative
    *******************************************************************************/
    CNEN1Cbits.CNEN1C3 = 1; //Pin : RC3; 

    /****************************************************************************
     * Interrupt On Change: flag
     ***************************************************************************/
    CNFCbits.CNFC3 = 0;    //Pin : SW0_RC3

    /****************************************************************************
     * Interrupt On Change: config
     ***************************************************************************/
    CNCONCbits.CNSTYLE = 1; //Config for PORTC
    CNCONCbits.ON = 1; //Config for PORTC

    /* Initialize IOC Interrupt Handler*/
    SW0_RC3_SetInterruptHandler(&SW0_RC3_CallBack);

    /****************************************************************************
     * Interrupt On Change: Interrupt Enable
     ***************************************************************************/
    IFS3bits.CNCIF = 0; //Clear CNCI interrupt flag
    IEC3bits.CNCIE = 1; //Enable CNCI interrupt
}

void __attribute__ ((weak)) SW0_RC3_CallBack(void)
{

}

void SW0_RC3_SetInterruptHandler(void (* InterruptHandler)(void))
{ 
    IEC3bits.CNCIE = 0; //Disable CNCI interrupt
    SW0_RC3_InterruptHandler = InterruptHandler; 
    IEC3bits.CNCIE = 1; //Enable CNCI interrupt
}

/* Interrupt service function for the CNCI interrupt. */
/* cppcheck-suppress misra-c2012-8.4
*
* (Rule 8.4) REQUIRED: A compatible declaration shall be visible when an object or 
* function with external linkage is defined
*
* Reasoning: Interrupt declaration are provided by compiler and are available
* outside the driver folder
*/
void __attribute__ ( ( interrupt ) ) _CNCInterrupt (void)
{
    if(CNFCbits.CNFC3 == 1)
    {
        if(SW0_RC3_InterruptHandler != NULL) 
        { 
            SW0_RC3_InterruptHandler(); 
        }
        
        CNFCbits.CNFC3 = 0;  //Clear flag for Pin - SW0_RC3
    }
    
    // Clear the flag
    IFS3bits.CNCIF = 0;
}

