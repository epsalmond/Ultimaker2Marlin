#ifndef ULTI_LCD2_MENU_TINKERGNOME_H
#define ULTI_LCD2_MENU_TINKERGNOME_H

#define EEPROM_UI_MODE_OFFSET 0x401
#define EEPROM_LED_TIMEOUT_OFFSET 0x402
#define EEPROM_LCD_TIMEOUT_OFFSET 0x404
#define EEPROM_LCD_CONTRAST_OFFSET 0x406

// customizations and enhancements (added by Lars June 3,2014)
#define EXTENDED_BEEP 1		// enables extended audio feedback

// UI Mode
#define UI_MODE_STANDARD 0
#define UI_MODE_EXPERT 1

extern uint8_t ui_mode;
extern uint16_t lcd_timeout;
extern uint8_t lcd_contrast;
extern const uint8_t standbyGfx[];
extern const uint8_t startGfx[];
extern const uint8_t pauseGfx[];
extern const uint8_t backGfx[];
extern const uint8_t hourglassGfx[];

extern float recover_height;

void tinkergnome_init();
void lcd_menu_maintenance_expert();
void lcd_menu_print_heatup_tg();
void lcd_menu_printing_tg();
void lcd_menu_move_axes();
void lcd_lib_draw_heater(uint8_t x, uint8_t y, uint8_t heaterPower);
void manage_led_timeout();
void manage_encoder_position(int8_t encoder_pos_interrupt);
void lcd_menu_expert_extrude();
void lcd_extrude_quit_menu();
void recover_start_print();

#endif//ULTI_LCD2_MENU_TINKERGNOME_H
