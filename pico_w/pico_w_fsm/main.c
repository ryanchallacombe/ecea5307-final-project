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
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include <time.h>

/**********************************
 *  FSM STATES
**********************************/
enum FSM_State : uint16_t
{
    S_INITIALIZE,
    S_SERVER_SETUP,
    S_LIS3DH_SETUP,
    S_LOOP,
    S_LIS3DH_INTERRUPT,
    S_ERROR
};

/**********************************
 *  MACROS
**********************************/

#define GPIO_INT_PIN    0
#define TCP_PORT        4242
#define BUF_SIZE        2048
#define POLL_TIME_S     5

/**********************************
 *  GLOBALS
**********************************/
enum FSM_State currentState, nextState;

bool s_initialize_fail = false;
bool s_server_setup_fail = false;
bool s_lis3dh_setup_fail = false;
bool s_lis3dh_interrupt_fail = false;

char *Q_IDN = "IDN?";
char *Q_DATA = "DATA?";

bool data_requested = false;     // set in handle_recved_msg() and reset in tcp_send_data()
err_t g_err = ERR_OK;

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    char recv_buffer[BUF_SIZE];
    char send_buffer[BUF_SIZE];
    float send_data_buffer[BUF_SIZE];
    //struct DATA_MSG_T struct_data[BUF_SIZE];
    //struct data_msg struct_data;
    int send_len;
    int sent_len;
    int recv_len;
} TCP_SERVER_T;


/*******************
 *  LIS3DH SETTINGS
********************/
enum Axis {
    X_Axis = 1,
    Y_Axis = 2,
    Z_Axis = 3
};

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
void uponEnter( const enum FSM_State fsm_state, void *arg );
void updateStateMachine( const enum FSM_State fsm_state );
void uponExit( const enum FSM_State fsm_state );
const char *stateToString ( const enum FSM_State fsm_state );
void gpio_callback( uint gpio, uint32_t events );
static TCP_SERVER_T* tcp_server_init(void);
static bool tcp_server_open(void *arg);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err);
static void tcp_server_err(void *arg, err_t err);
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
err_t handle_recved_msg(void *arg);
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb);
void lis3dh_data_capture(void *arg, uint16_t n, uint8_t axis);


/**********************************
 *  MAIN
**********************************/
int main()
{
    /**********************************
     *  GENERAL SETUP
     **********************************/
    stdio_init_all();
    busy_wait_ms(5000); // wait for USB serial

    /**********************************
     *  WIFI SETUP
     **********************************/
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        //s_initialize_fail = true;
        return -1;
    }

    // Enable wifi station
    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_pw, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("failed to connect.\n");
        //s_initialize_fail = true;
        return -1;
    }
    else
    {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }
    busy_wait_ms(500);

    /**********************************
     *  SERVER SETUP
     **********************************/
    TCP_SERVER_T *tcp_state = tcp_server_init();
    if (!tcp_state)
    {
        printf("tcp_server_init failed\n");
        return -1;
    }
    // This will block until the client connects
    if (!tcp_server_open(tcp_state))
    {
        printf("tcp_server_open failed\n");
        free(tcp_state);
        return -1;
    }

    absolute_time_t abstime = get_absolute_time();  
    uint64_t since_boot_us = to_us_since_boot(abstime);
    char fmt_abstime[20];
    //strftime(fmt_abstime, sizeof(fmt_abstime), "%Y-%m-%d %H:%M:%S", &abstime);
    printf("time: %lld\n", abstime);
    //printf("time: %s\n", fmt_abstime);
    printf("us since boot: %lld\n", since_boot_us);

    currentState = nextState = S_INITIALIZE;

    uponEnter(currentState, tcp_state);

    printf("Starting while() loop ....\n");

    while(true) {
        sleep_ms(500);

        // Udate FSM
        updateStateMachine( currentState );

        // Handle FSM_State Transitions
        if ( nextState != currentState ) {
            uponExit( currentState );
            uponEnter( nextState, tcp_state);
            currentState = nextState;   
        } 
    } // while
}   // main

/**********************************
 *  FUNCTION DEFINITIONS
**********************************/

void uponEnter( const enum FSM_State fsm_state, void *arg ) {
    printf("Entering fsm_state: %s\n", stateToString(fsm_state));
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;

    switch (fsm_state) {
        case S_INITIALIZE:
        {
            break;
        }
        case S_SERVER_SETUP:
        {
            break;
        }
        case S_LIS3DH_SETUP:
        {
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
            // setup interrupts but don't enable yet
            gpio_init(GPIO_INT_PIN);
            gpio_set_irq_enabled_with_callback(GPIO_INT_PIN, GPIO_IRQ_EDGE_RISE, false, &gpio_callback);

            break;
        }
        case S_LOOP:
            // enable interrupts for lis3dh
            gpio_set_irq_enabled_with_callback(GPIO_INT_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
            break;
        case S_LIS3DH_INTERRUPT:
        {
            // read where the interrupt came from and get some info
            uint8_t int1_src = 0;
            i2c_write_blocking(i2c_default, ADDRESS, &int1_src_addr, 1, true);
            i2c_read_blocking(i2c_default, ADDRESS, &int1_src, 1, false);
            printf("INT1_SRC: 0x%02X\n", int1_src);

            // tell the client about the interrupt
            memset(tcp_state->send_buffer, 0, BUF_SIZE); // clear send buffer
            char *msg = "interrupt occured";
            strncpy(tcp_state->send_buffer, msg, strlen(msg));
            tcp_state->send_len = strlen(msg);
            g_err = tcp_server_send_data(arg, tcp_state->client_pcb);
            if (g_err != ERR_OK) {
                printf("error: failed to notify client of interrupt\n");
                s_lis3dh_interrupt_fail = true;
            }
            break;
        }
        case S_ERROR:
            // free tcp state
            if (tcp_state) {
                printf("Freeing tcp state\n");
                free(tcp_state);
            }
            gpio_set_irq_enabled_with_callback(GPIO_INT_PIN, GPIO_IRQ_EDGE_RISE, false, &gpio_callback);
            break;
        default:
            break;
    }

}

void updateStateMachine( const enum FSM_State fsm_state ) {
    //printf("updateStateMachine fsm_state: %s\n", stateToString(fsm_state));

    switch (fsm_state) {
        case S_INITIALIZE:
        {
            if (s_initialize_fail)      // case no longer needed
            {
                nextState = S_ERROR;
            }
            else
            {
                nextState = S_SERVER_SETUP;
            }
            break;
        }
        case S_SERVER_SETUP:
        {
            if (s_server_setup_fail) {      // case no longer needed
                nextState = S_ERROR;
            }    
            else {
                nextState = S_LIS3DH_SETUP;
            }
            break;
        }
        case S_LIS3DH_SETUP:
        {
            if (s_lis3dh_setup_fail)
                nextState = S_ERROR;
            else 
                nextState = S_LOOP;
            break;
        } 
        case S_LOOP:
        {
            if ( lis3dh_int_triggered ) {
                nextState = S_LIS3DH_INTERRUPT;
            }

            // poll for data on tcp
            cyw43_arch_poll();

            break;
        }
        case S_LIS3DH_INTERRUPT:
            if (s_lis3dh_interrupt_fail)
            {
                nextState = S_ERROR;
            }
            else
            {
                nextState = S_LOOP;
            }
            break;
        case S_ERROR:
            break;
        default:
            break;
    }   // switch
}

void uponExit( const enum FSM_State fsm_state ) {
    printf("Exiting fsm_state: %s\n", stateToString(fsm_state));

    switch (fsm_state) {
        case S_INITIALIZE:
            // nothing to do on exit
            break;
        case S_SERVER_SETUP:
            // nothing to do on exit
            break;
        case S_LIS3DH_SETUP:
            break;
        case S_LOOP:
            // nothing to do on exit
            break;
        case S_LIS3DH_INTERRUPT:
            // nothing to do on exit
            break;
        case S_ERROR:
            break;
        default:
            break;
    }

}


const char *stateToString ( const enum FSM_State fsm_state ) {
    const char *stateStr;     // this is a string literal

    switch (fsm_state) {
        case S_INITIALIZE:
            stateStr = "S_INITIALIZE";
            break;
        case S_LOOP:
            stateStr = "S_LOOP";
            break;
        case S_ERROR:
            stateStr = "S_ERROR";
            break;
        case S_LIS3DH_INTERRUPT:
            stateStr = "S_LIS3DH_INTERRUPT";
            break;
        case S_LIS3DH_SETUP:
            stateStr = "S_LIS3DH_SETUP";
            break;
        case S_SERVER_SETUP:
            stateStr = "S_SERVER_SETUP";
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

static TCP_SERVER_T* tcp_server_init(void) {
    TCP_SERVER_T *tcp_state = calloc(1, sizeof(TCP_SERVER_T));
    if (!tcp_state) {
        printf("failed to allocate tcp_state\n");
        return NULL;
    }
    return tcp_state;
}

static bool tcp_server_open(void *arg) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, TCP_PORT);
    if (err) {
        printf("failed to bind to port %u\n", TCP_PORT);
        return false;
    }

    tcp_state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!tcp_state->server_pcb) {
        printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(tcp_state->server_pcb, tcp_state);
    tcp_accept(tcp_state->server_pcb, tcp_server_accept);

    return true;
}


static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        printf("Failure in accept\n");
        //tcp_server_result(arg, err);
        return ERR_VAL;
    }

    printf("Client connected\n");
    tcp_state->client_pcb = client_pcb;

    // register callback functions
    tcp_arg(client_pcb, tcp_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;  
}

static void tcp_server_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        printf("tcp_server_err_fn %d\n", err);
        //tcp_server_result(arg, err);
    }
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
    printf("tcp_server_poll_fn\n");
    return ERR_OK;
}

static err_t tcp_server_close(void *arg) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    err_t err = ERR_OK;
    if (tcp_state->client_pcb != NULL) {
        tcp_arg(tcp_state->client_pcb, NULL);
        tcp_poll(tcp_state->client_pcb, NULL, 0);
        tcp_sent(tcp_state->client_pcb, NULL);
        tcp_recv(tcp_state->client_pcb, NULL);
        tcp_err(tcp_state->client_pcb, NULL);
        err = tcp_close(tcp_state->client_pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(tcp_state->client_pcb);
            err = ERR_ABRT;
        }
        tcp_state->client_pcb = NULL;
    }
    if (tcp_state->server_pcb) {
        tcp_arg(tcp_state->server_pcb, NULL);
        tcp_close(tcp_state->server_pcb);
        tcp_state->server_pcb = NULL;
    }
    return err;
}

// called automatically when data is received???
// TODO how to handle a closed connection
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    if (!p) {
        // return tcp_server_result(arg, -1);
        printf("Client closed connection\n");
        return ERR_OK;
    }

    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        printf("tcp_server_recv %d err %d\n", p->tot_len, err);

        tcp_state->recv_len = pbuf_copy_partial(p, tcp_state->recv_buffer, p->tot_len, 0);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    return handle_recved_msg(tcp_state);
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    printf("tcp_server_sent %u\n", len);
    tcp_state->sent_len = len;
    return ERR_OK;
}

err_t handle_recved_msg(void *arg) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T *)arg;
    data_requested = false;     

    // Determine response to recieved message
    // Note: strcmp = 0 if they strings equal
    if ( !strncmp(Q_IDN, tcp_state->recv_buffer, strlen(Q_IDN)) ) {    
        printf("Recieved IDN?\n");
        memset(tcp_state->send_buffer,0, BUF_SIZE);     // clear send buffer
        char *response = "pico w";
        strncpy(tcp_state->send_buffer, response, strlen(response));
        tcp_state->send_len = strlen(response);
        return tcp_server_send_data(arg, tcp_state->client_pcb);
    } 
    else if ( !strncmp(Q_DATA, tcp_state->recv_buffer, strlen(Q_DATA)) ) {
        memset( tcp_state->send_data_buffer, 0, BUF_SIZE*sizeof(float) );
        data_requested = true;
        
        // hardcode to capture BUF_SIZE of z data for now
        // todo: accept axis and amount request from client
        //uint16_t requested_count = BUF_SIZE;
        uint16_t requested_count = 10;
        uint16_t requested_byte_count = sizeof(float) * requested_count;
        lis3dh_data_capture(tcp_state, requested_count, Z_Axis);
        tcp_state->send_len = requested_byte_count;
        return tcp_server_send_data(arg, tcp_state->client_pcb);
    }
    else {
        printf("Unknown message received\n");
    }

    return ERR_OK;
}

err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb)
{
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;

    tcp_state->sent_len = 0;
    printf("Writing %ld bytes to client\n", tcp_state->send_len);

    cyw43_arch_lwip_check();

    // Two types of transmission, float and char
    err_t err = ERR_OK;
    if ( data_requested ) {
        err = tcp_write(tpcb, tcp_state->send_data_buffer, tcp_state->send_len, TCP_WRITE_FLAG_COPY);        
        data_requested = false;     // reset flag
    } else {
        err = tcp_write(tpcb, tcp_state->send_buffer, tcp_state->send_len, TCP_WRITE_FLAG_COPY);
    }
    if (err != ERR_OK) {
        printf("Failed to write data %d\n", err);
        return err;
    }
    return err;
}

/**********************
 * inputs:  arg:    pointer to TCP state structure
 *          n:      size of the buffer
 *          axis:   selected axis (1 = x, 2 = y, 3 = z)
 */
void lis3dh_data_capture(void *arg, uint16_t n, uint8_t axis) {
    TCP_SERVER_T *tcp_state = (TCP_SERVER_T*)arg;
    
    printf("lis3dh_data_capture fn \n");
    printf("sizeof(float) = %zu\n", sizeof(float));

    switch (axis) {
        case X_Axis:     // X axis
        {
            for (int i = 0; i < n; i++) {
                lis3dh_read_data(0x28, tcp_state->send_data_buffer+i, true);
            }
            break;
        }
        case Y_Axis:     // Y axis
        {
            for (int i = 0; i < n; i++) {
                lis3dh_read_data(0x2A, tcp_state->send_data_buffer+i, true);
            }
            break;
        }
        case Z_Axis:     // Z axis
        {
            absolute_time_t daq_start = get_absolute_time();
            uint64_t daq_start_us = to_us_since_boot(daq_start);
            uint64_t sample_times_us[n];
            for (int i = 0; i < n; i++) {
                lis3dh_read_data(0x2C, tcp_state->send_data_buffer + i*sizeof(float), true);
                sample_times_us[i] = to_us_since_boot(get_absolute_time()) - daq_start_us;
                //printf("Z acceleration: %.3fg\n", *(tcp_state->send_data_buffer + i*sizeof(float)) );
                //sleep_ms(100);
            }
            for (int i = 0; i < n; i++) {
                printf("Z acceleration: %.3fg\n", *(tcp_state->send_data_buffer + i*sizeof(float)) );
                printf("Sample time in us: %lld\n", sample_times_us[i]);
            }
            break;
        }
    }
}