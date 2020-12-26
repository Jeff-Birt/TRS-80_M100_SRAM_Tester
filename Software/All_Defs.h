/*
 Arduino_TRS-80_M100_SRAM_Tester
 Jeffrey T. Birt (Hey Birt!) http://www.soigeneris.com , http://www.youtube.com/c/HeyBirt

 All_Defs.h - This file contains defintions etc. that are 'universal',
              i.e. they deal with the UI, shared functions, etc.
*/

#pragma once

// used to add NOP delay
#define NOP __asm__ __volatile__ ("nop\n\t")
#define DISABLE_REFRESH TIMSK3 = (TIMSK3 & B11111110) // disable refresh timer
#define ENABLE_REFRESH  TIMSK3 = (TIMSK3 & B11111110) | 0x01 // enable refresh timer

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Will be configured by IIC library
// PORTD bits         Ard Prt Pin
//	SCL					  PD0 #21
//	SDA				      PD1 #20
//					      PD2 
//					      PD3 
//					      PD4
//					      PD5  
//                        PD6
//				          PD7

// Used for user interface
// PORTF bits			   Ard Prt Pin
#define KEYPAD0	0x01	// PF0 #A0
#define KEYPAD1	0x02	// PF1 #A1
#define KEYPAD2 0x04	// PF2 #A2
//	    				   PF3 #A3
//					       PF4 #A4
//					       PF5 #A5
//                         PF6 #A6
#define LED		0x80	// PF7 #A7
// DDRF Definitions
#define PF_Init 0x80


// states the UI can be in
enum UISTATES
{
	SPLASH, // splash screen
	BEGIN,  // displays available tests
	SELECT,	// allows navigating/selecting test
	TIMES,	// lets user select # test reps
	TEST,	// starts test running
	CONT,	// pause to show test results
	AGAIN	// allows user to rerun same test
};

// states LED can be in
enum LEDSTATES
{
	LED_ON,
	LED_BLINK,
	LED_OFF
};

// DRAM refresh state, Timer originally used for DRAM refresh
// now used just for LED blinking
enum REFRESH_STATES
{
	REF_ON,
	REF_OFF
};

void clearDisplay(); // helper to clear display set cursor position
void doTests(int startAddress, int numBytes, int reps); // do set of tests
int getBtn(); // returns analog button pressed
void logError(int address, byte written, byte read, String lable); // helper to log erros
unsigned int readPattern(int startAddress, int numBytes, byte pattern);
bool runTest(int reps, byte pattern, int startAddress, int numBytes, String lable);
bool walkTest(int reps, int startAddress, int numBytes, byte state, String lable); // do march type test
void writePattern(int startAddress, int numBytes, byte pattern);

/// Some inline helper functions for controlling LED

// Turns LED OFF
inline void ledOFF()
{
	PORTF = PORTF & ~LED; // turn off grn LED
}

// Turns LED ON
inline void ledON()
{
	PORTF = PORTF | LED; // turn off grn LED
}

// Toggles LED state
inline void ledToggle()
{
	PORTF = PORTF ^ LED; // toggle grn LED
}
