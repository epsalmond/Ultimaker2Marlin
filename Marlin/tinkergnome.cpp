#include "Configuration.h"
#ifdef ENABLE_ULTILCD2
#include <math.h>
#include "Marlin.h"
#include "cardreader.h"
#include "temperature.h"
#include "lifetime_stats.h"
#include "UltiLCD2_low_lib.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2.h"
#include "UltiLCD2_menu_first_run.h"
#include "UltiLCD2_menu_print.h"
#include "UltiLCD2_menu_material.h"
#include "UltiLCD2_menu_maintenance.h"
#include "UltiLCD2_menu_utils.h"

#include "tinkergnome.h"

// low pass filter constant, from 0.0 to 1.0 -- Higher numbers mean more smoothing, less responsiveness.
// 0.0 would be completely disabled, 1.0 would ignore any changes
#define LOW_PASS_SMOOTHING 0.95
#define DEFAULT_FILAMENT_AREA 6.3793966

#define LED_DIM_TIME 0		    // 0 min -> off
#define LED_DIM_MAXTIME 240		// 240 min
#define DIM_LEVEL 0

//#define LED_NORMAL() lcd_lib_led_color(48,48,60)
//#define LED_GLOW() lcd_lib_led_color(8 + led_glow, 8 + led_glow, 32 + led_glow)
//#define LED_GLOW_ERROR() lcd_lib_led_color(8+min(245,led_glow<<3),0,0);

// #define LED_FLASH() lcd_lib_led_color(8 + (led_glow<<3), 8 + min(255-8,(led_glow<<3)), 32 + min(255-32,led_glow<<3))
// #define LED_HEAT() lcd_lib_led_color(192 + led_glow/4, 8 + led_glow/4, 0)
// #define LED_DONE() lcd_lib_led_color(0, 8+led_glow, 8)
// #define LED_COOL() lcd_lib_led_color(0, 4, 16+led_glow)

#define MOVE_DELAY 500  // 500ms

#define GET_UI_MODE() (eeprom_read_byte((const uint8_t*)EEPROM_UI_MODE_OFFSET))
#define SET_UI_MODE(n) do { eeprom_write_byte((uint8_t*)EEPROM_UI_MODE_OFFSET, n); } while(0)
#define GET_LED_TIMEOUT() (eeprom_read_word((const uint16_t*)EEPROM_LED_TIMEOUT_OFFSET))
#define SET_LED_TIMEOUT(n) do { eeprom_write_word((uint16_t*)EEPROM_LED_TIMEOUT_OFFSET, n); } while(0)
#define GET_LCD_TIMEOUT() (eeprom_read_word((const uint16_t*)EEPROM_LCD_TIMEOUT_OFFSET))
#define SET_LCD_TIMEOUT(n) do { eeprom_write_word((uint16_t*)EEPROM_LCD_TIMEOUT_OFFSET, n); } while(0)
#define GET_LCD_CONTRAST() (eeprom_read_byte((const uint8_t*)EEPROM_LCD_CONTRAST_OFFSET))
#define SET_LCD_CONTRAST(n) do { eeprom_write_byte((uint8_t*)EEPROM_LCD_CONTRAST_OFFSET, n); } while(0)

uint8_t ui_mode = UI_MODE_STANDARD;
float recover_height = 0.0f;
uint16_t lcd_timeout = LED_DIM_TIME;
uint8_t lcd_contrast = 0xDF;

static uint16_t led_timeout = LED_DIM_TIME;

// these are used to maintain a simple low-pass filter on the speeds - thanks norpchen
static float e_smoothed_speed[EXTRUDERS] = ARRAY_BY_EXTRUDERS(0.0, 0.0, 0.0);
static float nominal_speed = 0.0;
static float target_position[NUM_AXIS];
static int8_t movingSpeed = 0;
static bool delayMove = false;
static uint8_t printing_page = 0;

static float old_max_feedrate_e;
static float old_retract_acceleration;

const uint8_t thermometerGfx[] PROGMEM = {
    5, 8, //size
    0x60, 0x9E, 0x81, 0x9E, 0x60
};

const uint8_t bedTempGfx[] PROGMEM = {
    7, 8, //size
    0x40, 0x4A, 0x55, 0x40, 0x4A, 0x55, 0x40
};

const uint8_t flowGfx[] PROGMEM = {
    7, 8, //size
    0x07, 0x08, 0x13, 0x7F, 0x13, 0x08, 0x07
};


const uint8_t clockGfx[] PROGMEM = {
    7, 8, //size
    0x1C, 0x22, 0x41, 0x4F, 0x49, 0x22, 0x1C
};

const uint8_t clockInverseGfx[] PROGMEM = {
    7, 8, //size
    0x1C, 0x3E, 0x7F, 0x71, 0x77, 0x3E, 0x1C
};

const uint8_t hourglassGfx[] PROGMEM = {
    7, 8, //size
    0x7F, 0x5D, 0x49, 0x41, 0x49, 0x5D, 0x7F
};

const uint8_t degreeGfx[] PROGMEM = {
    5, 8, //size
    0x06, 0x09, 0x09, 0x06, 0x00
};

const uint8_t fan1Gfx[] PROGMEM = {
    5, 8, //size
    0x26, 0x34, 0x08, 0x16, 0x32
};

const uint8_t speedGfx[] PROGMEM = {
    6, 8, //size
    0x22, 0x14, 0x08, 0x22, 0x14, 0x08
};

const uint8_t revSpeedGfx[] PROGMEM = {
    6, 8, //size
    0x08, 0x14, 0x22, 0x08, 0x14, 0x22
};

const uint8_t homeGfx[] PROGMEM = {
    7, 8, //size
    0x08, 0x7C, 0x46, 0x6F, 0x46, 0x7C, 0x08
};

const uint8_t previousGfx[] PROGMEM = {
    11, 8, //size
    0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

const uint8_t nextGfx[] PROGMEM = {
    11, 8, //size
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x1C, 0x08
};

const uint8_t brightnessGfx[] PROGMEM = {
    7, 8, //size
    0x49, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x49
};

//const uint8_t pauseGfx[] PROGMEM = {
//    7, 8, //size
//    0x7F, 0x41, 0x5D, 0x41, 0x5D, 0x41, 0x7F
//};

const uint8_t startGfx[] PROGMEM = {
    5, 8, //size
    0x7F, 0x7F, 0x3E, 0x1C, 0x08
};

const uint8_t pauseGfx[] PROGMEM = {
    5, 8, //size
    0x3E, 0x3E, 0x00, 0x3E, 0x3E
};

const uint8_t standbyGfx[] PROGMEM = {
    7, 8, //size
    0x1C, 0x22, 0x40, 0x4F, 0x40, 0x22, 0x1C
};

const uint8_t backGfx[] PROGMEM = {
    7, 8, //size
    0x10, 0x38, 0x7C, 0x10, 0x10, 0x10, 0x1E
};

const uint8_t menuGfx[] PROGMEM = {
    5, 8, //size
    0x2A, 0x2A, 0x2A, 0x2A, 0x2A
};

static void lcd_menu_print_page_inc() { lcd_lib_beep(); lcd_basic_screen(); ++printing_page; }
static void lcd_menu_print_page_dec() { lcd_lib_beep(); lcd_basic_screen(); --printing_page; }
static void lcd_print_tune_speed();
static void lcd_print_tune_fan();
static void lcd_print_tune_nozzle0();
static void lcd_print_flow_nozzle0();
#if EXTRUDERS > 1
static void lcd_print_tune_nozzle1();
static void lcd_print_flow_nozzle1();
static void lcd_print_tune_swap_length();
#endif // EXTRUDERS
#if TEMP_SENSOR_BED != 0
static void lcd_print_tune_bed();
#endif
static void lcd_print_tune_retract_length();
static void lcd_print_tune_retract_speed();
static void lcd_print_tune_accel();
static void lcd_print_tune_xyjerk();
static void lcd_print_tune_led();

static void lcd_lib_draw_bargraph( uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, float value );
static char* float_to_string2(float f, char* temp_buffer, const char* p_postfix);

void tinkergnome_init()
{
    if (GET_UI_MODE() == UI_MODE_EXPERT)
        ui_mode = UI_MODE_EXPERT;
    else
        ui_mode = UI_MODE_STANDARD;

    led_timeout = constrain(GET_LED_TIMEOUT(), 0, LED_DIM_MAXTIME);
    lcd_timeout = constrain(GET_LCD_TIMEOUT(), 0, LED_DIM_MAXTIME);

    lcd_contrast = constrain(GET_LCD_CONTRAST(), 0, 0xFF);
    if (lcd_contrast == 0)
        lcd_contrast = 0xDF;
    lcd_lib_contrast(lcd_contrast);
}

void menu_printing_init()
{
    printing_page = 0;
}

// return heatup menu option
static const menu_t & get_heatup_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_print_tune);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_print_abort);
    }
    else if (nr == menu_index++)
    {
        // temp nozzle 1
        opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_nozzle0);
    }
#if EXTRUDERS > 1
    else if (nr == menu_index++)
    {
        // temp nozzle 2
        opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_nozzle1);
    }
#endif
#if TEMP_SENSOR_BED != 0
    else if (nr == menu_index++)
    {
        // temp buildplate
        opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_bed);
    }
#endif
    return opt;
}

static void lcd_print_ask_pause()
{
    menu.add_menu(menu_t(lcd_select_first_submenu, lcd_menu_print_pause, NULL, MAIN_MENU_ITEM_POS(0)));
}

// return print menu option
static const menu_t & get_print_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (printing_page == 1)
    {
        if (nr == menu_index++)
        {
            opt.setData(MENU_NORMAL, LCDMenu::reset_selection, lcd_menu_print_page_dec, NULL);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_NORMAL, lcd_print_ask_pause);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_led, 4);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_retract_length, 4);
        }
#if EXTRUDERS > 1
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_swap_length, 4);
        }
#endif
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_retract_speed, 2);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_accel, 2);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_xyjerk, 2);
        }
    }
    else
    {
        if (nr == menu_index++)
        {
            opt.setData(MENU_NORMAL, lcd_print_ask_pause);
        }
        else if (nr == menu_index++)
        {
            opt.setData(MENU_NORMAL, LCDMenu::reset_selection, lcd_menu_print_page_inc, NULL);
        }
        else if (nr == menu_index++)
        {
            // flow nozzle 1
            opt.setData(MENU_INPLACE_EDIT, lcd_print_flow_nozzle0);
        }
    #if EXTRUDERS > 1
        else if (nr == menu_index++)
        {
            // flow nozzle 2
            opt.setData(MENU_INPLACE_EDIT, lcd_print_flow_nozzle1);
        }
    #endif
        else if (nr == menu_index++)
        {
            // speed
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_speed);
        }
        else if (nr == menu_index++)
        {
            // temp nozzle 1
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_nozzle0);
        }
    #if EXTRUDERS > 1
        else if (nr == menu_index++)
        {
            // temp nozzle 2
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_nozzle1);
        }
    #endif
        else if (nr == menu_index++)
        {
            // fan
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_fan);
        }
    #if TEMP_SENSOR_BED != 0
        else if (nr == menu_index++)
        {
            // temp buildplate
            opt.setData(MENU_INPLACE_EDIT, lcd_print_tune_bed);
        }
    #endif
    }
    return opt;
}

// draws a bargraph
static void lcd_lib_draw_bargraph( uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, float value )
{
	lcd_lib_draw_box(x0, y0, x1, y1);
	value = constrain(value, 0.0, 1.0);
    // draw scale
    float segment = float(abs(x1-x0))/10;
	uint8_t ymin = y0+1;
	uint8_t ymax = y0+(y1-y0)/2-1;
    for (uint8_t i=1; i<10; ++i)
    {
        lcd_lib_draw_vline(x0 + i*segment, ymin, ymax);
    }

    // draw graph
	if (value<0.01) return;
	uint8_t xmax = value*abs(x1-x0)+x0+0.5;
	ymin = ymax+2;
	ymax = y1-1;
    for (uint8_t xpos=x0+1; xpos<xmax; xpos+=3)
    {
        lcd_lib_set (xpos, ymin, xpos+1, ymax);
    }
}


void lcd_lib_draw_heater(uint8_t x, uint8_t y, uint8_t heaterPower)
{
    // draw frame
    lcd_lib_draw_gfx(x, y, thermometerGfx);
    if (heaterPower)
    {
        // draw power beam
        uint8_t beamHeight = min(LCD_CHAR_HEIGHT-2, (heaterPower*(LCD_CHAR_HEIGHT-2)/128)+1);
        lcd_lib_draw_vline(x+2, y+LCD_CHAR_HEIGHT-beamHeight-1, y+LCD_CHAR_HEIGHT-1);

        beamHeight = constrain(beamHeight, 0, 2);
        if (beamHeight>1)
        {
            lcd_lib_draw_vline(x+1, y+LCD_CHAR_HEIGHT-beamHeight-1, y+LCD_CHAR_HEIGHT-1);
            lcd_lib_draw_vline(x+3, y+LCD_CHAR_HEIGHT-beamHeight-1, y+LCD_CHAR_HEIGHT-1);
        }
    }
}

static char* float_to_string2(float f, char* temp_buffer, const char* p_postfix)
{
    int32_t i = (f*10.0) + 0.5;
    char* c = temp_buffer;
    if (i < 0)
    {
        *c++ = '-';
        i = -i;
    }
    if (i >= 1000)
        *c++ = ((i/1000)%10)+'0';
    if (i >= 100)
        *c++ = ((i/100)%10)+'0';
    *c++ = ((i/10)%10)+'0';
    *c++ = '.';
    *c++ = (i%10)+'0';
    *c = '\0';
    if (p_postfix)
    {
        strcpy_P(c, p_postfix);
        c += strlen_P(p_postfix);
    }
    return c;
}

static char* int_to_time_string_tg(unsigned long i, char* temp_buffer)
{
    char* c = temp_buffer;
    uint8_t hours = i / 60 / 60;
    uint8_t mins = (i / 60) % 60;
    uint8_t secs = i % 60;

    if (!hours & !mins) {
        *c++ = '0';
        *c++ = '0';
        *c++ = ':';
        *c++ = '0' + secs / 10;
        *c++ = '0' + secs % 10;
    }else{
        if (hours > 99)
            *c++ = '0' + hours / 100;
        *c++ = '0' + (hours / 10) % 10;
        *c++ = '0' + hours % 10;
        *c++ = ':';
        *c++ = '0' + mins / 10;
        *c++ = '0' + mins % 10;
//        *c++ = 'h';
    }

    *c = '\0';
    return c;
}

static void lcd_progressline(uint8_t progress)
{
    progress = constrain(progress, 0, 124);
    if (progress)
    {
        lcd_lib_set(LCD_GFX_WIDTH-2, min(LCD_GFX_HEIGHT-1, LCD_GFX_HEIGHT - (progress*LCD_GFX_HEIGHT/124)), LCD_GFX_WIDTH-1, LCD_GFX_HEIGHT-1);
        lcd_lib_set(0, min(LCD_GFX_HEIGHT-1, LCD_GFX_HEIGHT - (progress*LCD_GFX_HEIGHT/124)), 1, LCD_GFX_HEIGHT-1);
    }
}

static void lcd_tune_value(int &value, int _min, int _max)
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_lib_tick();
        value = constrain(value + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM), _min, _max);
        lcd_lib_encoder_pos = 0;
    }
}

static void lcd_tune_value(uint8_t &value, uint8_t _min, uint8_t _max)
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_lib_tick();
        value = constrain(value + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM), _min, _max);
        lcd_lib_encoder_pos = 0;
    }
}

static bool lcd_tune_value(float &value, float _min, float _max, float _step)
{
    if (lcd_lib_encoder_pos != 0)
    {
        lcd_lib_tick();
        value = constrain(round((value + (lcd_lib_encoder_pos * _step))/_step)*_step, _min, _max);
        lcd_lib_encoder_pos = 0;
        return true;
    }
    return false;
}

static void lcd_tune_byte(uint8_t &value, uint8_t _min, uint8_t _max)
{
    int temp_value = int((float(value)*_max/255)+0.5);
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_lib_tick();
        temp_value += lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM;
        temp_value = constrain(temp_value, _min, _max);
        value = uint8_t((float(temp_value)*255/_max)+0.5);
        lcd_lib_encoder_pos = 0;
    }
}

static void lcd_tune_temperature(int &value, int _min, int _max)
{
//    if ((value > 0) && (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0))
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_lib_tick();
        value = constrain(value + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM), _min, _max);
        lcd_lib_encoder_pos = 0;
    }
}

static void lcd_tune_speed(float &value, float _min, float _max)
{
    if (lcd_lib_encoder_pos != 0)
    {
        lcd_lib_tick();
        value = constrain(value + (lcd_lib_encoder_pos * 60), _min, _max);
        lcd_lib_encoder_pos = 0;
    }
}

static void lcd_print_tune_speed()
{
    lcd_tune_value(feedmultiply, 0, 999);
}

static void lcd_print_tune_fan()
{
    lcd_tune_byte(fanSpeed, 0, 100);
}

static void lcd_print_tune_nozzle0()
{
    lcd_tune_temperature(target_temperature[0], 0, HEATER_0_MAXTEMP - 15);
}

static void lcd_print_flow_nozzle0()
{
    if (target_temperature[0]>0) {
        lcd_tune_value(extrudemultiply[0], 0, 999);
    }
}

#if EXTRUDERS > 1
static void lcd_print_tune_nozzle1()
{
    lcd_tune_temperature(target_temperature[1], 0, HEATER_1_MAXTEMP - 15);
}

static void lcd_print_flow_nozzle1()
{
    if (target_temperature[1]>0)
    {
        lcd_tune_value(extrudemultiply[1], 0, 999);
    }
}
#endif

#if TEMP_SENSOR_BED != 0
static void lcd_print_tune_bed()
{
    lcd_tune_temperature(target_temperature_bed, 0, BED_MAXTEMP - 15);
}
#endif

static void lcd_print_tune_led()
{
    lcd_tune_value(led_brightness_level, 0, 100);
    analogWrite(LED_PIN, 255 * int(led_brightness_level) / 100);
}

static void lcd_print_tune_retract_length()
{
    lcd_tune_value(retract_length, 0, 50, 0.01);
}

#if EXTRUDERS > 1
static void lcd_print_tune_swap_length()
{
    lcd_tune_value(extruder_swap_retract_length, 0, 50, 0.01);
}
#endif // EXTRUDERS

static void lcd_print_tune_retract_speed()
{
    lcd_tune_speed(retract_feedrate, 0, max_feedrate[E_AXIS]*60);
}

static void lcd_print_tune_accel()
{
    lcd_tune_value(acceleration, 0, 20000, 100.0);
}

static void lcd_print_tune_xyjerk()
{
    lcd_tune_value(max_xy_jerk, 0, 100, 1.0);
}

static void drawHeatupSubmenu (uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32];

    if (nr == index++)
    {
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Tune menu"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+3
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , PSTR("TUNE")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 5, standbyGfx);
            lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING, 5, PSTR("Abort print"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("ABORT"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + 10, BOTTOM_MENU_YPOS, standbyGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("ABORT"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + 10, BOTTOM_MENU_YPOS, standbyGfx);
        }
    }
    else if (nr == index++)
    {
        // temp nozzle 1
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
#if EXTRUDERS < 2
            strcpy_P(buffer, PSTR("Nozzle "));
#else
            strcpy_P(buffer, PSTR("Nozzle(1) "));
#endif
            int_to_string(target_temperature[0], int_to_string(dsp_temperature[0], buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
            lcd_lib_draw_string_left(5, buffer);
            flags |= MENU_STATUSLINE;
        }
        int_to_string(target_temperature[0], buffer, PSTR(DEGREE_SYMBOL));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                          , 51-(EXTRUDERS*LCD_LINE_HEIGHT)-(BED_MENU_OFFSET*LCD_LINE_HEIGHT)
                          , 24
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , flags);
    }
#if EXTRUDERS > 1
    else if (nr == index++)
    {
        // temp nozzle 2
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            strcpy_P(buffer, PSTR("Nozzle(2) "));
            int_to_string(target_temperature[1], int_to_string(dsp_temperature[1], buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
            lcd_lib_draw_string_left(5, buffer);
            flags |= MENU_STATUSLINE;
        }
        int_to_string(target_temperature[1], buffer, PSTR(DEGREE_SYMBOL));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                              , 42-(BED_MENU_OFFSET*LCD_LINE_HEIGHT)
                              , 24
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
#endif
#if TEMP_SENSOR_BED != 0
    else if (nr == index++)
    {
        // temp buildplate
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            strcpy_P(buffer, PSTR("Buildplate "));
            int_to_string(target_temperature_bed, int_to_string(dsp_temperature_bed, buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
            lcd_lib_draw_string_left(5, buffer);
            flags |= MENU_STATUSLINE;
        }
        int_to_string(target_temperature_bed, buffer, PSTR(DEGREE_SYMBOL));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                              , 42
                              , 24
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
#endif
}

static void drawPrintSubmenu (uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32];
    if (printing_page == 1) // second page
    {
        if (nr == index++)
        {
            if (flags & MENU_SELECTED)
            {
                lcd_lib_draw_string_leftP(5, PSTR("Goto page 1"));
                flags |= MENU_STATUSLINE;
            }
            LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT+3
                               , BOTTOM_MENU_YPOS
                               , 3*LCD_CHAR_SPACING
                               , LCD_CHAR_HEIGHT
                               , flags);
            if (flags & MENU_SELECTED)
            {
                lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, previousGfx);
            }
            else
            {
                lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, previousGfx);
            }
        }
        else if (nr == index++)
        {
            if (flags & MENU_SELECTED)
            {
                lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 5, pauseGfx);
                lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING, 5, PSTR("Pause print"));
                flags |= MENU_STATUSLINE;
            }
            LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 - LCD_CHAR_SPACING
                               , BOTTOM_MENU_YPOS
                               , 2*LCD_CHAR_SPACING
                               , LCD_CHAR_HEIGHT
                               , flags);
            if (flags & MENU_SELECTED)
            {
                lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 - 3, BOTTOM_MENU_YPOS, pauseGfx);
            }
            else
            {
                lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 - 3, BOTTOM_MENU_YPOS, pauseGfx);
            }
        }
        else if (nr == index++)
        {
            // brightness
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("LED brightness"));
                flags |= MENU_STATUSLINE;
            }
            // lcd_lib_draw_string_leftP(42, PSTR("LED"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH-2*LCD_CHAR_MARGIN_RIGHT-5*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, brightnessGfx);
            int_to_string(led_brightness_level, buffer, PSTR("%"));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                                  , BOTTOM_MENU_YPOS
                                  , 4*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
        else if (nr == index++)
        {
            // retract length
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Retract length"));
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_string_leftP(15, PSTR("Retract"));
            lcd_lib_draw_stringP(LCD_GFX_WIDTH - 2*LCD_CHAR_MARGIN_RIGHT - 8*LCD_CHAR_SPACING, 15, PSTR("L"));
            float_to_string(retract_length, buffer, PSTR("mm"));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-7*LCD_CHAR_SPACING
                                  , 15
                                  , 7*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
#if EXTRUDERS > 1
        else if (nr == index++)
        {
            // retract length
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Extruder change"));
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_string_leftP(24, PSTR("E"));
            float_to_string(extruder_swap_retract_length, buffer, PSTR("mm"));
            LCDMenu::drawMenuString(2*LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING
                                  , 24
                                  , 7*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
#endif
        else if (nr == index++)
        {
            // retract length
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Retract speed"));
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_stringP(LCD_GFX_WIDTH - 2*LCD_CHAR_MARGIN_RIGHT - 8*LCD_CHAR_SPACING, 24, PSTR("S"));
            int_to_string(retract_feedrate / 60 + 0.5, buffer, PSTR("mm/s"));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-7*LCD_CHAR_SPACING
                                  , 24
                                  , 7*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
        else if (nr == index++)
        {
            // retract length
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Acceleration"));
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_string_leftP(33, PSTR("Accel"));
            int_to_string(acceleration, buffer, PSTR("mm/s" SQUARED_SYMBOL));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-10*LCD_CHAR_SPACING // 2*LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING //
                                  , 33
                                  , 10*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
        else if (nr == index++)
        {
            // retract length
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Max. X/Y Jerk"));
                flags |= MENU_STATUSLINE;
            }

            lcd_lib_draw_string_leftP(42, PSTR("X/Y Jerk"));
            int_to_string(max_xy_jerk, buffer, PSTR("mm/s"));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-7*LCD_CHAR_SPACING
                                  , 42
                                  , 7*LCD_CHAR_SPACING
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    }
    else // first page
    {
       if (nr == index++)
        {
            if (flags & MENU_SELECTED)
            {
                lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 5, pauseGfx);
                lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING, 5, PSTR("Pause print"));
                flags |= MENU_STATUSLINE;
            }
            LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 - LCD_CHAR_SPACING
                               , BOTTOM_MENU_YPOS
                               , 2*LCD_CHAR_SPACING
                               , LCD_CHAR_HEIGHT
                               , flags);
            if (flags & MENU_SELECTED)
            {
                lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 - 3, BOTTOM_MENU_YPOS, pauseGfx);
            }
            else
            {
                lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 - 3, BOTTOM_MENU_YPOS, pauseGfx);
            }
        }
        else if (nr == index++)
        {
            if (flags & MENU_SELECTED)
            {
                lcd_lib_draw_string_leftP(5, PSTR("Goto page 2"));
                flags |= MENU_STATUSLINE;
            }
            LCDMenu::drawMenuBox(LCD_GFX_WIDTH - LCD_CHAR_MARGIN_RIGHT - 3*LCD_CHAR_SPACING
                                    , BOTTOM_MENU_YPOS
                                    , 3*LCD_CHAR_SPACING-1
                                    , LCD_CHAR_HEIGHT
                                    , flags);
            if (flags & MENU_SELECTED)
            {
                lcd_lib_clear_gfx(LCD_GFX_WIDTH - 2*LCD_CHAR_MARGIN_RIGHT - 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, nextGfx);
            }
            else
            {
                lcd_lib_draw_gfx(LCD_GFX_WIDTH - 2*LCD_CHAR_MARGIN_RIGHT - 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, nextGfx);
            }
        }
        else if (nr == index++)
        {
            // flow nozzle 1
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
    #if EXTRUDERS < 2
                strcpy_P(buffer, PSTR("Flow "));
    #else
                strcpy_P(buffer, PSTR("Flow(1) "));
    #endif
                float_to_string2(e_smoothed_speed[0], buffer+strlen(buffer), PSTR("mm" CUBED_SYMBOL PER_SECOND_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT-1, 24, flowGfx);
            int_to_string(extrudemultiply[0], buffer, PSTR("%"));
            LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+12
                                  , 24
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    #if EXTRUDERS > 1
        else if (nr == index++)
        {
            // flow nozzle 2
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                strcpy_P(buffer, PSTR("Flow(2) "));
                float_to_string2(e_smoothed_speed[1], buffer+strlen(buffer), PSTR("mm" CUBED_SYMBOL PER_SECOND_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            int_to_string(extrudemultiply[1], buffer, PSTR("%"));
            LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+42
                                  , 24
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    #endif
        else if (nr == index++)
        {
            // speed
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                strcpy_P(buffer, PSTR("Speed "));
                int_to_string(nominal_speed+0.5, buffer+strlen(buffer), PSTR("mm" PER_SECOND_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 33, speedGfx);
            int_to_string(feedmultiply, buffer, PSTR("%"));
            LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+12
                                  , 33
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
        else if (nr == index++)
        {
            // temp nozzle 1
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
    #if EXTRUDERS < 2
                strcpy_P(buffer, PSTR("Nozzle "));
    #else
                strcpy_P(buffer, PSTR("Nozzle(1) "));
    #endif
                int_to_string(target_temperature[0], int_to_string(dsp_temperature[0], buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 42, thermometerGfx);
            int_to_string((flags & MENU_ACTIVE) ? target_temperature[0] : dsp_temperature[0], buffer, PSTR(DEGREE_SYMBOL));
            LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+12
                                  , 42
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    #if EXTRUDERS > 1
        else if (nr == index++)
        {
            // temp nozzle 2
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                strcpy_P(buffer, PSTR("Nozzle(2) "));
                int_to_string(target_temperature[1], int_to_string(dsp_temperature[1], buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            int_to_string((flags & MENU_ACTIVE) ? target_temperature[1] : dsp_temperature[1], buffer, PSTR(DEGREE_SYMBOL));
            LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+42
                                  , 42
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    #endif
        else if (nr == index++)
        {
            // fan
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                lcd_lib_draw_string_leftP(5, PSTR("Fan"));
                lcd_lib_draw_bargraph(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING, 5, LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT, 11, float(fanSpeed)/255);
                flags |= MENU_STATUSLINE;
            }
            int_to_string(float(fanSpeed)*100/255 + 0.5f, buffer, PSTR("%"));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
    #if TEMP_SENSOR_BED != 0
                                  , 33
    #else
                                  , 42
    #endif
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);

            // fan speed
            static uint8_t fanAnimate = 0;
            static uint8_t prevFanSpeed = 0;

            // start animation
            if (!fanAnimate && fanSpeed!=prevFanSpeed)
                fanAnimate = 32;
            if ((fanSpeed == 0) || (!fanAnimate) || (fanAnimate%4))
            {
    #if TEMP_SENSOR_BED != 0
                lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING-11, 33, fan1Gfx);
    #else
                lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING-11, 42, fan1Gfx);
    #endif
            }
            if (fanAnimate && !(led_glow%16))
            {
                --fanAnimate;
            }
            prevFanSpeed = fanSpeed;

        }
    #if TEMP_SENSOR_BED != 0
        else if (nr == index++)
        {
            // temp buildplate
            if (flags & (MENU_SELECTED | MENU_ACTIVE))
            {
                strcpy_P(buffer, PSTR("Buildplate "));
                int_to_string(target_temperature_bed, int_to_string(dsp_temperature_bed, buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
                lcd_lib_draw_string_left(5, buffer);
                flags |= MENU_STATUSLINE;
            }
            lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING-12, 42, bedTempGfx);
            int_to_string((flags & MENU_ACTIVE) ? target_temperature_bed : dsp_temperature_bed, buffer, PSTR(DEGREE_SYMBOL));
            LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                                  , 42
                                  , 24
                                  , LCD_CHAR_HEIGHT
                                  , buffer
                                  , ALIGN_RIGHT | ALIGN_VCENTER
                                  , flags);
        }
    #endif
    }
}

void lcd_menu_print_heatup_tg()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    char buffer[32];
#if TEMP_SENSOR_BED != 0
    if (current_temperature_bed > target_temperature_bed - 10)
    {
#endif
        for(uint8_t e=0; e<EXTRUDERS; e++)
        {
            if (target_temperature[e] > 0)
                continue;
            target_temperature[e] = material[e].temperature;
        }

        if (current_temperature_bed >= target_temperature_bed - TEMP_WINDOW * 2 && !is_command_queued())
        {
            bool ready = true;
            for(uint8_t e=0; e<EXTRUDERS; e++)
                if (current_temperature[e] < target_temperature[e] - TEMP_WINDOW)
                    ready = false;

            if (ready)
            {
                menu.reset_submenu();
                doStartPrint();
                menu.replace_menu(menu_t(lcd_menu_printing_tg), false);
            }
        }
#if TEMP_SENSOR_BED != 0
    }
#endif

    uint8_t progress = 125;
    for(uint8_t e=0; e<EXTRUDERS; e++)
    {
        if (target_temperature[e] < 1)
            continue;
        if (current_temperature[e] > 20)
            progress = min(progress, (current_temperature[e] - 20) * 125 / (target_temperature[e] - 20 - TEMP_WINDOW));
        else
            progress = 0;
    }
#if TEMP_SENSOR_BED != 0
    if ((current_temperature_bed > 20) && (target_temperature_bed > 20+TEMP_WINDOW))
        progress = min(progress, (current_temperature_bed - 20) * 125 / (target_temperature_bed - 20 - TEMP_WINDOW));
    else if (target_temperature_bed > current_temperature_bed - 20)
        progress = 0;
#endif

    if (progress < minProgress)
        progress = minProgress;
    else
        minProgress = progress;

    lcd_lib_draw_string_leftP(15, PSTR("Heating up... "));

    int_to_string(progress*100/124, buffer, PSTR("%"));
    lcd_lib_draw_string_right(15, buffer);

    lcd_progressline(progress);

    // bed temperature
    uint8_t ypos = 42;
#if TEMP_SENSOR_BED != 0
    // bed temperature
    lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-26, ypos, PSTR("/"));
    int_to_string(dsp_temperature_bed, buffer, PSTR(DEGREE_SYMBOL));
    lcd_lib_draw_string_right(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-34, ypos, buffer);
    lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-71, ypos, bedTempGfx);
    // lcd_lib_draw_heater(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-70, ypos, getHeaterPower(-1));
    ypos -= LCD_LINE_HEIGHT;
#endif // TEMP_SENSOR_BED
#if EXTRUDERS > 1
    // temperature second extruder
    lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-26, ypos, PSTR("/"));
    int_to_string(dsp_temperature[1], buffer, PSTR(DEGREE_SYMBOL));
    lcd_lib_draw_string_right(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-34, ypos, buffer);
    lcd_lib_draw_heater(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-70, ypos, getHeaterPower(1));
    ypos -= LCD_LINE_HEIGHT;
#endif // EXTRUDERS
    // temperature first extruder
    lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-26, ypos, PSTR("/"));
    int_to_string(dsp_temperature[0], buffer, PSTR(DEGREE_SYMBOL));
    lcd_lib_draw_string_right(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-34, ypos, buffer);
    lcd_lib_draw_heater(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-70, ypos, getHeaterPower(0));

    menu.process_submenu(get_heatup_menuoption, EXTRUDERS + BED_MENU_OFFSET + 2);

    uint8_t flags = 0;
    for (uint8_t index=0; index<EXTRUDERS + BED_MENU_OFFSET + 2; ++index)
    {
        menu.drawSubMenu(drawHeatupSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_left(5, card.longFilename);
    }

    lcd_lib_update_screen();
}

unsigned long predictTimeLeft()
{
    float printTimeMs = (millis() - starttime);
    float printTimeSec = printTimeMs / 1000L;
    float totalTimeMs = float(printTimeMs) * float(card.getFileSize()) / float(card.getFilePos());
    static float totalTimeSmoothSec;
    totalTimeSmoothSec = (totalTimeSmoothSec * 999L + totalTimeMs / 1000L) / 1000L;
    if (isinf(totalTimeSmoothSec))
        totalTimeSmoothSec = totalTimeMs;

    if (LCD_DETAIL_CACHE_TIME() == 0 && printTimeSec < 60)
    {
        totalTimeSmoothSec = totalTimeMs / 1000;
        return 0;
    }else{
        unsigned long totalTimeSec;
        if (printTimeSec < LCD_DETAIL_CACHE_TIME() / 2)
        {
            float f = float(printTimeSec) / float(LCD_DETAIL_CACHE_TIME() / 2);
            if (f > 1.0)
                f = 1.0;
            totalTimeSec = float(totalTimeSmoothSec) * f + float(LCD_DETAIL_CACHE_TIME()) * (1 - f);
        }else{
            totalTimeSec = totalTimeSmoothSec;
        }

        return (printTimeSec > totalTimeSec) ? 1 : totalTimeSec - printTimeSec;
    }
}

void lcd_menu_printing_tg()
{
    if (card.pause)
    {
        menu.add_menu(menu_t(lcd_select_first_submenu, lcd_menu_print_resume, NULL, MAIN_MENU_ITEM_POS(0)), true);
    }
    else
    {
        lcd_basic_screen();
        lcd_lib_draw_hline(3, 124, 13);

        // calculate speeds
        if (current_block!=NULL)
        {

            if ((current_block->steps_e > 0) && (current_block->steps_x || current_block->steps_y))
            {
    //                float block_time = current_block->millimeters / current_block->nominal_speed;
    //                float mm_e = current_block->steps_e / axis_steps_per_unit[E_AXIS];

                // calculate live extrusion rate from e speed and filament area
                float speed_e = current_block->steps_e * current_block->nominal_rate / axis_steps_per_unit[E_AXIS] / current_block->step_event_count;
                float volume = (volume_to_filament_length[current_block->active_extruder] < 0.99) ? speed_e / volume_to_filament_length[current_block->active_extruder] : speed_e*DEFAULT_FILAMENT_AREA;

                e_smoothed_speed[current_block->active_extruder] = (e_smoothed_speed[current_block->active_extruder]*LOW_PASS_SMOOTHING) + ( volume *(1.0-LOW_PASS_SMOOTHING));
                nominal_speed = current_block->nominal_speed;
            }
        }

        if (printing_page == 0)
        {
            uint8_t progress = card.getFilePos() / ((card.getFileSize() + 123) / 124);
            unsigned long timeLeftSec;
            char buffer[32];

            switch(printing_state)
            {
            default:

                if (card.pause || isPauseRequested())
                {
                    lcd_lib_draw_gfx(54, 15, hourglassGfx);
                    lcd_lib_draw_stringP(64, 15, (movesplanned() < 1) ? PSTR("Paused...") : PSTR("Pausing..."));
                }
                else
                {
                    // time left
                    timeLeftSec = predictTimeLeft();
                    if (timeLeftSec > 0)
                    {
                        lcd_lib_draw_gfx(54, 15, clockInverseGfx);
                        int_to_time_string_tg(timeLeftSec, buffer);
                        lcd_lib_draw_string(64, 15, buffer);

                        // draw progress string right aligned
                        int_to_string(progress*100/124, buffer, PSTR("%"));
                        lcd_lib_draw_string_right(15, buffer);
                        lcd_progressline(progress);
                    }
                }

                break;
            case PRINT_STATE_WAIT_USER:
                lcd_lib_encoder_pos = ENCODER_NO_SELECTION;
                menu.reset_submenu();
                // lcd_lib_draw_string_left(5, PSTR("Paused..."));
                lcd_lib_draw_string_left(5, card.longFilename);
                lcd_lib_draw_gfx(54, 15, hourglassGfx);
                if (movesplanned() < 1)
                {
                    lcd_lib_draw_stringP(64, 15, PSTR("Paused..."));
                    lcd_lib_draw_string_leftP(BOTTOM_MENU_YPOS, PSTR("Click to continue..."));
                }
                else
                {
                    lcd_lib_draw_stringP(64, 15, PSTR("Pausing..."));
                }
                if (!led_glow)
                {
                    lcd_lib_tick();
                }
                break;
            }

            // all printing states
            // z position
            lcd_lib_draw_string_leftP(15, PSTR("Z"));

            // calculate current z position
            float_to_string(st_get_position(Z_AXIS) / axis_steps_per_unit[Z_AXIS], buffer, 0);
            lcd_lib_draw_string(LCD_CHAR_MARGIN_LEFT+12, 15, buffer);

        }

        uint8_t index = 0;
        uint8_t len = (printing_page == 1) ? 6 + min(EXTRUDERS, 2) : EXTRUDERS*2 + BED_MENU_OFFSET + 4;
        if (printing_state == PRINT_STATE_WAIT_USER)
        {
            index += (printing_page == 1) ? 3 : 2;
        }
        else
        {
            menu.process_submenu(get_print_menuoption, len);
            const char *message = lcd_getstatus();
            if (!menu.isSubmenuSelected() && message && *message)
            {
                lcd_lib_draw_string_left(BOTTOM_MENU_YPOS, message);
                index += (printing_page == 1) ? 3 : 2;
            }
        }

        uint8_t flags = 0;
        for (; index < len; ++index) {
            menu.drawSubMenu(drawPrintSubmenu, index, flags);
        }
        if (!(flags & MENU_STATUSLINE))
        {
            if (printing_state == PRINT_STATE_HEATING)
            {
                lcd_lib_draw_string_leftP(5, PSTR("Heating"));
            }
            else if (printing_state == PRINT_STATE_HEATING_BED)
            {
                lcd_lib_draw_string_leftP(5, PSTR("Heating buildplate"));
            }
            else
            {
                lcd_lib_draw_string_left(5, card.longFilename);
            }
        }
        lcd_lib_update_screen();
    }
}

static char* lcd_expert_item(uint8_t nr, char *buffer)
{
    if (nr == 0)
    {
        strcpy_P(buffer, PSTR("< RETURN"));
    }
    else if (nr == 1)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("User interface"));
    }
    else if (nr == 2)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("LED timeout"));
    }
    else if (nr == 3)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Screen timeout"));
    }
    else if (nr == 4)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Screen contrast"));
    }
    else if (nr == 5)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Move axis"));
    }
    else if (nr == 6)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Disable steppers"));
    }
    else
    {
        strcpy_P(buffer, PSTR("???"));
    }

    return buffer;
}

static char* lcd_uimode_item(uint8_t nr, char *buffer)
{
    if (nr == 0)
    {
        strcpy_P(buffer, PSTR("< RETURN"));
    }
    else if (nr == 1)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Standard Mode"));
    }
    else if (nr == 2)
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("Geek Mode"));
    }
    else
    {
        buffer[0] = ' ';
        strcpy_P(buffer+1, PSTR("???"));
    }

    if (nr-1 == ui_mode)
        buffer[0] = '>';

    return buffer;
}

static void lcd_expert_details(uint8_t nr)
{
    char buffer[32];
    if (nr == 0)
        return;
    else if(nr == 1)
    {
        if (ui_mode & UI_MODE_EXPERT)
        {
            strcpy_P(buffer, PSTR("Geek Mode"));
        }
        else
        {
            strcpy_P(buffer, PSTR("Standard Mode"));
        }
    }
    else if(nr == 2)
    {
        if (led_timeout > 0)
        {
            int_to_string(led_timeout, buffer, PSTR(" min"));
        }
        else
        {
            strcpy_P(buffer, PSTR("off"));
        }
    }
    else if(nr == 3)
    {
        if (lcd_timeout > 0)
        {
            int_to_string(lcd_timeout, buffer, PSTR(" min"));
        }
        else
        {
            strcpy_P(buffer, PSTR("off"));
        }
    }
    else if(nr == 4)
    {
        int_to_string(int(lcd_contrast)*100/255, buffer, PSTR("%"));
    }
    else
    {
        buffer[0] = '\0';
    }
    lcd_lib_draw_string_left(BOTTOM_MENU_YPOS, buffer);
}

static void lcd_uimode_details(uint8_t nr)
{
    // nothing displayed
}

void lcd_menu_uimode()
{
    lcd_scroll_menu(PSTR("User interface"), 3, lcd_uimode_item, lcd_uimode_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(1))
        {
            if (!(ui_mode & UI_MODE_STANDARD))
            {
                ui_mode = UI_MODE_STANDARD;
                SET_UI_MODE(ui_mode);
            }
        }
        else if (IS_SELECTED_SCROLL(2))
        {
            if (!(ui_mode & UI_MODE_EXPERT))
            {
                ui_mode = UI_MODE_EXPERT;
                SET_UI_MODE(ui_mode);
            }
        }
        menu.return_to_previous();
    }
}

static void lcd_menu_led_timeout()
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        led_timeout = constrain(int(led_timeout + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM)), 0, LED_DIM_MAXTIME);
        lcd_lib_encoder_pos = 0;
    }
    if (lcd_lib_button_pressed)
    {
        SET_LED_TIMEOUT(led_timeout);
        lcd_change_to_previous_menu();
    }

    lcd_lib_clear();
    lcd_lib_draw_string_centerP(20, PSTR("LED timeout"));
    lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Click to return"));

    char buffer[32];
    if (led_timeout > 0)
    {
        int_to_string(int(led_timeout), buffer, PSTR(" min"));
    }
    else
    {
        strcpy_P(buffer, PSTR("off"));
    }
    lcd_lib_draw_string_center(30, buffer);
    lcd_lib_update_screen();
}

static void lcd_menu_screen_timeout()
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_timeout = constrain(int(lcd_timeout + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM)), 0, LED_DIM_MAXTIME);
        lcd_lib_encoder_pos = 0;
    }
    if (lcd_lib_button_pressed)
    {
        SET_LCD_TIMEOUT(lcd_timeout);
        menu.return_to_previous();
    }

    lcd_lib_clear();
    lcd_lib_draw_string_centerP(20, PSTR("Screen saver timeout"));
    lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Click to return"));

    char buffer[32];
    if (lcd_timeout > 0)
    {
        int_to_string(int(lcd_timeout), buffer, PSTR(" min"));
    }
    else
    {
        strcpy_P(buffer, PSTR("off"));
    }
    lcd_lib_draw_string_center(30, buffer);
    lcd_lib_update_screen();
}

static void lcd_menu_screen_contrast()
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        lcd_contrast = constrain(int(lcd_contrast + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM)), 0x01, 0xFF);
        lcd_lib_encoder_pos = 0;
        lcd_lib_contrast(lcd_contrast);
    }
    if (lcd_lib_button_pressed)
    {
        SET_LCD_CONTRAST(lcd_contrast);
        menu.return_to_previous();
    }

    lcd_lib_clear();
    lcd_lib_draw_string_centerP(20, PSTR("Screen contrast"));
    lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Click to return"));

    char buffer[32];
    int_to_string(int(lcd_contrast)*100/255, buffer, PSTR("%"));

    lcd_lib_draw_string_center(30, buffer);
    lcd_lib_update_screen();
}

void lcd_menu_maintenance_expert()
{
    lcd_scroll_menu(PSTR("Expert settings"), 7, lcd_expert_item, lcd_expert_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(1))
        {
            menu.add_menu(menu_t(lcd_menu_uimode));
        }
        else if (IS_SELECTED_SCROLL(2))
        {
            menu.add_menu(menu_t(lcd_menu_led_timeout, 0, 4));
        }
        else if (IS_SELECTED_SCROLL(3))
        {
            menu.add_menu(menu_t(lcd_menu_screen_timeout, 0, 4));
        }
        else if (IS_SELECTED_SCROLL(4))
        {
            menu.add_menu(menu_t(lcd_menu_screen_contrast, 0, 4));
        }
        else if (IS_SELECTED_SCROLL(5))
        {
            menu.add_menu(menu_t(lcd_menu_move_axes, MAIN_MENU_ITEM_POS(0)));
        }
        else if (IS_SELECTED_SCROLL(6))
        {
            // disable steppers
            enquecommand_P(PSTR("M84"));
            lcd_lib_beep();
        }
        else
        {
            menu.return_to_previous();
        }
    }
}

static void init_target_positions()
{
    lcd_lib_encoder_pos = 0;
    movingSpeed = 0;
    delayMove = false;
    for (uint8_t i=0; i<NUM_AXIS; ++i)
    {
        target_position[i] = current_position[i] = st_get_position(i)/axis_steps_per_unit[i];
    }
}

static bool endstop_reached(AxisEnum axis, int8_t direction)
{
    if (axis == X_AXIS)
    {
        #if defined(X_MIN_PIN) && X_MIN_PIN > -1
        if ((direction < 0) && (READ(X_MIN_PIN) != X_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        #if defined(X_MAX_PIN) && X_MAX_PIN > -1
        if ((direction > 0) && (READ(X_MAX_PIN) != X_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    else if (axis == Y_AXIS)
    {
        #if defined(Y_MIN_PIN) && Y_MIN_PIN > -1
        if ((direction < 0) && (READ(Y_MIN_PIN) != Y_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        // check max endstop
        #if defined(Y_MAX_PIN) && Y_MAX_PIN > -1
        if ((direction > 0) && (READ(Y_MAX_PIN) != Y_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    else if (axis == Z_AXIS)
    {
        #if defined(Z_MIN_PIN) && Z_MIN_PIN > -1
        if ((direction < 0) && (READ(Z_MIN_PIN) != Z_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        // check max endstop
        #if defined(Z_MAX_PIN) && Z_MAX_PIN > -1
        if ((direction > 0) && (READ(Z_MAX_PIN) != Z_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    return false;
}

static void plan_move(AxisEnum axis)
{
    if (!is_command_queued() && !blocks_queued())
    {
        // enque next move
        if ((abs(target_position[axis] - current_position[axis])>0.005) && !endstop_reached(axis, (target_position[axis]>current_position[axis]) ? 1 : -1))
        {
            current_position[axis] = target_position[axis];
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], homing_feedrate[axis]/800, active_extruder);
        }
    }
}

static void stopMove()
{
    // stop moving
    // enquecommand_P(PSTR("M401"));
    quickStop();
    lcd_lib_encoder_pos = 0;
    movingSpeed = 0;
    delayMove = true;
    for (uint8_t i=0; i<NUM_AXIS; ++i)
    {
        target_position[i] = current_position[i] = constrain(st_get_position(i)/axis_steps_per_unit[i], min_pos[i], max_pos[i]);
    }
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
}

static void lcd_move_axis(AxisEnum axis, float diff)
{
    if (endstop_reached(axis, movingSpeed) || lcd_lib_button_pressed || ((lcd_lib_encoder_pos < 0) && (movingSpeed > 0)) || ((lcd_lib_encoder_pos > 0) && (movingSpeed < 0)))
    {
        // stop moving
        stopMove();
    }
    else if (delayMove)
    {
        // prevent too fast consecutively inputs
        lcd_lib_encoder_pos = 0;
        delayMove = (millis() - last_user_interaction) < MOVE_DELAY;
    }
    else
    {
        if (lcd_lib_encoder_pos/ENCODER_TICKS_PER_SCROLL_MENU_ITEM < 0)
        {
            --movingSpeed;
            lcd_lib_encoder_pos = 0;
        }
        else if (lcd_lib_encoder_pos/ENCODER_TICKS_PER_SCROLL_MENU_ITEM > 0)
        {
            ++movingSpeed;
            lcd_lib_encoder_pos = 0;
        }
        if (movingSpeed != 0)
        {
            movingSpeed = constrain(movingSpeed, -15, 15);
            uint8_t steps = min(abs(movingSpeed)*2, (BLOCK_BUFFER_SIZE - movesplanned()) >> 1);
            for (uint8_t i = 0; i < steps; ++i)
            {
                target_position[axis] = round((current_position[axis]+float(movingSpeed)*diff)/diff)*diff;
                current_position[axis] = constrain(target_position[axis], min_pos[axis], max_pos[axis]);
                plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], abs(movingSpeed), active_extruder);
                if (endstop_reached(axis, movingSpeed))
                {
                    // quickstop
                    stopMove();
                    break;
                }
                else if ((movingSpeed < 0 && (target_position[axis] < min_pos[axis])) || ((movingSpeed > 0) && (target_position[axis] > max_pos[axis])))
                {
                    // stop planner
                    lcd_lib_encoder_pos = 0;
                    movingSpeed = 0;
                    delayMove = true;
                    last_user_interaction = millis();
                    break;
                }
            }
        }
    }
}

static void lcd_move_x_axis()
{
    lcd_move_axis(X_AXIS, 0.1);
}

static void lcd_move_y_axis()
{
    lcd_move_axis(Y_AXIS, 0.1);
}

static void lcd_move_z_axis()
{
    lcd_move_axis(Z_AXIS, 0.05);
}

static void lcd_position_x_axis()
{
    lcd_tune_value(target_position[X_AXIS], min_pos[X_AXIS], max_pos[X_AXIS], 0.05f);
    plan_move(X_AXIS);
}

static void lcd_position_y_axis()
{
    lcd_tune_value(target_position[Y_AXIS], min_pos[Y_AXIS], max_pos[Y_AXIS], 0.05f);
    plan_move(Y_AXIS);
}

static void lcd_position_z_axis()
{
    lcd_tune_value(target_position[Z_AXIS], min_pos[Z_AXIS], max_pos[Z_AXIS], 0.01f);
    plan_move(Z_AXIS);
}

FORCE_INLINE void lcd_home_x_axis() { enquecommand_P(PSTR("G28 X0")); }
FORCE_INLINE void lcd_home_y_axis() { enquecommand_P(PSTR("G28 Y0")); }
FORCE_INLINE void lcd_home_z_axis() { enquecommand_P(PSTR("G28 Z0")); }
FORCE_INLINE void lcd_home_all()    { enquecommand_P(PSTR("G28")); }

static void drawMoveDetails()
{
    char buffer[32];
    int_to_string(abs(movingSpeed), buffer, PSTR("mm" PER_SECOND_SYMBOL));
    lcd_lib_draw_string_center(5, buffer);
    if (movingSpeed <= 0)
    {
        lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT, 5, revSpeedGfx);
    }
    if (movingSpeed >= 0)
    {
        lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-6, 5, speedGfx);
    }
}

// create menu options for "move axes"
static const menu_t & get_move_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // Home all axis
        opt.setData(MENU_NORMAL, lcd_home_all);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // x move
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_move_x_axis, stopMove, 2);
    }
    else if (nr == index++)
    {
        // x position
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_position_x_axis, NULL, 4);
    }
    else if (nr == index++)
    {
        // x home
        opt.setData(MENU_NORMAL, lcd_home_x_axis);
    }
    else if (nr == index++)
    {
        // y move
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_move_y_axis, stopMove, 2);
    }
    else if (nr == index++)
    {
        // y position
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_position_y_axis, NULL, 4);
    }
    else if (nr == index++)
    {
        // y home
        opt.setData(MENU_NORMAL, lcd_home_y_axis);
    }
    else if (nr == index++)
    {
        // z move
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_move_z_axis, stopMove, 2);
    }
    else if (nr == index++)
    {
        // z position
        opt.setData(MENU_INPLACE_EDIT, init_target_positions, lcd_position_z_axis, NULL, 5);
    }
    else if (nr == index++)
    {
        // z home
        opt.setData(MENU_NORMAL, lcd_home_z_axis);
    }
    return opt;
}

static void drawMoveSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32];
    if (nr == index++)
    {
        // Home all axis
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Home all axes"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+3
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , PSTR(" ALL")
                                , ALIGN_CENTER
                                , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+10, 54, homeGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+10, 54, homeGfx);
        }
    }
    else if (nr == index++)
    {
        // RETURN
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
    }
    else if (nr == index++)
    {
        // x move
        if (flags & MENU_ACTIVE)
        {
            drawMoveDetails();
            flags |= MENU_STATUSLINE;
        }
        else if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Move X axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING
                                , 17
                                , LCD_CHAR_SPACING*3
                                , LCD_CHAR_HEIGHT
                                , PSTR("X")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // x position
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("X axis position"));
            flags |= MENU_STATUSLINE;
        }
        float_to_string(st_get_position(X_AXIS) / axis_steps_per_unit[X_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING
                              , 17
                              , 48
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
    else if (nr == index++)
    {
        // x home
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Home X axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING
                           , 17
                           , 10
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 17, homeGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 17, homeGfx);
        }
    }
    else if (nr == index++)
    {
        // y move
        if (flags & MENU_ACTIVE)
        {
            drawMoveDetails();
            flags |= MENU_STATUSLINE;
        }
        else if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Move Y axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING
                                , 29
                                , LCD_CHAR_SPACING*3
                                , LCD_CHAR_HEIGHT
                                , PSTR("Y")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // y position
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Y axis position"));
            flags |= MENU_STATUSLINE;
        }
        float_to_string(st_get_position(Y_AXIS) / axis_steps_per_unit[Y_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING
                              , 29
                              , 48
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
    else if (nr == index++)
    {
        // y home
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Home Y axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING
                           , 29
                           , 10
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 29, homeGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 29, homeGfx);
        }
    }
    else if (nr == index++)
    {
        // z move
        if (flags & MENU_ACTIVE)
        {
            drawMoveDetails();
            flags |= MENU_STATUSLINE;
        }
        else if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Move Z axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING
                                , 41
                                , LCD_CHAR_SPACING*3
                                , LCD_CHAR_HEIGHT
                                , PSTR("Z")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // z position
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Z axis position"));
            flags |= MENU_STATUSLINE;
        }
        float_to_string(st_get_position(Z_AXIS) / axis_steps_per_unit[Z_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING
                              , 41
                              , 48
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
    else if (nr == index++)
    {
        // z home
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Home Z axis"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING
                           , 41
                           , 10
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 41, homeGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH - 3*LCD_CHAR_MARGIN_RIGHT - LCD_CHAR_SPACING + 1, 41, homeGfx);
        }
    }
}

void lcd_menu_move_axes()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_move_menuoption, 11);

    uint8_t flags = 0;
    for (uint8_t index=0; index<11; ++index) {
        menu.drawSubMenu(drawMoveSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Move axis"));
    }

    lcd_lib_update_screen();
}

void manage_led_timeout()
{
    if (led_timeout > 0)
    {
        static uint8_t ledDimmed = 0;
        if ((millis() - last_user_interaction) > (led_timeout*MILLISECONDS_PER_MINUTE))
        {
            if (!ledDimmed)
            {
                analogWrite(LED_PIN, 255 * min(int(DIM_LEVEL), led_brightness_level) / 100);
                ledDimmed ^= 1;
            }
        }
        else if (ledDimmed)
        {
            analogWrite(LED_PIN, 255 * int(led_brightness_level) / 100);
            ledDimmed ^= 1;
        }
    }
}

void manage_encoder_position(int8_t encoder_pos_interrupt)
{
// Added encoder acceleration (Lars Jun 2014)
// if we detect we're moving the encoder the same direction for repeated frames, we increase our step size (up to a maximum)
// if we stop, or change direction, set the step size back to +/- 1
// we only want this for SOME things, like changing a value, and not for other things, like a menu.
// so we have an enable bit
    static int8_t accelFactor = 0;
    if (encoder_pos_interrupt != 0)
    {
        if ((ui_mode & UI_MODE_EXPERT) && (menu.encoder_acceleration_factor() > 1))
        {
            if (encoder_pos_interrupt > 0)		// positive -- were we already going positive last time?  If so, increase our accel
            {
                // increase positive acceleration
                if (accelFactor >= 0)
                    ++accelFactor;
                else
                    accelFactor = 1;
                // beep with a pitch changing tone based on the acceleration factor
                // lcd_lib_beep_ext (500+accelFactor*25, 10);
            }
            else //if (lcd_lib_encoder_pos_interrupt <0)
            {
                // decrease negative acceleration
                if (accelFactor <= 0 )
                    --accelFactor;
                else
                    accelFactor = -1;
                // lcd_lib_beep_ext (600+accelFactor*25, 10);
            }
            lcd_lib_tick();
            lcd_lib_encoder_pos += encoder_pos_interrupt * constrain(abs(accelFactor), 1, menu.encoder_acceleration_factor());
        }
        else
        {
            lcd_lib_encoder_pos += encoder_pos_interrupt;
        }
    }
    else
    {
        // no movement -> no acceleration
        accelFactor=0;
    }
}

static void lcd_extrude_homehead()
{
    lcd_lib_beep();
    enquecommand_P(PSTR("G28 X0 Y0"));
    enquecommand_P(PSTR("M84 X0 Y0"));
}

static void lcd_extrude_headtofront()
{
    lcd_lib_beep();
    enquecommand_P(PSTR("G1 F12000 X110 Y10"));
    enquecommand_P(PSTR("M84 X0 Y0"));
}

static void lcd_extrude_disablexy()
{
    lcd_lib_beep();
    enquecommand_P(PSTR("M84 X0 Y0"));
}

static const menu_t & get_extrude_tune_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_SCROLL_ITEM, lcd_extrude_homehead);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_SCROLL_ITEM, lcd_extrude_headtofront);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_SCROLL_ITEM, lcd_extrude_disablexy);
    }
    return opt;
}

static void drawExtrudeTuneSubmenu (uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT
                           , 20
                           , LCD_GFX_WIDTH-LCD_CHAR_MARGIN_LEFT-LCD_CHAR_MARGIN_RIGHT
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(2*LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, 20, PSTR("Home head"));
            lcd_lib_clear_gfx(2*LCD_CHAR_MARGIN_LEFT, 20, homeGfx);
        }
        else
        {
            lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, 20, PSTR("Home head"));
            lcd_lib_draw_gfx(2*LCD_CHAR_MARGIN_LEFT, 20, homeGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT
                           , 30
                           , LCD_GFX_WIDTH-LCD_CHAR_MARGIN_LEFT-LCD_CHAR_MARGIN_RIGHT
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(2*LCD_CHAR_MARGIN_LEFT, 30, PSTR("Move head to front"));
        }
        else
        {
            lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT, 30, PSTR("Move head to front"));
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT
                           , 40
                           , LCD_GFX_WIDTH-LCD_CHAR_MARGIN_LEFT-LCD_CHAR_MARGIN_RIGHT
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_stringP(2*LCD_CHAR_MARGIN_LEFT, 40, PSTR("Disable XY-steppers"));
        }
        else
        {
            lcd_lib_draw_stringP(2*LCD_CHAR_MARGIN_LEFT, 40, PSTR("Disable XY-steppers"));
        }
    }
}

static void lcd_menu_tune_extrude()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_extrude_tune_menuoption, 4);

    uint8_t flags = 0;
    for (uint8_t index=0; index<4; ++index)
    {
        menu.drawSubMenu(drawExtrudeTuneSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Select option"));
    }

    lcd_lib_update_screen();

}

static void lcd_extrude_return()
{
    set_extrude_min_temp(EXTRUDE_MINTEMP);
    if (!card.sdprinting) target_temperature[active_extruder] = 0;
    menu.return_to_previous();
}

static void lcd_extrude_temperature()
{
    lcd_tune_temperature(target_temperature[active_extruder], 0, (active_extruder > 0) ? HEATER_1_MAXTEMP - 15 : HEATER_0_MAXTEMP - 15);
}

static void lcd_extrude_reset_pos()
{
    lcd_lib_beep();
    plan_set_e_position(0);
    target_position[E_AXIS] = st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS];
    plan_set_e_position(target_position[E_AXIS]);
}

static void lcd_extrude_init_move()
{
    target_position[E_AXIS] = st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS];
    plan_set_e_position(target_position[E_AXIS]);
}

static void lcd_extrude_move()
{
    if (printing_state == PRINT_STATE_NORMAL && movesplanned() < 3)
    {
        if (lcd_tune_value(target_position[E_AXIS], -10000.0, +10000.0, 0.1))
        {
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], target_position[E_AXIS], 10, active_extruder);
        }
    }
}

static void lcd_extrude_quit_move()
{
    // disable E-steppers
    enquecommand_P(PSTR("M84 E0"));
}

static void lcd_extrude_init_pull()
{
    target_position[E_AXIS] = st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS];
    plan_set_e_position(target_position[E_AXIS]);
    //Set E motor power lower so the motor will skip instead of grind.
    digipot_current(2, motor_current_setting[2]*2/3);
    //increase max. feedrate and reduce acceleration
    old_max_feedrate_e = max_feedrate[E_AXIS];
    old_retract_acceleration = retract_acceleration;
    max_feedrate[E_AXIS] = FILAMENT_REVERSAL_SPEED;
    retract_acceleration = FILAMENT_LONG_MOVE_ACCELERATION;
}

static void lcd_extrude_quit_pull()
{
    // reset feeedrate and acceleration to default
    max_feedrate[E_AXIS] = old_max_feedrate_e;
    retract_acceleration = old_retract_acceleration;
    //Set E motor power to default.
    digipot_current(2, motor_current_setting[2]);
    // disable E-steppers
    enquecommand_P(PSTR("M84 E0"));
    target_position[E_AXIS] = st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS];
    plan_set_e_position(target_position[E_AXIS]);
}

static void lcd_extrude_pull()
{
    if (lcd_lib_button_down)
    {
        if (printing_state == PRINT_STATE_NORMAL && movesplanned() < 1)
        {
            target_position[E_AXIS] -= FILAMENT_REVERSAL_LENGTH / volume_to_filament_length[active_extruder];
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], target_position[E_AXIS], FILAMENT_REVERSAL_SPEED*2/3, active_extruder);
        }
    } else {
        quickStop();
        menu.reset_submenu();
    }
}

static void lcd_extrude_tune()
{
    menu.add_menu(lcd_menu_tune_extrude);
}

static const menu_t & get_extrude_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (card.sdprinting) ++nr;

    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_extrude_tune);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_extrude_return);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_INPLACE_EDIT, lcd_extrude_temperature, 4);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_INPLACE_EDIT, lcd_extrude_init_pull, lcd_extrude_pull, lcd_extrude_quit_pull);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_extrude_reset_pos);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_INPLACE_EDIT, lcd_extrude_init_move, lcd_extrude_move, lcd_extrude_quit_move, 2);
    }
    return opt;
}

static void drawExtrudeSubmenu (uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32];
    if (card.sdprinting) ++nr;

    if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT + 1
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("More options"));
            flags |= MENU_STATUSLINE;
            lcd_lib_clear_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_clear_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT + 3*LCD_CHAR_SPACING+1, BOTTOM_MENU_YPOS, PSTR("TUNE"));
            lcd_lib_draw_gfx(2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, menuGfx);
        }
    }
    else if (nr == index++)
    {
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
            lcd_lib_clear_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_clear_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
        else
        {
            lcd_lib_draw_stringP(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 4*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("BACK"));
            lcd_lib_draw_gfx(LCD_GFX_WIDTH/2 + LCD_CHAR_MARGIN_LEFT + 2*LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, backGfx);
        }
    }
    else if (nr == index++)
    {
        // temp nozzle
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            strcpy_P(buffer, PSTR("Nozzle "));
            int_to_string(target_temperature[active_extruder], int_to_string(dsp_temperature[active_extruder], buffer+strlen(buffer), PSTR(DEGREE_SYMBOL"/")), PSTR(DEGREE_SYMBOL));
            lcd_lib_draw_string_left(5, buffer);
            flags |= MENU_STATUSLINE;
        }

        lcd_lib_draw_heater(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING, 20, getHeaterPower(active_extruder));
        int_to_string(dsp_temperature[active_extruder], buffer, PSTR(DEGREE_SYMBOL "/"));
        lcd_lib_draw_string_right(LCD_GFX_WIDTH-2*LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING, 20, buffer);
        int_to_string(target_temperature[active_extruder], buffer, PSTR(DEGREE_SYMBOL));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                          , 20
                          , 24
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , flags);
    }
    else if (nr == index++)
    {
        // move material
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click & hold to pull"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-12*LCD_CHAR_SPACING, 35, PSTR("Pos. E"));
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT+2
                           , 35
                           , 3*LCD_CHAR_SPACING
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING+2, 35, revSpeedGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING+2, 35, revSpeedGfx);
        }
    }
    else if (nr == index++)
    {
        // move material
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Reset position"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-12*LCD_CHAR_SPACING, 35, PSTR("Pos. E"));
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-LCD_CHAR_MARGIN_LEFT
                           , 35
                           , LCD_CHAR_SPACING+2*LCD_CHAR_MARGIN_LEFT
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        }
    }
    else if (nr == index++)
    {
        // move material
        if (flags & MENU_ACTIVE)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Rotate to extrude"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        float_to_string(st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-11*LCD_CHAR_SPACING
                          , 35
                          , 11*LCD_CHAR_SPACING
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , flags);
    }
}

void lcd_extrude_quit_menu()
{
    plan_set_e_position(current_position[E_AXIS]);
}

void lcd_menu_expert_extrude()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    uint8_t len = card.sdprinting ? 5 : 6;

    menu.process_submenu(get_extrude_menuoption, len);

    uint8_t flags = 0;
    for (uint8_t index=0; index<len; ++index)
    {
        menu.drawSubMenu(drawExtrudeSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Move material"));
    }

    lcd_lib_update_screen();

}

static void lcd_recover_start()
{

}

static void lcd_recover_zvalue()
{
    lcd_tune_value(target_position[Z_AXIS], min_pos[Z_AXIS], max_pos[Z_AXIS], 0.01f);
}

static const menu_t & get_recover_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t menu_index = 0;
    if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_recover_start);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == menu_index++)
    {
        opt.setData(MENU_INPLACE_EDIT, lcd_recover_zvalue, 4);
    }
    return opt;
}

static void drawRecoverSubmenu (uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32];
    if (nr == index++)
    {
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Start print"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+2*LCD_CHAR_SPACING
                                , BOTTOM_MENU_YPOS
                                , 8*LCD_CHAR_SPACING
                                , LCD_CHAR_HEIGHT
                                , PSTR("PRINT")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_GFX_WIDTH/2+LCD_CHAR_MARGIN_LEFT+2*LCD_CHAR_SPACING
                                , BOTTOM_MENU_YPOS
                                , 8*LCD_CHAR_SPACING
                                , LCD_CHAR_HEIGHT
                                , PSTR("RETURN")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // temp nozzle
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Recover height"));
            flags |= MENU_STATUSLINE;
        }

        float_to_string(recover_height, buffer, PSTR(DEGREE_SYMBOL));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-4*LCD_CHAR_SPACING
                          , 20
                          , 24
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , flags);
    }
    else if (nr == index++)
    {
        // move material
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click & hold to pull"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-12*LCD_CHAR_SPACING, 35, PSTR("Pos. E"));
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        LCDMenu::drawMenuBox(LCD_CHAR_MARGIN_LEFT+2
                           , 35
                           , 3*LCD_CHAR_SPACING
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING+2, 35, revSpeedGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING+2, 35, revSpeedGfx);
        }
    }
    else if (nr == index++)
    {
        // move material
        if (flags & (MENU_SELECTED | MENU_ACTIVE))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Reset position"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_string_rightP(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-12*LCD_CHAR_SPACING, 35, PSTR("Pos. E"));
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-LCD_CHAR_MARGIN_LEFT
                           , 35
                           , LCD_CHAR_SPACING+2*LCD_CHAR_MARGIN_LEFT
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_clear_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        }
        else
        {
            lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        }
    }
    else if (nr == index++)
    {
        // move material
        if (flags & MENU_ACTIVE)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Rotate to extrude"));
            flags |= MENU_STATUSLINE;
        }
        // lcd_lib_draw_gfx(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-13*LCD_CHAR_SPACING-1, 35, flowGfx);
        float_to_string(st_get_position(E_AXIS) / axis_steps_per_unit[E_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-11*LCD_CHAR_SPACING
                          , 35
                          , 11*LCD_CHAR_SPACING
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , flags);
    }
}

static void lcd_menu_expert_recover()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_recover_menuoption, 5);

    uint8_t flags = 0;
    for (uint8_t index=0; index<5; ++index)
    {
        menu.drawSubMenu(drawRecoverSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Recover print"));
    }

    lcd_lib_update_screen();

}

void recover_start_print()
{
    // recover print from current position

    // heatup nozzle


    while (printing_state == PRINT_STATE_RECOVER)
    {
        manage_heater();
        manage_inactivity();
        lcd_update();
        lifetime_stats_tick();
    }
}

#endif//ENABLE_ULTILCD2