/*
 Arduino_TRS-80_M100_SRAM_Tester
 Jeffrey T. Birt (Hey Birt!) http://www.soigeneris.com , http://www.youtube.com/c/HeyBirt
 */

#include "All_Defs.h"
#include "Mega_TRS100_Defs.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h> 

// OLED Config - about 3.5 lines, 11-12 columns with font used
#define OLED_RESET     -1					// OLED Reset pin#, -1 menas none
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // 

LEDSTATES ledstate = LED_OFF;
UISTATES uistate = SPLASH;		// start out showing our splash screen

int blinkCounter = 0;			// Blink counter for green LED
int testSelected = 0;			// Index of test selected
int startAddress = 0;			// 0, 2048, 4096, 6144
int numBytes = 8192;			// 4x2K bytes
int btn = 0;					// hold button prressed
int btnLast = 0;				// last button pressed
int btnHeld = 0;				// # cycles button has been held down
const int btnValid = 10;		// debounce button delay
const int btnRepeat = 150;		// 1000 * 1ms = 1s key repeat
int reps = 1;					// # of test reps

const int maxTests = 5;			// const for test names
String TEST_NAME[maxTests] = { "All", "C1", "C2", "C3", "C4" };	// could use PROGMEM to put these in flash
const int numChips = 4;			// number of individual SRAM chips
int failures[numChips] = { 0,0,0,0 };	// keep track of #failures per SRAM chip
bool verbose = false;			// set to 'true' to enable verbose serial output

// the setup function runs once when you press reset or power the board
// Initalize IO to standby state, enable refresh timer
void setup()
{
	initStandby();	// initialize ports used for POD interface
	initUI();		// initialize port used for UI

	TCCR3B = (TCCR3B & B11111000) | 0x03; // set DRAM/LED timer to 2ms
	ENABLE_REFRESH;						  // start timer

	Serial.begin(19200);				// Only used for debugging
	Serial.println("Setup");

	// set up OLED display
	if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
	{
		Serial.println(F("SSD1306 allocation failed"));
		for (;;); // Don't proceed, loop forever
	}

	display.display();				// Display Adafruit Splash
	delay(2000);					// Pause for 2 seconds

	display.setFont(&FreeMono9pt7b);// Nice sized font
	display.setTextSize(1);			// Normal 1:1 pixel scale
	display.setTextColor(WHITE);	// Draw white text on black background
	display.cp437(true);			// Use full 256 char 'Code Page 437' font
}

//-----------------------------------------------------------------------------
// UI 
//-----------------------------------------------------------------------------

// Initialize the user interface port
void initUI()
{
	DDRF = PF_Init; // set to all inputs except LED output pin ***need series resistor in schematic
	PORTF = 0x07;	// turn off all outputs on UI port
}

// We use the loop function for the UI
void loop()
{
	delay(10);		// cycle through UI every 10ms
	btn = getBtn();	// get button pressed code

	switch (uistate)
	{
		Serial.print(uistate); // serial dump of errors

		case SPLASH:
			clearDisplay();
			display.println(F("M100 SRAM"));
			display.println(F("Tester"));
			display.display();
			delay(2000);

			uistate = BEGIN;
			break;

		case BEGIN:
			unsigned int miss;	// miss counter, incorrect bits
			ledstate = LED_OFF;

			clearDisplay();
			display.println(F("Test?"));

			// print out test list starting with a ">" beside currently selected 
			for (int i = 0; i < maxTests; i++)
			{
				if (i >= testSelected & i< testSelected+3)
				{
					i == testSelected ? display.print(F(">")) : display.print(F(" "));
					display.println(TEST_NAME[i]);
				}
			}

			display.display();
			uistate = SELECT1;
			break;

		case SELECT1:
			if (btn == 1)
			{
				uistate = TIMES;
			}
			else if (btn == 2)
			{
				testSelected++;
				if (testSelected > 4) { testSelected = 0; }
				uistate = BEGIN;
			}
			else if (btn == 4)
			{
				testSelected--;
				if (testSelected < 0) { testSelected = 4; }
				uistate = BEGIN;
			}
			break;

		case TIMES:
			clearDisplay();
			display.println(F("# of Reps?"));
			display.println(reps);
			display.display();
			uistate = SELECT2;
			break;

		case SELECT2:
			if (btn == 1)
			{
				uistate = TEST;
			}
			else if (btn == 2)
			{
				reps--;
				if (reps < 1) { reps = 1; }
				uistate = TIMES;
			}
			else if (btn == 4)
			{
				reps++;
				uistate = TIMES;
			}
			break;

		case TEST:
			clearDisplay();
			display.println("Test " + TEST_NAME[testSelected]);
			display.display();
			ledstate = LED_BLINK;

			startAddress = 0;	// default to full test
			numBytes = 8192;	// from address 0
			if (testSelected > 0) 
			{ 
				// Possible starting addresses 0, 2048, 4096, 6144
				startAddress = (testSelected - 1) * 2048; 
				numBytes = 2048;
			}
			doTests(startAddress, numBytes, reps);

			ledstate = LED_OFF;
			uistate = CONT;
			break;

		case CONT:
			if (btn > 0) 
			{ 
				clearDisplay();
				display.println(F("Again?"));
				display.println(F("Yes ___  No"));
				display.display();
				uistate = AGAIN; 
			}
			break;

		case AGAIN:
			if (btn == 1)
			{
				uistate = SPLASH;
			}
			else if (btn == 4)
			{
				// run samne test again
				uistate = TEST;
			}
			break;
	}

}

// returns button pressed, no/multi btn=0, A=1, B=2, C=4
// Clever power of 2 detection code from:
// https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2 
int getBtn()
{
	int result = 0;
	int keyIn = (PINF &0x07) ^ 0x07; // mask off bits 0-2, invert
	if ((keyIn & (keyIn - 1)) == 0) { result = keyIn; } // filter out multiple buttons

	// This is where we do button debouncing and key repeat
	if (result != btnLast) 
	{ 
		btnLast = result;
		btnHeld = 0;
		result = 0;
	}
	else if (result != 0)				// don't want to auto-repeat zero 
	{
		btnHeld++;
		if (btnHeld < btnValid)
		{
			result = 0;					// return zero until debounced
		}
		else if (btnHeld == btnValid)
		{
			result = result;			// return debounced btn once
		}
		else if ((btnHeld > btnValid) & (btnHeld < btnRepeat))
		{
			result = 0;					// has not been held enough for repeat
		}
		else if (btnHeld >= btnRepeat) 
		{
			btnHeld = btnRepeat * 0.75; // repeat & btnHeld set to 3/4 for repeat rate
		}
	}
	
	return result;
}

//-----------------------------------------------------------------------------
// Tests 
//-----------------------------------------------------------------------------

// Runs a series of checkboard and walking 1/0 tests
// maxRow, maxCol -> maximum row and column DRAM has
void doTests(int startAddress, int numBytes, int reps)
{
	bool failure = false;
	int miss = 0;

	initTest();		// initialize ports/pins to test mode
	turn5VOn();		// turn on +/- 5V power
	delay(2000);	// time for DC-DC converter to stabilize

	// zero out failure counters
	for (int i = 0; i < numChips; i++) { failures[i] = 0; }

	// test with checkerboard patterns
	failure |= runTest(reps, 0x55, startAddress, numBytes, F("Fill 0x55"));
	delay(2000);

	failure |= runTest(reps, 0xAA, startAddress, numBytes, F("Fill 0xAA"));
	delay(2000);

	// test with walking 1/0 tests
	failure |= walkTest(reps, startAddress, numBytes, 0xFF, F("Walk 0xFF"));
	delay(2000);

	failure |= walkTest(reps, startAddress, numBytes, 0x00, F("Walk 0x00"));
	delay(2000);

	// display final results, shows all C#s that have failures logged
	clearDisplay();
	failure ? display.println(F("--Failed")) : display.println(F("--Passed"));
	for (int i = 0; i < numChips; i++)
	{
		if (failures[i] > 0)
		{
			display.print("C" + String(i+1) + " ");
		}
	}

	display.display();

	turn5VOff();
	initStandby();
}

// runs an individual checkerboard type test
bool runTest(int reps, byte pattern, int startAddress, int numBytes, String lable)
{
	unsigned int miss = 0;
	bool fail;

	clearDisplay();
	display.println(lable);
	display.display();

	// run test reps# times
	for (int i = 0; i < reps; i++)
	{
		writePattern(startAddress, numBytes, pattern);
		delay(1000);
		miss += readPattern(startAddress, numBytes, pattern);
	}

	fail = (miss != 0) ? true : false;	// miss > 0 is test failure
	fail ? display.println(F("Fail ")) : display.println(F("Passed "));
	
	miss = (fail) ? miss / reps : miss;	// average misses/test
	display.println(miss, HEX);
	display.display();
	
	return fail;
}

// fill memory with 1 or 0, step through each cell check for filled value
// write opposite value, check for new value. state == fill value
bool walkTest(int reps, int startAddress, int numBytes, byte state, String lable)
{
	int miss = 0;
	bool fail = false;

	// first, fill all with state
	byte walkValue = ~state; // walking value is state inverted

	clearDisplay();
	display.println(lable);
	display.display();

	for (int r = 0; r < reps; r++)
	{
		writePattern(startAddress, numBytes, state); // fill memory first

		for (int i = startAddress; i < numBytes; i++)
		{
			//if (i == 2048) { i = 6144; } // skip from C1 to C4
			setAddress(i);
			byte readValue = readBits();

			if (readValue == state)
			{
				setAddress(i);
				setWE();
				writeBits(walkValue);
				resetWE();

				setAddress(i);
				readValue = readBits();
				if (readValue != walkValue)
				{
					miss++;
					logError(i, walkValue, readValue, "WTW:");
				}
			}
			else
			{
				miss++;
				logError(i, state, readValue, "WTF:");
			}
		}
	}

	fail = (miss != 0) ? true : false;	// miss > 0 is test failure
	fail ? display.println(F("Fail ")) : display.println(F("Passed "));
	miss = (fail) ? miss / reps : miss; // average misses/test

	display.println(miss, HEX);
	display.display();

	return fail;
}

// Fills RAM with pattern of bits
void writePattern(int startAddress, int numBytes, byte pattern)
{
	resetWE();
	int value = 0;
	for (int i = startAddress; i < startAddress + numBytes; i++) //8192
	{
		setAddress(i);
		setWE();
		writeBits(pattern);
		resetWE();
		value++;
	}

	return;
}

// verifies that pattern of bits is in RAM
unsigned int readPattern(int startAddress, int numBytes, byte pattern)
{
	unsigned int miss = 0;
	resetWE();

	int value = 0;
	for (int i = startAddress; i < startAddress + numBytes; i++)
	{
		setAddress(i);
		byte read = readBits();

		if (read != pattern) 
		{
			miss++;
			logError(i, pattern, read, "ReadPattern:");
		}

		value++;
	}

	return miss;
}

//-----------------------------------------------------------------------------
// Helpers 
//-----------------------------------------------------------------------------

// helper to clear display and set correct cursor location
void clearDisplay()
{
	display.clearDisplay();   // Clear the buffer
	display.setCursor(0, 12); // Start at top-left corner
}

// helper to log errors
void logError(int address, byte written, byte read, String lable)
{
	int index = ((address / 2048) + 0.5); // which SRAM chip?
	failures[index]++;

	// serial dump of errors if in verbose mode
	// ***NOTE: the index==1` test used to find false error when 
	// changing from C1 to C2
	if (verbose /*& index == 1*/)
	{
		Serial.print(lable + " A:" + String (address, HEX));
		Serial.print(", W:" + String(written, HEX));
		Serial.println(", R:" + String(read, HEX));
	}
}

//-----------------------------------------------------------------------------
// ISR handler 
//-----------------------------------------------------------------------------

// Handles LED flashing
// NOTE: Serial writes can interfere with this ISR timing
// was also used for DRAM refresh on orignal project
ISR(TIMER3_OVF_vect)
{
	if (ledstate == LED_BLINK)	// update Green LED
	{
		if (blinkCounter > 500)
		{
			ledToggle();
			blinkCounter = 0;
		}
		blinkCounter++;
	}
	else if (ledstate == LED_ON) // will turn LED on even if already on
	{
		ledON();
	}
	else if (ledstate == LED_OFF) // will turn LED off even if already off
	{
		ledOFF();
	}
}

