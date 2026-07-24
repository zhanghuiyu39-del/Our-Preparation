#include "soft_i2c.h"

#define SCL_PIN  GPIO_PIN_3
#define SDA_PIN  GPIO_PIN_5
#define I2C_PORT GPIOB

/*
 * 用 BASEPRI 只屏蔽低优先级中断，TIM1(SPWM) 保持 priority 0 可抢占。
 * __disable_irq() 会连 TIM1 一起关掉，调优先级无效。
 * STM32F4 4 位优先级：BASEPRI=0x10 → 屏蔽 priority>=1，放行 priority 0。
 */
#define I2C_BASEPRI_MASK  (0x10U)

#define SCL_H()  HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET)
#define SDA_IN() HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)

static void I2C_Delay(void) {
    for (volatile int i = 0; i < 20; i++);
}

static uint32_t I2C_Lock(void) {
    uint32_t prev = __get_BASEPRI();
    __set_BASEPRI(I2C_BASEPRI_MASK);
    __ISB();
    __DSB();
    return prev;
}

static void I2C_Unlock(uint32_t prev) {
    __set_BASEPRI(prev);
}

static void I2C_Start(void) {
    SDA_H(); SCL_H(); I2C_Delay();
    SDA_L(); I2C_Delay();
    SCL_L(); I2C_Delay();
}

static void I2C_Stop(void) {
    SDA_L(); SCL_H(); I2C_Delay();
    SDA_H(); I2C_Delay();
}

static int I2C_WriteByte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        if (byte & 0x80) SDA_H();
        else             SDA_L();
        byte <<= 1;
        I2C_Delay();
        SCL_H(); I2C_Delay();
        SCL_L(); I2C_Delay();
    }
    SDA_H();
    SCL_H(); I2C_Delay();
    int ack = (SDA_IN() == GPIO_PIN_RESET);
    SCL_L(); I2C_Delay();
    return ack;
}

void Soft_I2C_Init(void) {
    SDA_H();
    SCL_H();
}

void Soft_I2C_WriteCmd(uint8_t addr, uint8_t cmd) {
    uint32_t prim = I2C_Lock();
    I2C_Start();
    I2C_WriteByte(addr);
    I2C_WriteByte(0x00);
    I2C_WriteByte(cmd);
    I2C_Stop();
    I2C_Unlock(prim);
}

void Soft_I2C_WriteData(uint8_t addr, uint8_t data) {
    uint32_t prim = I2C_Lock();
    I2C_Start();
    I2C_WriteByte(addr);
    I2C_WriteByte(0x40);
    I2C_WriteByte(data);
    I2C_Stop();
    I2C_Unlock(prim);
}

void Soft_I2C_WriteMultiData(uint8_t addr, const uint8_t *pdata, uint16_t len) {
    if (len == 0) return;
    uint32_t prim = I2C_Lock();
    I2C_Start();
    I2C_WriteByte(addr);
    I2C_WriteByte(0x40);
    while (len--) {
        I2C_WriteByte(*pdata++);
    }
    I2C_Stop();
    I2C_Unlock(prim);
}
