/*
 Arduino_TRS-80_M100_SRAM_Tester
 Jeffrey T. Birt (Hey Birt!) http://www.soigeneris.com , http://www.youtube.com/c/HeyBirt

 Mega_TRS100_Defs.h - This file contains defintions unique to testing 
			          TRS-80 M100 RAM modules
*/

#pragma once

//-----------------------------------------------------------------------------
// #defines to describe use of each port/pin
// as well as the state the pins should be in
// Px_Stndby - Standby state, when test is not running most pins at HiZ
// Px_Init   - Intial pin state when test running
// Px_Test   - Inital DDRx State when test running 

//PORTA bits		   Act	Prt Pin                
#define A0 0x01		// H	PA0 #22 
#define A1 0x02		// H	PA1 #23
#define A2 0x04		// H	PA2 #24
#define A3 0x08		// H	PA3 #25
#define A4 0x10		// H	PA4 #26
#define A5 0x20		// H	PA5 #27
#define A6 0x40		// H	PA6 #28
#define A7 0x80		// H	PA7 #29
// DDRA Definitions
#define PA_Stndby		0x00 // all inputs, hiz
#define PA_TRS100_Init	0x00 // Initial state out outputs
#define PA_TRS100_Test	0xFF // all outputs


// PORTC bits		   Act	Prt	Pin
#define A8  0x01	// H	PC0 #37
#define A9  0x02	// H	PC1 #36
#define A10 0x04	// H	PC2 #35
#define CE1 0x08	// L	PC3 #34
#define CE2 0x10	// L	PC4 #33
#define CE3 0x20	// L	PC5 #32  
#define CE4 0x40	// L	PC6 #31
#define CEX 0x80	// L	PC7 #30
// DDRC Definitions
#define PC_Stndby		0x00 // all inputs, hiz
#define PC_TRS100_Init	0xF8 // CEx hi, CE1-4 hi
#define PC_TRS100_Test	0xFF // all outputs


// PORTH bits			  Act	Prt Pin
#define WE		  0x01 // L		PH0 17
//								PH1 11
//								PH2 
//								PH3 
//								PH4
#define	DUT_PWR5  0x20 // L		PH5 8
#define DUT_PWR12 0x40 // L		PH6 9 
//								PH7
// DDRH Definitions
#define PH_Stndby		0x60 // outputs, bits set
#define PH_TRS100_Test	0x61 // Initial state out outputs
//#define PH_TRS100_Test  0x41 // 5V on


// PORTL bits		  Act	Prt Pin
#define D0	0x01	// H	PL0 #49
#define D1	0x02	// H	PL1 #48
#define D2	0x04	// H	PL2 #47
#define D3	0x08	// H	PL3 #46
#define D4	0x10	// H	PL4 #45
#define	D5	0x20	// H	PL5 #44
#define D6  0x40	// H	PL6 #43
#define D7	0x80	// H	PL7 #42
// DDRL Definitions
#define PL_Stndby			0x00 // all inputs, hiz
#define PL_TRS100_TestRd	0x00 // all inputs
#define PL_TRS100_TestWr	0xFF // all outputs

// There could be multiple test modes but for now we only have 1
enum TestMode
{
	SRAM_TRS100,
};
TestMode mode = SRAM_TRS100;

// ----------------------------------------------------------------------------
// Yes, having lots of code in a '.h' is ugly but...
// The idea here is to abstract the functions that do the actual bit twiddling.
// This makes it easier to re-implelent on antoher platform

// Set state of DDRx and pins during standby, when test not running
//  For HIZ set DDRx bits to zero and set corresponding PORTx pins low
//  set DDRx bit to a 1 to set pin to output *** set all outputs low/off
void initStandby()
{
	DDRA = PA_Stndby;	// Address lines 0-7
	PORTA = PA_Stndby;	// Set pins low for hiz
	DDRC = PC_Stndby;   // Address lines 8-10, CE lines
	PORTC = PC_Stndby;	// Set pins low for hiz
	PORTH = PH_Stndby;	// Set DC-DC converter controls off (HI).
	DDRH = PH_Stndby;	// DUT Power
	DDRL = PL_Stndby;	// Data lines 1-8
	PORTL = PL_Stndby;	// Set pins low for hiz
}

// Set state of DDRx and pins when test is starting
// We only have 1 test mode in this case but there could be more
//  Set pins output buffer to correct state before changing port to output
void initTest()
{
	switch(mode)
	{
		case SRAM_TRS100:
			PORTA = PA_TRS100_Init;		// Address lines 0-7
			DDRA = PA_TRS100_Test;		// Set port/pins to output
			PORTC = PC_TRS100_Init;		// Address lines 8-10, CE lines
			DDRC = PC_TRS100_Test;		// Set port/pins to output
			DDRH = PH_TRS100_Test;		// DUT Power, turns on +5V
			PORTL = PL_TRS100_TestRd;	// Data lines 1-8
			DDRL = PL_TRS100_TestRd;	// DRAM address, control, I/O
			break;

		//default:
			// do nothing
	}
}

// Set the address lines, whcih are split between two ports A and C
// PortA pins 0-7 are A0-A7, PortC pins 0-2 are A8-A10. We decode
// A11 and A12 with a virtual 2-to-4 demux to select the correct SRAM chip on
// the module. PortC pins 3-6 are CE#1-CE#4, individual CE based on address.
// PortC pin 7 is CEx, global chip enable for all 4 chips. This is controlled
// by the 'readBits' and 'writeBits' functions. 
void setAddress(int address)
{
	PORTA = (byte)(address & 0xFF); // bits 0-7
	int cePins = (address & 0x1800); //mask off virtual A11 and A12
	cePins = cePins >> 11; // shift A11/A12 to LSB 
	cePins = 0x01 << cePins; // shift 0x01 << cePins # to select correct RAM
	cePins = cePins << 3; // shift left 3x more to align with CE pins
	cePins = cePins ^ 0x78; // invert state for active low

	//CE pins are active low, need to invert the pin state
	int PCNew = ((address & 0x700) >> 8); // mask off A8-A10 shift over
	PCNew = PCNew | cePins; // select correct RAM chip
	PCNew = PCNew;// &(CEX ^ 0xFF);
	PORTC = (byte)PCNew; // update actual port
}

// Read from data bus (PortL, Pins 0-7)
// Set PortC Pin7 low for global enable, read PortL
// Raise PortC Pin7 to high, return data
inline byte readBits()
{
	PORTC = PINC & ~CEX; // lower global CE
	NOP;				 // may not need the NOP
	byte value =  PINL;	 // read dta bus
	PORTC = PINC | CEX;  // raise global CE
	return value;
}

// Write to data bus (PortL, Pins 0-7)
// Set PortC Pin7 low for global enable, set PortL
// Raise PortC Pin7 to high
inline void writeBits(byte state)
{
	PORTC = PINC & ~CEX; // lower global CE
	NOP;				 // may not need NOP
	PORTL = state;		 // write to data bus
	PORTC = PINC | CEX;  // raise global CE
}

// Set /WE low for to enable a write
inline void setWE()
{
	DDRL = PL_TRS100_TestWr; // data bus to write mode
	PORTH = PORTH & ~WE;  
}

// Set /WE high to complete write
inline void resetWE()
{
  PORTH = PORTH | WE;
  DDRL = PL_TRS100_TestRd; //databus to read mode
}

// Turn on the +/- 5V DC-DC converter
inline void turn5VOn()
{
	PORTH = PINH & (DUT_PWR5 ^ 0xFF);
}

// Turn off the +/- 5V DC-DC converter
inline void turn5VOff()
{
	PORTH = PINH | DUT_PWR5;
}




