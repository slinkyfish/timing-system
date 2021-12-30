// AJW 28/06/07 Gantry timer
// 19/6/07 Added comms at 115200
//
// 19/06/07 added startgate: comparators used.


#include <p18f4520.h>
#include "gantry.h"
#include <timers.h>
#include <usart.h>

#pragma config OSC=HSPLL,PWRT=ON,WDT=OFF,BOREN=OFF,MCLRE=ON,PBADEN=OFF
#pragma config CCP2MX=PORTC,STVREN=ON,LVP=OFF,XINST=OFF,DEBUG=ON

typedef struct {
  unsigned char val0;
  unsigned char val1;
  unsigned char val2;
  unsigned char val3;
  unsigned char val4;
  unsigned char val5;
}  display_buffer ;

static display_buffer dbuf;
static unsigned char TIMER = FALSE, state = 0, SGTRIG = FALSE, FGTRIG = FALSE, FALSE_START = FALSE;
const char seg7[] = {zero, one, two, three, four, five, six, seven, eight, nine, blank};

static unsigned int secs = 0, millisecs = 0, timeout = 0, beeptimer = 0;

/********************* INTERRUPT initialisation ****************************/
void ISR_HI(void);
void ISR_LO(void);
void init_interrupts(void);
void init_cmp(void);
void refresh_display(void);
void update_time(void);
void init_ports(void);
void fsm(void);
void zero_time(void);
void beep(unsigned int);
/*-----------------------------------------------------------------------*/
/* This bit is to initialise the interrupt vectors
/*-----------------------------------------------------------------------*/
#pragma code high_vector_section=0x8  // goto absolute code section
void high_vector(void) {
  _asm GOTO ISR_HI _endasm
}

#pragma code low_vector_section=0x18  // goto absolute code section
void low_vector(void) {
  _asm GOTO ISR_LO _endasm
}

#pragma code   // return to normal code section

/*********************   INTERRUPT routines    ****************************/

/*-----------------------------------------------------------------------*/
/* High level interrupt routine  */
/*-----------------------------------------------------------------------*/
#pragma interrupt ISR_HI
void ISR_HI(void) {
  static unsigned char lastCOUT1 = 0, lastCOUT2 = 0, currentCOUT1 = 0, currentCOUT2 = 0, intcount = 0, dspcount = 0;

  if (PIR1bits.TMR1IF & PIE1bits.TMR1IE) {  // timer 1 is master 1ms timer
    PIR1bits.TMR1IF = 0;
    WriteTimer1(T1CONST);

    SGTX = FGTX = 1; //irleds on
    // use clock/display update as the IRLED setup time
    update_time();
    if (TIMER) {
      if (millisecs++ >= 999) {
        millisecs = 0;
        if (secs++ >= 999) {
          secs = 0;
        }
      }
    }
    if (dspcount++ > 1) {
      dspcount = 0;
      refresh_display();
    }
    // Read Start Gate comparator and set/clear flag
    if (SGRX) {
      SGTRIG = TRUE;  // SG = Start Gate
    } else {
      SGTRIG = FALSE;
    }
    // Read Finish Gate comparator and set/clear flag
    if (FGRX) {
      FGTRIG = TRUE;  // FG = Finish Gate
    } else {
      FGTRIG = FALSE;
    }
    SGTX = FGTX = 0; 	//irleds off
    if (timeout > 0) {
      timeout --;  // countdown
    }
    fsm();				// do state machine
    if (beeptimer > 0) {
      beeptimer --;
      BUZZ1 = TRUE;
    } else {
      BUZZ1 = FALSE;
    }
  }
}
/*-----------------------------------------------------------------------*/
/* low priority interrupt routine  */
/*-----------------------------------------------------------------------*/
#pragma interruptlow ISR_LO
void
ISR_LO(void) {

}

/*********************      MAIN     ****************************/




void main(void) {


  unsigned  int i = 0;

  init_ports();
  init_interrupts();
  init_cmp();
  state = 0;
  WriteTimer1(T1CONST);
  OpenTimer1(TIMER_INT_ON &
             T1_16BIT_RW &
             T1_SOURCE_INT &
             T1_PS_1_1 &
             T1_OSC1EN_OFF &
             T1_SYNC_EXT_OFF);

  OpenUSART(USART_TX_INT_OFF  &
            USART_RX_INT_OFF  &
            USART_ASYNCH_MODE &
            USART_EIGHT_BIT   &
            USART_CONT_RX &
            USART_BRGH_HIGH, 51); //38400 from 32MHz

  while (1) {

  }
}

void init_interrupts(void) {
  /* Enable interrupt priority */
  RCONbits.IPEN = 1;

  /* SET timer1 INTERRUPT PRIORITY and enable*/
  IPR1bits.TMR1IP = 1;
  PIE1bits.TMR1IE = 1;
  INTCONbits.GIEH = 1;
  INTCONbits.GIEL = 1;
}

void init_cmp(void) {
  ADCON1 = 10;
  CMCON = 0x06; // only mode with common CVREF
  CVRCON = 12; // SET TO 2.5V
  CVRCONbits.CVREN = 1;  // comparator on
  CVRCONbits.CVROE = 0;  //
  CVRCONbits.CVRR = 1;
  CVRCONbits.CVRSS = 0;
}

void refresh_display(void) {
  static unsigned char dignum = 0;

  switch (dignum) {
    case 0:
      digsel = dig0;
      dignum ++;
      digdata = seg7[dbuf.val0];
      dp = 0;
      break;
    case 1:
      digsel = dig1;
      dignum ++;
      digdata = seg7[dbuf.val1];
      dp = 0;
      break;
    case 2:
      digsel = dig2;
      dignum ++;
      digdata = seg7[dbuf.val2];
      dp = 0;
      break;
    case 3:
      digsel = dig3;
      dignum ++;
      digdata = seg7[dbuf.val3];
      dp = 1;
      break;
    case 4:
      digsel = dig4;
      dignum ++;
      digdata = seg7[dbuf.val4];
      dp = 0;
      break;
    case 5:
      digsel = dig5;
      dignum = 0;
      //digdata = seg7[dbuf.val5];
      if (FALSE_START) {
        digdata = 0x71;  // F for false
      } else {
        digdata = blank;
      }
      dp = 0;

      break;
  }
}

void update_time(void) {
  dbuf.val0 = millisecs % 10;
  dbuf.val1 = (millisecs / 10) % 10;
  dbuf.val2 = millisecs / 100;
  dbuf.val3 = secs % 10;
  dbuf.val4 = (secs / 10) % 10;
  dbuf.val5 = secs / 100;
}

void zero_time(void) {
  millisecs = 0;
  secs = 0;
}

void init_ports(void) {
  TRISB = 0xc0; 			// bottom 5 outputs  for digit select
  LATB = 0;				// all segments off
  TRISD = 0; 			// all outputs  for segment select i.e. data
  LATD = 0x3f;			// all digits off
  TRISA = 0xff;          // all inputs

  TRISCbits.TRISC3 = 0;	// BUZZ2
  BUZZ2 = 0;
  TRISCbits.TRISC2 = 0;	// SGTX
  SGTX = 0;
  TRISCbits.TRISC1 = 0;	// FGTX
  FGTX = 0;
  TRISCbits.TRISC0 = 0;	// BUZZ1
  BUZZ1 = 0;
  TRISCbits.TRISC5 = 0;	// GREEN led
  GREEN = 0;
  TRISCbits.TRISC4 = 0;	// RED led
  RED = 0;
  TRISEbits.TRISE0 = 0;	// YELL1
  YELL1 = 0;
  TRISEbits.TRISE1 = 0;	// YELL2
  YELL1 = 0;
  TRISEbits.TRISE2 = 0;	// YELL3
  YELL1 = 0;
}

void fsm(void) {
  switch (state) {
    case 0:
      YELL3 = YELL2 = YELL1 = RED = GREEN = FALSE;

      if (SGTRIG) {
        RED = TRUE;
      } else {
        RED = FALSE;
      }
      if (ARM == 0) {
        state = 8;
        timeout = 1000;  //1 second timeout
        zero_time();
        FALSE_START = FALSE;
        beep(250);
      }
      break;
    case 8:
      if (SGTRIG) {
        RED = TRUE;
      } else {
        RED = FALSE;
      }

      if ((ARM == 0) && (timeout == 0)) {
        state = 1;
        timeout = 3000;  //1 second timeout
        beep(1000);
      }
      break;


    case 1:
      if (SGTRIG) {
        RED = TRUE;   // RED doesn't clear - FALSE START
        FALSE_START = TRUE;
      }
      if (timeout == 0) {
        YELL3 = YELL2 = YELL1 = TRUE;
        timeout = 1000;
        state = 2;
      }
      break;
    case 2:
      if (SGTRIG) {
        RED = TRUE;   // RED doesn't clear - FALSE START
        FALSE_START = TRUE;
      }
      if (timeout == 0) {
        state = 3;
        timeout = 1000;  //  1 second timeout
        YELL1 = FALSE;
      }
      break;
    case 3:
      if (SGTRIG) {
        RED = TRUE;   // RED doesn't clear - FALSE START
        FALSE_START = TRUE;
      }
      if (timeout == 0) {
        state = 4;
        timeout = 1000;  // 1 second timeout
        YELL2 = FALSE;
      }
      break;
    case 4:
      if (SGTRIG) {
        RED = TRUE;   // RED doesn't clear - FALSE START
        FALSE_START = TRUE;
      }
      if (timeout == 0) {
        state = 5;
        YELL3 = FALSE;
      }
      break;
    case 5:
      if (SGTRIG) {
        RED = TRUE;   // RED doesn't clear - FALSE START
        FALSE_START = TRUE;
      }
      if (timeout == 0) {
        state = 6;
        GREEN = TRUE;
        TIMER = TRUE;
      }
      break;
    case 6:
      if (FGTRIG) {
        TIMER = FALSE;
        state = 7;
      }
      break;
    case 7:
      if (ARM == 0) {
        state = 0;
      }
      break;
  }

}

void beep(unsigned int time) {
  beeptimer = time;
}