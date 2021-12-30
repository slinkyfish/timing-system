///////////////////////////////////////////////////////////////////////////
////                             LCDD.C                                ////
////                 Driver for common LCD modules                     ////
////                                                                   ////
////  lcd_init()   Must be called before any other function.           ////
////                                                                   ////
////  lcd_putc(c)  Will display c on the next position of the LCD.     ////
////                     The following have special meaning:           ////
////                      \f  Clear display                            ////
////                      \n  Go to start of second line               ////
////                      \b  Move back one position                   ////
////                                                                   ////
////  lcd_gotoxy(x,y) Set write position on LCD (upper left is 1,1)    ////
////                                                                   ////
////  lcd_getc(x,y)   Returns character at position x,y on LCD         ////
////                                                                   ////
///////////////////////////////////////////////////////////////////////////

// Delay times extended due to potential write problems

#define STATUS_READ 0

struct lcd_pin_map {  // This structure is overlayed onto the port
  int1 regsel;        // portE0
  int1 rw;            // E1
  int1 enable;        // E2
} lcd_ctrl;

#byte lcd_ctrl = 0xF84  // This puts the controlstructure onto portE
#byte lcd_data = 0xF83  // data is on portd upper

#define set_tris_lcd_CTRL(x) set_tris_e(x)
#define set_tris_lcd_DATA(x) set_tris_d(x)

#define lcd_type 2         // 0=5x7, 1=5x10, 2=2 lines
#define lcd_line_two 0x40  // LCD RAM address for the second line

// The following are used for setting
// the I/O port direction register.

struct lcd_pin_map const LCD_CTRL_INIT = {0, 0, 0};  // For read/write mode all control pins are out
#define LCD_READ 0xf6
#define LCD_WRITE 0x06

BYTE lcd_read_byte() {
  BYTE low, high;

  set_tris_lcd_DATA(LCD_READ);
  lcd_ctrl.rw = 1;
  delay_cycles(5);
  lcd_ctrl.enable = 1;
  delay_cycles(5);
  high = lcd_data & 0xf0;
  lcd_ctrl.enable = 0;
  delay_cycles(5);
  lcd_ctrl.enable = 1;
  delay_us(1);
  low = lcd_data >> 4;
  lcd_ctrl.enable = 0;
  set_tris_lcd_DATA(LCD_WRITE);
  delay_cycles(5);
  return (high | low);
}

void lcd_send_nibble(BYTE n) {
  lcd_data = (lcd_data & 0x0f) | (n << 4);  // clear high nibble,and or with n<<4
  delay_cycles(2);
  lcd_ctrl.enable = 1;
  delay_us(4);
  lcd_ctrl.enable = 0;
  delay_us(160);
}

void lcd_send_byte(BYTE address, BYTE n) {
  lcd_ctrl.regsel = 0;

#if (STATUS_READ)
  while (bit_test(lcd_read_byte(), 7))
    ;
#else
  delay_us(160);
#endif
  lcd_ctrl.regsel = address;
  delay_cycles(2);
  lcd_ctrl.rw = 0;
  delay_cycles(2);
  lcd_ctrl.enable = 0;
  lcd_send_nibble(n >> 4);
  lcd_send_nibble(n & 0xf);
}

void lcd_init() {
  set_tris_lcd_ctrl(LCD_CTRL_INIT);
  set_tris_lcd_DATA(LCD_WRITE);
  lcd_ctrl.regsel = 0;
  lcd_ctrl.rw = 0;
  lcd_ctrl.enable = 0;
  delay_ms(15);
  lcd_send_nibble(3);  // delays since can't read status flag yet
  delay_ms(5);
  lcd_send_nibble(3);
  lcd_send_nibble(3);
  lcd_send_nibble(2);
  lcd_send_byte(0, 0x28);
  lcd_send_byte(0, 0x0c);
  lcd_send_byte(0, 0x01);
  lcd_send_byte(0, 0x06);
}

void lcd_gotoxy(BYTE x, BYTE y) {
  BYTE address;

  if (y != 1)
    address = lcd_line_two;
  else
    address = 0;
  address += x - 1;
  lcd_send_byte(0, 0x80 | address);
}

void lcd_putc(char c) {
  switch (c) {
    case '\f':
      lcd_send_byte(0, 1);
      delay_ms(2);
      break;
    case '\n':
      lcd_gotoxy(1, 2);
      break;
    case '\b':
      lcd_send_byte(0, 0x10);
      break;
    default:
      lcd_send_byte(1, c);
      break;
  }
}

char lcd_getc(BYTE x, BYTE y) {
  char value;

  lcd_gotoxy(x, y);
#if (STATUS_READ)
  while (bit_test(lcd_read_byte(), 7))
    ;
#else
  delay_ms(160);
#endif
  lcd_ctrl.regsel = 1;
  value = lcd_read_byte();
  lcd_ctrl.regsel = 0;
  return (value);
}
