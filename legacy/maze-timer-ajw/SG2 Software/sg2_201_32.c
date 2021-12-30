// AJW 0506 : V106 Modified to include drag race
// Tried 20 MHz version to see if PLL clock is causing reset problems
// Answer - makes NO difference to reset ....
// turned off interrupts for test : NO difference to reset ....
// Not a reset problem ... it's an LCD reset problem!
// The LCD is crashing on re-initialisation, causing lock-up
// in LCD_READ function, which means initialisation can't continue
// and the whole thing stops.

//

// v 201 goes back to basics with LCD05, and sends out an init sequence in-line
// It also gives up on trying to read back status : I think the tris switching may
// have been a problem, particularly as the control lines are not pulled down.
// it seems to work fine now ....!!

#include <18F452.h>
#include <stdbool.h>
#include <stdint.h>

#fuses H4, PUT, NOPROTECT, BROWNOUT, NOLVP, NOWRT, DEBUG, NOWDT

#use delay(clock = 32000000)
#use rs232(baud = 9600, xmit = PIN_C6, rcv = PIN_C7, bits = 8)

#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)
#use fast_io(E)

#BIT T0IF = 0XFF2.2
#BIT T1IF = 0xF9E.0
#BIT T2IF = 0xF9E.1
#BIT T3IF = 0xFA1.1

#include "lcd05.c"  // LCD display library

#define G1TX PIN_C2
#define G2TX PIN_C3
#define G3TX PIN_C4
#define SGTX PIN_C5
#define G1RX PIN_A0
#define G2RX PIN_A1
#define G3RX PIN_A2
#define SGRX PIN_A3

#define VTAP PIN_A5
#define ARM PIN_D2
#define DISARM PIN_C0
#define TOUCH PIN_A4   // dual function
#define MODE PIN_A4    // dual function
#define CANCEL PIN_B0  // dual function
#define CLEAR PIN_B0   // dual function
#define START PIN_B4
#define STOP PIN_B5
#define BUZZER PIN_C1
#define ILED1 PIN_B1
#define ILED2 PIN_D3

#define TCONST1MS -996L  // 1ms ticks calibrated
#define GATE_ON 20
#define MAX_TIME 600000L

// state machine states
#define INIT_SESSION 1
#define DISARMED 2
#define INC_RUN_NO 3
#define ARMED1 4
#define ARMEDn 5
#define STARTED 6
#define UPDATING 7
#define STOPPED 8
#define CANCELLED 9
#define RUNNING1 10
#define DRAG_RUNNING 11
/*********** Function prototypes ************/

void init(void);
void read_gates(void);
void read_switches(void);
void update_display(void);
void set_display(void);
void test_gates(void);
void update_host(void);
void beep(int);
void prompt(void);
void delay_sign_on(void);

/*********** GLOBALS *************/
#define int32 long
#define int16 int16_t

static uint8_t SG2;
static uint8_t MANUAL;  // operating mode
static uint8_t GATES;
static uint8_t FG_MASK;
static uint8_t mrun_no;
static uint8_t arun_no;
static uint8_t BUTTON;
static uint8_t LOCKOUT;
static uint8_t sw_CANCEL;
static uint8_t sw_TOUCH;
static uint8_t sw_STOP;
static uint8_t sw_START;
static uint8_t sw_DISARM;
static uint8_t sw_ARM;
static uint8_t reset_display;
static uint8_t FLASH;
static uint8_t aFlgRunCancelled;
static uint8_t aSTOPPED;
static uint8_t mARMED;
static uint8_t aFlgSystemArmed;
static uint8_t TIME_UP;
static uint8_t mSTARTED;
static uint8_t aSTARTED;
static uint8_t aNEW_RUN;
static uint8_t mNEW_RUN;
static uint8_t HOST_UPDATE;

static uint32_t AMT;
static uint32_t AMT_SPLIT;
static uint32_t ART;
static uint32_t ART_SPLIT;
static uint32_t MMT;
static uint32_t MMT_SPLIT;
static uint32_t MRT;
static uint32_t MRT_SPLIT;

static uint32_t millisecs;
static uint16_t flash_timer;
static char str[4] = {'X', 'X', 'X', 'X'};

/*********** MAIN *************/

static uint8_t beep_duration;

static uint8_t beep_on;

static uint8_t astate;

static uint8_t mstate;

void manualStateMachine() {
  switch (mstate) {
    case INIT_SESSION:
      MMT = 0;
      MRT = 0;
      MMT_SPLIT = 0;
      MRT_SPLIT = 0;
      mrun_no = 1;
      reset_display = true;
      mstate = DISARMED;
      FLASH = false;
      TIME_UP = false;
      break;
    case DISARMED:
      output_low(ILED1);
      output_low(ILED2);
      if (mARMED) {
        output_high(ILED1);
      }
      break;
    case INC_RUN_NO:
      mrun_no++;
      mNEW_RUN = true;
      MRT = 0;  // reset the run timer
      MMT++;    // increment maze timer
      mstate = ARMEDn;
      break;
    case ARMED1:
      if (!mARMED) {  // first time in don't want to start timers
        mstate = INIT_SESSION;
      }
      if (sw_START) {  // if start gate triggered
        mstate = STARTED;
      }
      break;
    case ARMEDn:
      MMT++;
      if (!mARMED) {
        mstate = INIT_SESSION;
      }
      if (sw_START) {  // if start button triggered
        mstate = STARTED;
      }
      break;
    case STARTED:
      FLASH = true;
      if (sw_CANCEL) {
        mstate = CANCELLED;
      }
      if (!mARMED) {
        mstate = INIT_SESSION;
      }
      mSTARTED = true;
      MMT++;  // increment auto maze timer at 1ms intervals
      MRT++;  // increment auto run timer at 1ms intervals
      if (SG2) {
        if (sw_START) {  // if start button triggered
          MRT = 0;       // reset run timer
        }
        if (sw_STOP) {       // if stop button triggered
          mstate = STOPPED;  // next state = stopped
          MMT_SPLIT = MMT;
        }
      } else {  // DRAG GATE MODE
        if (sw_START) {
          mstate = RUNNING1;
        }
      }

      break;
    case RUNNING1:
      MRT++;
      if (!marmed) {
        mstate = INIT_SESSION;
      }
      if (sw_START) {
        MRT = 0;
      }
      if (MRT > 3000) {
        mstate = DRAG_RUNNING;
      }
      break;
    case DRAG_RUNNING:
      if (!marmed) {
        mstate = INIT_SESSION;
      }
      if (sw_START) {
        mstate = STOPPED;
      }
      MRT++;
      break;
    case STOPPED:
      FLASH = false;
      output_low(ILED2);
      mSTARTED = false;
      if (!marmed) {
        mstate = INIT_SESSION;
      }
      if (sw_START) {
        mstate = INC_RUN_NO;
      }
      MRT_SPLIT = MRT;  // for DRAG race we want RUN-TIME splits
      MMT++;            // maze timer keeps running for entire session
      break;
    case CANCELLED:
      FLASH = false;
      output_high(ILED2);
      MMT = MMT - MRT;  // reset MMT
      MRT = 0;
      mstate = ARMED1;
      break;
    default:
      break;
  }
}
void autostateMachine() {
  switch (astate) {
    case INIT_SESSION:
      AMT = 0;
      ART = 0;
      AMT_SPLIT = 0;
      ART_SPLIT = 0;
      aSTARTED = aSTOPPED = false;
      arun_no = 1;
      aNEW_RUN = true;
      astate = DISARMED;
      FLASH = false;
      TIME_UP = false;
      break;
    case DISARMED:
      if (aFlgSystemArmed) {  // synchronous state changes
        astate = ARMED1;
      }
      break;
    case INC_RUN_NO:
      arun_no++;
      aNEW_RUN = true;
      ART = 0;  // reset run timer
      AMT++;    // increment maze timer
      astate = ARMEDn;

      break;
    case ARMED1:
      if (!aFlgSystemArmed) {  // first time in don't want to start MT
        astate = INIT_SESSION;
      }
      if (GATES & 0x01) {  // if start gate triggered
        astate = STARTED;
      }
      break;
    case ARMEDn:
      AMT++;  // increment MT during armed once session started
      if (!aFlgSystemArmed) {
        astate = INIT_SESSION;
      }
      if (GATES & 0x01) {  // if start gate triggered
        astate = STARTED;
      }
      break;

    case STARTED:
      aSTARTED = true;
      if (aFlgRunCancelled) {
        astate = CANCELLED;
      }
      if (!aFlgSystemArmed) {
        astate = INIT_SESSION;
      }
      if (!aNEW_RUN) {
        astate = UPDATING;
      }
      AMT++;  // increment auto maze timer at 1ms intervals
      ART++;  // increment auto run timer at 1ms intervals

      if (GATES & 0x01) {  // if start gate triggered
        ART = 0;           // reset auto run timer if new gate trigger
      }
      break;

    case UPDATING:
      if (aFlgRunCancelled) {
        astate = CANCELLED;
      }
      if (!aFlgSystemArmed) {
        astate = INIT_SESSION;
      }
      AMT++;  // increment auto maze timer at 1ms intervals
      ART++;  // increment auto run timer at 1ms intervals

      if (SG2) {             // MAZE MODE
        if (GATES & 0x01) {  // if start gate triggered
          ART = 0;           // reset auto run timer if new gate trigger
        }

        if ((GATES & 0x0e) ^ FG_MASK) {  // if finish gates(s) triggered
          astate = STOPPED;              // next state = stopped
          AMT_SPLIT = AMT;
          aSTOPPED = true;
        }
      } else {  // DRAG MODE
        if ((GATES & 0x01) && (ART <= 3000)) {
          ART = 0;  // reset if less than 3 seconds from start
        }
        if (ART > 3000) {
          astate = DRAG_RUNNING;
        }
      }
      break;
    case DRAG_RUNNING:  // DRAG MODE
      if (!aFlgSystemArmed) {
        astate = INIT_SESSION;
      }
      if (GATES & 0x01) {
        astate = STOPPED;
        aSTOPPED = true;
      }
      ART++;
      break;
    case STOPPED:
      AMT++;
      aSTARTED = false;
      if (!aFlgSystemArmed) {
        astate = INIT_SESSION;
      }
      if (GATES & 0x01) {
        astate = INC_RUN_NO;
      }
      ART_SPLIT = ART;  // hold last run in ART_SPLIT
      break;
    case CANCELLED:
      AMT = AMT - ART;  // reset AMT
      ART = 0;
      astate = ARMED1;
      aFlgRunCancelled = false;
      FLASH = false;
      output_high(ILED2);
      break;
    default:
      break;
  }
}

/*********** INTERRUPT ROUTINES *******************/
#int_timer3
void timer3_handler(void) {
  set_timer3(TCONST1MS);
  read_switches();
  read_gates();
  autostateMachine();
  manualStateMachine();

  // General housekeeping sub-tasks
  // 100 millisecond comms update to HOST
  if (millisecs++ >= 100) {
    HOST_UPDATE = true;
    millisecs = 0;
  }

  // Max time exceeded
  if (AMT == MAX_TIME) {
    AMT = MAX_TIME;
    TIME_UP = true;
    FLASH = true;
  }

  if (MMT == MAX_TIME) {
    MMT = MAX_TIME;
    TIME_UP = true;
    FLASH = true;
  }

  if ((!TIME_UP) & (!beep_on)) {
    output_low(BUZZER);
  }

  // 250 millisecond toggle of LED and buzzer

  if (flash_timer++ > 250) {
    flash_timer -= 250;
    if (FLASH) {
      output_bit(ILED2, !input(ILED2));  // flash ILED2
      if (TIME_UP) {
        output_bit(BUZZER, !input(BUZZER));  // Modulate buzzer
      }
    }
  }

  // Millisecond resolution timer for buzzer duration
  if (beep_on) {
    if (beep_duration-- == 0) {  // downcount buzz_duration every millisecond
      output_low(BUZZER);        // and turn buzzer off when zero
      beep_on = false;
    }
  }
}

void main(void)

{
  init();
  lcd_init();
  delay_ms(50);
  T3IF = 0;
  setup_timer_3(T3_INTERNAL | T3_DIV_BY_8);  // 31.25ns*4*8 = 1us ticks
  set_timer3(TCONST1MS);                     // timeout every 1ms
  enable_interrupts(INT_TIMER3);
  enable_interrupts(GLOBAL);
  beep(50);

  printf(lcd_putc, "STARTGATE SG-2 V201_32  AJW May 2006\n");
  printf(lcd_putc, "      UK Micromouse at TIC");
  delay_sign_on();
  set_display();
  test_gates();

  // host display
  putc(0x0c);  // clear screen
  printf("STARTGATE SG2.1: AJW/0506\r\n");
  prompt();

  while (true) {
    update_display();
    update_host();
  }
}

void init(void) {
  output_a(0);
  output_b(0);
  output_c(0);
  output_d(0);
  output_e(0);

  set_tris_a(0xff);  // All input
  set_tris_b(0xfd);  // ILED1 is the only output, but MODE is an unallocated pin set as input
  set_tris_c(0x81);  //
  set_tris_d(0x06);
  set_tris_e(0xf8);  // E2-E0 output

  mstate = INIT_SESSION;
  astate = INIT_SESSION;
  GATES = 0;
  millisecs = 0;
  mNEW_RUN = false;
  aNEW_RUN = false;
  mARMED = false;
  HOST_UPDATE = false;
  aSTOPPED = false;
  aSTARTED = false;
  mSTARTED = false;

  aFlgSystemArmed = false;
  test_gates();  // need to test for auto/manual switchover
  read_gates();  // set up a mask for testing finish gates
  if (MANUAL) {
    FG_MASK = 0x0c;  // tell SG2 that SG,FG1 are OK, SG2,SG3 fail
  } else {
    FG_MASK = GATES & 0x0e;  // the mask intialises the number and order of gates connected
  }

  if (input(TOUCH)) {
    SG2 = false;  // DRAG mode
  } else {
    SG2 = true;  // STARTGATE mode
  }
  delay_ms(50);  // give time for LCD to power-up reset correctly
}

/***
 *
 *
 * For gates,  1 = CLOSED, 0 = OPEN
 *
 */

void test_gates(void) {
  int MAN;
  int i;
  int tmp;
  uint8_t STAT = 0;

  disable_interrupts(GLOBAL);
  STAT = (STAT <<= 1) | input(G1RX);  // FINISH GATE 1 ambient
  output_high(G1TX);
  delay_us(GATE_ON);
  STAT = (STAT <<= 1) | input(G1RX);  // FINISH GATE 1
  output_low(G1TX);

  STAT = (STAT <<= 1) | input(G2RX);  // FINISH GATE 2	ambient
  output_high(G2TX);
  delay_us(GATE_ON);
  STAT = (STAT <<= 1) | input(G2RX);  // FINISH GATE 2
  output_low(G2TX);

  STAT = (STAT <<= 1) | input(G3RX);  // FINISH GATE 3 ambient
  output_high(G3TX);
  delay_us(GATE_ON);
  STAT = (STAT <<= 1) | input(G3RX);  // FINISH GATE 3
  output_low(G3TX);

  STAT = (STAT <<= 1) | input(SGRX);  // START GATE 1   ambient
  output_high(SGTX);
  delay_us(GATE_ON);
  STAT = (STAT <<= 1) | input(SGRX);  // START GATE 1
  output_low(SGTX);
  enable_interrupts(GLOBAL);

  MAN = STAT;
  lcd_gotoxy(37, 1);
  for (i = 0; i < 4; i++) {
    tmp = STAT & 3;
    switch (tmp) {
      case 0x00:
      case 0x01:  // Ambient lighting too bright
        lcd_putc('B');
        str[i] = 'B';
        break;
      case 0x02:  // AOK
        lcd_putc('K');
        str[i] = 'K';
        break;
      case 0x03:  // Gate Fail
        lcd_putc('F');
        str[i] = 'F';
        break;
      default:
        lcd_putc('?');
    }
    STAT >>= 2;
  }
  if (SG2) {  // START_GAT mode
    if ((MAN & 0x0f) == 0x0A) {
      MANUAL = false;
    } else {
      MANUAL = true;
    }
  } else {                       // DRAG MODE
    if ((MAN & 0x03) == 0x02) {  // only test startgate
      MANUAL = false;
    } else {
      MANUAL = true;
    }
  }
}

void read_gates(void) {
  GATES = 0x0c;  // set for SG OK, FG1 OK, FG2,3 FAIL

  if (MANUAL) {
    if (sw_START) {
      bit_set(GATES, 0);  // two fails, FG open, SG closed
    }
    if (sw_STOP) {
      bit_set(GATES, 1);
    }
  } else {
    output_high(G1TX);
    delay_us(GATE_ON);
    GATES = (GATES <<= 1) | input(G1RX);  // FINISH GATE 1
    output_low(G1TX);
    output_high(G2TX);
    delay_us(GATE_ON);
    GATES = (GATES <<= 1) | input(G2RX);  // FINISH GATE 2
    output_low(G2TX);
    output_high(G3TX);
    delay_us(GATE_ON);
    GATES = (GATES <<= 1) | input(G3RX);  // FINISH GATE 3
    output_low(G3TX);
    output_high(SGTX);
    delay_us(GATE_ON);
    GATES = (GATES <<= 1) | input(SGRX);  // START GATE 1
    output_low(SGTX);
  }
}

void read_switches(void) {
  sw_ARM = input(ARM);
  sw_DISARM = input(DISARM);
  sw_TOUCH = input(TOUCH);
  sw_CANCEL = input(CANCEL);
  sw_START = input(START);
  sw_STOP = input(STOP);
  if (sw_ARM) {
    mARMED = true;
  }
  if (sw_DISARM) {
    mARMED = false;
  }
  BUTTON = sw_ARM || sw_DISARM || sw_TOUCH || sw_CANCEL || sw_START || sw_STOP;
  // LOCKOUT is simply to prevent continous tone when pressing key
  if (!BUTTON) {
    LOCKOUT = false;  // if no keys pressed, clear lockout
  } else {
    if (BUTTON && !LOCKOUT) {
      beep(50);  // interrupt driven beep
      LOCKOUT = true;
    }
  }
}

void set_display(void) {
  int8 i;
  if (SG2) {  // StartGate Mode
    printf(lcd_putc, "\f");
    printf(lcd_putc, "Run:%2U", 1);
    lcd_gotoxy(21, 1);
    printf(lcd_putc, "Run:%2U\n", 1);
    lcd_gotoxy(9, 1);
    printf(lcd_putc, "MT:");
    lcd_gotoxy(9, 2);
    printf(lcd_putc, "RT:");
    lcd_gotoxy(29, 1);
    printf(lcd_putc, "MT:");
    lcd_gotoxy(29, 2);
    printf(lcd_putc, "RT:");
    lcd_gotoxy(37, 1);
    for (i = 0; i < 4; i++) {
      lcd_putc(str[i]);
    }
  } else {
    printf(lcd_putc, "\f");
    printf(lcd_putc, "Run:%2U", 1);
    lcd_gotoxy(21, 1);
    printf(lcd_putc, "Run:%2U\n", 1);
    lcd_gotoxy(9, 1);
    printf(lcd_putc, "ST:");
    lcd_gotoxy(9, 2);
    printf(lcd_putc, "RT:");
    lcd_gotoxy(29, 1);
    printf(lcd_putc, "ST:");
    lcd_gotoxy(29, 2);
    printf(lcd_putc, "RT:");
    lcd_gotoxy(40, 1);
    lcd_putc(str[0]);
  }
}

void update_display(void) {
  uint32_t intgr, decml, tmp;

  if (reset_display) {
    set_display();
    reset_display = false;
  }

  if (SG2) {  // START GATE
    lcd_gotoxy(5, 1);
    printf(lcd_putc, "%02U", mrun_no);
    lcd_gotoxy(25, 1);
    if (arun_no == 0) {
      printf(lcd_putc, "%02U\n", 1);
    } else {
      printf(lcd_putc, "%02U", arun_no);
    }

    lcd_gotoxy(1, 2);
    if (mARMED) {
      printf(lcd_putc, "A");
    } else {
      printf(lcd_putc, "D");
    }
    lcd_gotoxy(21, 2);
    if (aFlgSystemArmed) {
      printf(lcd_putc, "A");
    } else {
      printf(lcd_putc, "D");
    }
    if (MANUAL) {
      printf(lcd_putc, " MSG");
    } else {
      printf(lcd_putc, " ASG");
    }

    lcd_gotoxy(12, 1);
    if (mSTARTED) {
      tmp = MMT / 1000;
      if (tmp > 999) {
        tmp = 999;
      }
      printf(lcd_putc, "%03lu", tmp);
    } else {
      tmp = MMT_SPLIT / 1000;
      if (tmp > 999) {
        tmp = 999;
      }
      printf(lcd_putc, "%03lu", tmp);
    }
    lcd_gotoxy(12, 2);
    intgr = MRT / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = MRT - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);
    lcd_gotoxy(32, 1);
    if (aSTARTED) {
      tmp = AMT / 1000;
      if (tmp > 999) {
        tmp = 999;
      }
      printf(lcd_putc, "%03lu", tmp);
    } else {
      tmp = AMT_SPLIT / 1000;
      if (tmp > 999) {
        tmp = 999;
      }
      printf(lcd_putc, "%03lu", tmp);
    }

    lcd_gotoxy(32, 2);
    intgr = ART / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = ART - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);
  } else {
    // DRAG GATE DISPLAY
    lcd_gotoxy(5, 1);
    printf(lcd_putc, "%02U", mrun_no - 1);
    lcd_gotoxy(25, 1);
    printf(lcd_putc, "%02U", arun_no - 1);
    lcd_gotoxy(1, 2);
    if (mARMED) {
      printf(lcd_putc, "A");
    } else {
      printf(lcd_putc, "D");
    }
    lcd_gotoxy(21, 2);
    if (aFlgSystemArmed) {
      printf(lcd_putc, "A");
    } else {
      printf(lcd_putc, "D");
    }
    if (MANUAL) {
      printf(lcd_putc, " MDG");
    } else {
      printf(lcd_putc, " ADG");
    }
    lcd_gotoxy(12, 1);

    intgr = MRT_SPLIT / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = MRT_SPLIT - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);

    lcd_gotoxy(12, 2);
    intgr = MRT / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = MRT - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);
    lcd_gotoxy(32, 1);

    intgr = ART_SPLIT / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = ART_SPLIT - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);

    lcd_gotoxy(32, 2);
    intgr = ART / 1000;
    if (intgr > 999) {
      intgr = 999;
    }
    decml = ART - (intgr * 1000);
    if (decml > 999) {
      decml = 999;
    }
    printf(lcd_putc, "%03lu.%03lu", intgr, decml);
  }
}

void beep(int8 duration) {
  output_high(BUZZER);       // turn buzzer on
  beep_duration = duration;  // set duration
  beep_on = true;            // start the countdown at millesecond rate in timer3 interrupt routine
}

/***
 * All the host interaction is here
 *
 * First process any commands:
 *
 * Commands from the host are single characters:
 *
 * A - Arm system
 *      resets state machine and all timing variables
 * S - Test gates
 *      return state of manual ops flag an gate states
 * C - Cancel Run
 *      sets cancelled flag
 * D - terminate session
 *      disarms system
 *
 * Then send the status
 */
void update_host(void)

{
  char cmd;

  if (kbhit()) {    // ie char waiting in serial
    cmd = getch();  // get command
    // echo to host
    putc(cmd);
    putc(0x0d);
    putc(0x0a);
    switch (cmd) {
      case 'A':  // arm gate
      case 'a':

        if (!aFlgSystemArmed) {  // lockout additional 'A' presses
          aFlgSystemArmed = true;
          printf("\r\nARMED ....");
        }
        break;

      case '?':  // read manual/auto, test optos by turning on emitters and reading receivers
      case 'S':
      case 's':
        test_gates();
        if (MANUAL) {
          putc('M');
        } else {
          putc('A');
        }
        // show gate state: K => OK, B => too bright, F => failed
        putc(str[0]);
        putc(',');
        putc(str[1]);
        putc(',');
        putc(str[2]);
        putc(',');
        putc(str[3]);
        prompt();

        break;
      case 'C':  // cancel last run
      case 'c':
        aFlgRunCancelled = true;
        prompt();
        break;
      case 'D':  // End Session : reset run_number and timers
      case 'd':
        aFlgSystemArmed = false;
        prompt();

        break;
      default:
        prompt();
        break;
    }
  }

  /// the HOST_UPDATE flag is set every 100ms by the system timer
  if (HOST_UPDATE) {
    if (aSTARTED && (!aSTOPPED)) {  // run timer started and 100ms update due
      if (aNEW_RUN) {
        printf("\r\nS\r\n");
        aNEW_RUN = false;
      } else {
        printf("\rU %lu", ART);
      }
    }

    if (aSTOPPED) {
      printf("\r\nP %lu\r\n", ART_SPLIT);
      aSTOPPED = false;
      prompt();
    }
    HOST_UPDATE = false;
  }
}

void prompt(void) {
  putc(0x0d);  // characters faster than printf()
  putc(0x0a);
  putc('>');
  putc(' ');
  // printf("\r\n> ";
}

void delay_sign_on(void) {
  unsigned long int i;
  for (i = 0; i < 100; i++) {
    delay_ms(10);
  }
}
