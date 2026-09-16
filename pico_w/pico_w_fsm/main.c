/**********************************
 * PICO W MOTION DETECTION SYSTEM FIRMWARE
 * By: Ryan Challacombe
 * Date: 9/6/2026
 * Version: 0.1
 **********************************/
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include "pico/binary_info.h"
#include "include/wifi_info.h"

/**********************************
 *  FSM STATES
**********************************/
enum State : uint16_t
{
    S_INITIALIZE,
    S_LOOP,
    S_LIS3DH_INTERRUPT,
    S_ERROR
};

/**********************************
 *  MACROS
**********************************/

#define GPIO_INT_PIN 0

/**********************************
 *  GLOBALS
**********************************/
enum State currentState, nextState;

bool s_initialize_fail = false;

/*******************
 *  LIS3DH SETTINGS
********************/
// LIS3DH address
const int ADDRESS = 0x18;

// control reg settings
const uint8_t CTRL_REG_1 = 0x20;    
// const uint8_t SET_CTRL_REG_1 = 0x97;     // enable 3 axes, high res / nomal power mode, ref table 29-31, 10
// const uint8_t SET_CTRL_REG_1 = 0x27;        // enable 3 axes, high res/normal power mode/10Hz, ref table 29-31, 10
const uint8_t SET_CTRL_REG_1 = 0x77;        // enable 3 axes, high res/normal power mode/400Hz, ref table 29-31, 10

const uint8_t CTRL_REG_4 = 0x23;
//const uint8_t SET_CTRL_REG_4 = 0x80;    // block data update enabled, +/-2g FS, ref table 37-38
const uint8_t SET_CTRL_REG_4 = 0x00;        // +/-2g FS
// const uint8_t SET_CTRL_REG_4 = 0x16;    // +/-4g FS

// interrupt settings
const uint8_t CTRL_REG_3 = 0x22;        // interrupt control register
const uint8_t SET_CTRL_REG_3 = 0x3C;    // enable interrupt when IA (interrupt active) + others...

// interrupt settings
const uint8_t CTRL_REG_5 = 0x24;        // interrupt control register
const uint8_t SET_CTRL_REG_5 = 0x08;    // latch interrupt 1

// Don't believe we need this one
//const uint8_t REFERENCE = 0x26;     // reference value for interrupt generation

// INT1_CFG = 0x30, INT1_SRC = 0x31, INT1_THS = 0x32, INT1_DURATION = 0x33
const uint8_t INT1_CFG = 0x30;
//const uint8_t SET_INT1_CFG = 0x3F;    // Enable interrupt on all axes, ref table 53
//const uint8_t SET_INT1_CFG = 0x6A;      // Enable interrupt on HIGH threshold on all axes, ref table 53
const uint8_t SET_INT1_CFG = 0x60;      // Enable interrupt on HIGH threshold on Z-axis, ref table 53

const uint8_t INT1_SRC = 0x31;

const uint8_t INT1_THS = 0x32;        
//const uint8_t SET_INT1_THS = 0x3F;    // 63 LSBs, ref table 59
//const uint8_t SET_INT1_THS = 0x50;      // 80 LSBs, ref table 59
const uint8_t SET_INT1_THS = 0x6E;      // 110 LSBs, ref table 59

const uint8_t INT1_DURATION = 0x33;
const uint8_t SET_INT1_DURATION = 0x01;  // 1 decimal, ref table 61

float x_accel, y_accel, z_accel;

bool lis3dh_int_triggered = false;      // flag that is set in the gpio interrupt callback
const uint8_t int1_src_addr = INT1_SRC;
uint8_t int1_src = 0;

/**********************************
 *  FUNCTION PROTOTYPES
**********************************/
void lis3dh_init();
void lis3dh_calc_value(uint16_t raw_value, float *final_value, bool isAccel);
void lis3dh_read_data(uint8_t reg, float *final_value, bool IsAccel);
void uponEnter( const enum State state );
void updateStateMachine( const enum State state );
void uponExit( const enum State state );
const char *stateToString ( const enum State state );
void gpio_callback( uint gpio, uint32_t events );


/**********************************
 *  MAIN
**********************************/
int main()
{
    currentState = nextState = S_INITIALIZE;

    uponEnter(currentState);

    printf("Starting while() loop ....");

    while(true) {
        // todo: add some loop timing control here

        // Udate FSM
        updateStateMachine( currentState );

        // Handle State Transitions
        if ( nextState != currentState ) {
            uponExit( currentState );
            uponEnter( nextState );
            currentState = nextState;   
        } 

    } // while
}   // main

/**********************************
 *  FUNCTION DEFINITIONS
**********************************/

void uponEnter( const enum State state ) {
    printf("Entering state: %s\n", stateToString(state));

    switch (state) {
        case S_INITIALIZE:
        {
            /**********************************
             *  GENERAL SETUP
             **********************************/
            stdio_init_all();
            busy_wait_ms(5000);     // wait for USB serial

            /**********************************
             *  WIFI SETUP
             **********************************/
            // Initialise the Wi-Fi chip
            if (cyw43_arch_init()) {
                printf("Wi-Fi init failed\n");
                s_initialize_fail = true;
            }

            // Enable wifi station
            cyw43_arch_enable_sta_mode();

            printf("Connecting to Wi-Fi...\n");
            if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_pw, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
                printf("failed to connect.\n");
                // TODO handle this differently
                s_initialize_fail = true;
            } else {
                printf("Connected.\n");
                // Read the ip address in a human readable way
                uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
                printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
            }

            busy_wait_ms(500);

            /**********************************
             *  I2C SETUP
            *********************************/
            // Use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
            i2c_init(i2c_default, 400 * 1000);
            gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
            gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
            gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
            gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
            // Make the I2C pins available to picotool
            bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
            lis3dh_init();

            /**********************************
             *  GPIO INTERRUPT SETUP
            *********************************/
            gpio_init(GPIO_INT_PIN);
            gpio_set_irq_enabled_with_callback(GPIO_INT_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

            break;
        }

        case S_LOOP:
            break;
        case S_LIS3DH_INTERRUPT:
            // todo: 
            // potentially read where the interrupt came from and get some info
            // notify client of interrupt
            break;
        case S_ERROR:
            break;
        default:
            break;
    }

}

void updateStateMachine( const enum State state ) {
    printf("updateStateMachine state: %s\n", stateToString(state));

    switch (state) {
        case S_INITIALIZE:
        {
            if (s_initialize_fail)
            {
                nextState( S_ERROR );

            }
            else
            {
                nextState = S_LOOP;
            }
            break;
        }

        case S_LOOP:
        {

            if ( lis3dh_int_triggered ) {
                nextState( S_LIS3DH_INTERRUPT );
            }




            // todo:   really just need to wait for an interrupt then handle it
            lis3dh_read_data(0x28, &x_accel, true);
            lis3dh_read_data(0x2A, &y_accel, true);
            lis3dh_read_data(0x2C, &z_accel, true);

            if (int_triggered) {
                printf("An interrupt has been triggered since main() began\n");
            }

            // check if interrupt has been triggered
            int1_src = 0;
            i2c_write_blocking(i2c_default, ADDRESS, &int1_src_addr, 1, true);
            i2c_read_blocking(i2c_default, ADDRESS, &int1_src, 1, false);
            if (int1_src & 0x40) {
                int_triggered = true;
                printf("Interrupt triggered!\n");
            }

            // Display data 
            // Acceleration is read as a multiple of g (gravitational acceleration on the Earth's surface)
            printf("ACCELERATION VALUES: \n");
            printf("X acceleration: %.3fg\n", x_accel);
            printf("Y acceleration: %.3fg\n", y_accel);
            printf("Z acceleration: %.3fg\n", z_accel);

            sleep_ms(500);

            // Clear terminal 
            printf("\033[1;1H\033[2J");

            break;
        }
        case S_ERROR:
            break;
        default:
            break;
    }   // switch
}

void uponExit( const enum State state ) {
    printf("Exiting state: %s\n", stateToString(state));

    switch (state) {
        case S_INITIALIZE:
            // nothing to do on exit
            break;
        case S_LOOP:
            // nothing to do on exit
            break;
        case S_ERROR:
            break;
        default:
            break;
    }

}

const char *stateToString ( const enum State state ) {
    const char *stateStr;     // this is a string literal

    switch (state) {
        case S_INITIALIZE:
            stateStr = "S_INITIALIZE";
            break;
        case S_LOOP:
            stateStr = "S_LOOP";
            break;
        case S_ERROR:
            stateStr = "S_ERROR";
            break;
        default:
            break;
    }
    return stateStr;
}

void lis3dh_init() {
    uint8_t buf[2];

    // Turn normal mode and 1.344kHz data rate on
    // ODR = 0b1001 (1.344kHz), LPen = 0 (normal mode), Zen = Yen = Xen = 1 (enable all axes)
    buf[0] = CTRL_REG_1;
    buf[1] = SET_CTRL_REG_1;      
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // Reg 4
    buf[0] = CTRL_REG_4;
    buf[1] = SET_CTRL_REG_4;          
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // latch interrupt 
    buf[0] = CTRL_REG_5;
    buf[1] = SET_CTRL_REG_5;  
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // enable interrupt on IA 
    buf[0] = CTRL_REG_3;
    buf[1] = SET_CTRL_REG_3;  
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // Turn on interrupt 1
    buf[0] = INT1_CFG;
    buf[1] = SET_INT1_CFG;
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // Set threshold for interrupt 1 to 0x10 (16 decimal), ref table 59
    buf[0] = INT1_THS;  
    buf[1] = SET_INT1_THS;  
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);

    // Set duration for interrupt 1
    buf[0] = INT1_DURATION; 
    buf[1] = SET_INT1_DURATION;  // 1 decimal, ref table 61
    i2c_write_blocking(i2c_default, ADDRESS, buf, 2, false);
}

void lis3dh_calc_value(uint16_t raw_value, float *final_value, bool isAccel) {
    // Convert with respect to the value being temperature or acceleration reading 
    float scaling;
    float senstivity = 0.004f; // g per unit

    if (isAccel == true) {
        scaling = 64 / senstivity;
    } else {
        scaling = 64;
    }

    // raw_value is signed
    *final_value = (float) ((int16_t) raw_value) / scaling;
}

void lis3dh_read_data(uint8_t reg, float *final_value, bool IsAccel) {
    // Read two bytes of data and store in a 16 bit data structure
    uint8_t lsb;
    uint8_t msb;
    uint16_t raw_accel;
    i2c_write_blocking(i2c_default, ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, ADDRESS, &lsb, 1, false);

    reg |= 0x01;        // reg = reg | 0x01, effectively adds on for the registers 28, 2A, 2C
    i2c_write_blocking(i2c_default, ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, ADDRESS, &msb, 1, false);

    raw_accel = (msb << 8) | lsb;

    lis3dh_calc_value(raw_accel, final_value, IsAccel);
}

void gpio_callback(uint gpio, uint32_t events) {
    lis3dh_int_triggered = true;
    return;
}