#define TRUE 1
#define FALSE 0

#define dp LATDbits.LATD7
#define dig0 0xfe
#define dig1 0xfd
#define dig2 0xfb
#define dig3 0xf7
#define dig4 0xef
#define dig5 0xdf

#define digdata LATD
#define digsel LATB

//               Pgfedcba
#define zero   0b00111111
#define one    0b00000110
#define two    0b01011011
#define three  0b01001111
#define four   0b01100110
#define five   0b01101101
#define six    0b01111101
#define seven  0b00000111
#define eight  0b01111111
#define nine   0b01101111
#define blank  0b00000000



//#define T1CONST -7999  // gives 1.03ms at 32MHz
#define T1CONST -7767  // gives 1.000ms at 32MHz

#define SGTX LATCbits.LATC2
#define FGTX LATCbits.LATC1
#define BUZZ1 LATCbits.LATC0
#define BUZZ2 LATCbits.LATC3
#define RED   LATCbits.LATC4
#define GREEN LATCbits.LATC5
#define YELL1 LATEbits.LATE0
#define YELL2 LATEbits.LATE1
#define YELL3 LATEbits.LATE2

#define ARM PORTAbits.RA4
#define SGRX CMCONbits.C1OUT
#define FGRX CMCONbits.C2OUT


