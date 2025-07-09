#include "pinctrl.h"
#include "uart.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"

/* UART传输线程 */
#define UART1                       1
#define UART1_TXD_PIN               (GPIO_15)
#define UART1_RXD_PIN               (GPIO_16)
#define UART_BAUDRATE               230400
#define UART_RX_BUF_SIZE            4
#define UART_TASK_PRIO              16
#define UART_TASK_STACK_SIZE        0x1200
#define UART_RX_LENGTH              4
#define UART_TX_LENGTH              4
#define LED_PIN                     (GPIO_02)

/* 舵机线程 */
#define PWM_TASK_STACK_SIZE         0x1200
#define PWM1_TASK_PRIO              20
#define PWM2_TASK_PRIO              21
#define PWM1_PIN                    (GPIO_10)
#define PWM2_PIN                    (GPIO_12)
#define PWM_HIGH(pin)               (uapi_gpio_set_val(pin, GPIO_LEVEL_HIGH))
#define PWM_LOW(pin)                (uapi_gpio_set_val(pin, GPIO_LEVEL_LOW))
#define PWM_PERIOD                  (20000)

static void app_uart_read_int_handler(const void *buffer, uint16_t length, bool error);
void LED_Init(void);
void LED_BLINK(void);
void PWM_Init(void);
uint8_t dataCheck(uint8_t* buf);
void setAngle(uint8_t angle, uint8_t pwmPin, uint16_t range);

uint8_t uartRcvBuf[UART_RX_BUF_SIZE] = {0};
static uart_buffer_config_t uartBufferConfig = {
    .rx_buffer = uartRcvBuf,
    .rx_buffer_size = UART_RX_BUF_SIZE
};

static void uart1_PinInit(void)
{
    uapi_pin_set_mode(UART1_TXD_PIN, PIN_MODE_1);
    uapi_pin_set_mode(UART1_RXD_PIN, PIN_MODE_1);
}

static void uart1_Init(void)
{
    uart_attr_t attr = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_config = {
        .tx_pin = UART1_TXD_PIN,
        .rx_pin = UART1_RXD_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uapi_uart_deinit(UART1);
    uapi_uart_init(UART1, &pin_config, &attr, NULL, &uartBufferConfig);
    if(uapi_uart_register_rx_callback(UART1, UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE, 8, app_uart_read_int_handler) != ERRCODE_SUCC) 
    {
        while (1)
        {
            osal_printk("uart int mode register receive callback fail!\r\n");
            osal_msleep(1000);
        }
    }
}

uint8_t intRcvFlag = 0;
static void app_uart_read_int_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    if (buffer == NULL || length == 0) 
    {
        osal_printk("uart int mode transfer illegal data!\r\n");
        return;
    }

    uint8_t *buff = (uint8_t *)buffer;
    uint16_t intRcvLength = length;
    if(intRcvLength == UART_RX_LENGTH && dataCheck(buff) == 0)
    {
        if(memcpy_s(uartRcvBuf, length, buff, length) == EOK)
        {
            intRcvFlag = 1;
            osal_printk("recvLength:%u\r\n", intRcvLength);
        }
        else
        {
            osal_printk("memcopy fail!\r\n");
        }
        return;
    }
    else
    {
        osal_printk("illegal data!: %u %x\r\n", intRcvLength, buff[0]);
        return;
    }
}

uint8_t angle1 = 90;
uint8_t angle2 = 140;
static void *uart_task(void)
{
    uart1_PinInit();
    uart1_Init();

    while(1) 
    {
        if(intRcvFlag == 1)
        {
            angle1 = uartRcvBuf[1];
            angle2 = uartRcvBuf[2];
            if(angle2 < 80) angle2 = 80;
            if(angle2 > 160) angle2 = 160;
            osal_printk("angle: %u %u\r\n", angle1, angle2);
            LED_BLINK();
            intRcvFlag = 0;
        }
        osal_msleep(5);
    }
    return NULL;
}

static void *servo_task(void)
{
    while(1)
    {
        setAngle(angle1, PWM1_PIN, 270);
        osal_msleep(10);
        setAngle(angle2, PWM2_PIN, 180);
        osal_msleep(10);
    }
    return NULL;
}

static void uartTaskEntry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)uart_task, NULL, "uart_task", UART_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, UART_TASK_PRIO);
    } else {
        osal_printk("Create uart_task fail.\r\n");
    }
    osal_kfree(task_handle);
    osal_kthread_unlock();
}

static void servoTaskEntry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)servo_task, NULL, "servo_task", PWM_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, PWM1_TASK_PRIO);
    } else {
        osal_printk("Create servo_task fail.\r\n");
    }
    osal_kfree(task_handle);
    osal_kthread_unlock();
}

/* Run the main. */
static void main_entry(void)
{
    LED_Init();
    PWM_Init();
    uartTaskEntry();
    servoTaskEntry();
}
app_run(main_entry);
/* End. */

void setAngle(uint8_t angle, uint8_t pwmPin, uint16_t range)
{
    uint8_t i = 0;
    uint32_t usDelay = 500 + angle * (2000) / range; //us

    PWM_HIGH(pwmPin);
    osal_udelay(usDelay);
    PWM_LOW(pwmPin);
    osal_udelay(PWM_PERIOD - usDelay);

    PWM_HIGH(pwmPin);
    osal_udelay(usDelay);
    PWM_LOW(pwmPin);
    osal_udelay(PWM_PERIOD - usDelay);

    PWM_HIGH(pwmPin);
    osal_udelay(usDelay);
    PWM_LOW(pwmPin);
    osal_udelay(PWM_PERIOD - usDelay);

    PWM_HIGH(pwmPin);
    osal_udelay(usDelay);
    PWM_LOW(pwmPin);
    osal_udelay(PWM_PERIOD - usDelay);

    PWM_HIGH(pwmPin);
    osal_udelay(usDelay);
    PWM_LOW(pwmPin);
    osal_udelay(PWM_PERIOD - usDelay);
}

// void setAngle(uint8_t angle, uint8_t pwmPin, uint16_t range)
// {
//     uint8_t i = 0, msDelay = 0;
//     uint16_t usDelay2; //us
//     uint32_t usDelay1 = 500 + angle * (2000) / range; //us
//     msDelay = (PWM_PERIOD - usDelay1) / 1000;
//     usDelay2 = PWM_PERIOD - usDelay1 - msDelay * 1000;

//     PWM_HIGH(pwmPin);
//     osal_udelay(usDelay1);
//     PWM_LOW(pwmPin);
//     osal_udelay(usDelay2);
//     osal_msleep(msDelay);
// }

void LED_Init(void)
{
    uapi_pin_set_mode(LED_PIN, PIN_MODE_0);
    uapi_gpio_set_dir(LED_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(LED_PIN, GPIO_LEVEL_LOW);
}

void LED_BLINK(void)
{
    uapi_gpio_toggle(LED_PIN);
}

void PWM_Init(void)
{
    uapi_pin_set_mode(PWM1_PIN, PIN_MODE_0);
    uapi_gpio_set_dir(PWM1_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(PWM1_PIN, GPIO_LEVEL_LOW);

    uapi_pin_set_mode(PWM2_PIN, PIN_MODE_0);
    uapi_gpio_set_dir(PWM2_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(PWM2_PIN, GPIO_LEVEL_LOW);
}

uint8_t dataCheck(uint8_t* buf)
{
    uint8_t data = 0;
    if(buf[0] == 0xA5)
    {
        data = (uint8_t)(buf[0] + buf[1] + buf[2]);
        if(data == buf[3])
        {
            return 0;
        }
        else
        {
            osal_printk("dataCheck error.\r\n");
            return 1;
        }
    }
    else
    {
        return 1;
    }
}

// void setAngle(uint8_t angle)
// {
//     uint8_t i = 0;
//     uint32_t usDelay = 500 + angle * (2000) / 180; //us

//     for(i=0; i<PWM_COUNT; i++)
//     {
//         PWM_HIGH;
//         osal_udelay(usDelay);
//         PWM_LOW;
//         osal_udelay(20000 - usDelay);
//     }
// }
