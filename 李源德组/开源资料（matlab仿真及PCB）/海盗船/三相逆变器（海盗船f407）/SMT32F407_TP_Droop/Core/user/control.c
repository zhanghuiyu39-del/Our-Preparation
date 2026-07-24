#include "control.h"
#include "control_dq.h"
#include "adc_calib.h"

static uint8_t control_enabled = 0;

static uint16_t key_start_cnt = 0;
static uint8_t key_start_was_pressed = 0;

static uint16_t key_up_cnt = 0;
static uint8_t key_up_was_pressed = 0;

static uint16_t key_dn_cnt = 0;
static uint8_t key_dn_was_pressed = 0;

static uint16_t key_calib_cnt = 0;
static uint8_t key_calib_was_pressed = 0;

static uint8_t key_is_pressed(GPIO_TypeDef *port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

static void key_on_release(uint8_t pressed, uint16_t *press_cnt, uint8_t *was_pressed,
                           void (*on_trigger)(void))
{
    if (pressed) {
        if (*press_cnt < 0xFFFFu) {
            (*press_cnt)++;
        }
    } else if (*was_pressed) {
        if (*press_cnt >= KEY_DEBOUNCE_MIN_SAMPLES &&
            *press_cnt <= KEY_DEBOUNCE_MAX_SAMPLES) {
            on_trigger();
        }
        *press_cnt = 0;
    } else {
        *press_cnt = 0;
    }
    *was_pressed = pressed;
}

static void on_start_toggle(void)
{
    control_enabled = !control_enabled;
    if (control_enabled) {
        ControlDq_OnStart();
    } else {
        Control_PwmStop();
    }
}

static void on_calib_trigger(void)
{
    control_enabled = 0;
    Control_PwmStop();
    AdcCalib_Request();
}

void Control_Init(void)
{
    control_enabled = 0;

    key_start_cnt = 0;
    key_start_was_pressed = 0;
    key_up_cnt = 0;
    key_up_was_pressed = 0;
    key_dn_cnt = 0;
    key_dn_was_pressed = 0;
    key_calib_cnt = 0;
    key_calib_was_pressed = 0;

    AdcCalib_Init();
    ControlDq_Init();
    Control_PwmStop();
}

void Control_UpdateKeys(void)
{
    key_on_release(key_is_pressed(KEY_START_GPIO_Port, KEY_START_Pin),
                   &key_start_cnt, &key_start_was_pressed, on_start_toggle);

    key_on_release(key_is_pressed(KEY_FREQ_UP_GPIO_Port, KEY_FREQ_UP_Pin),
                   &key_up_cnt, &key_up_was_pressed, ControlDq_OnFreqUp);

    key_on_release(key_is_pressed(KEY_FREQ_DN_GPIO_Port, KEY_FREQ_DN_Pin),
                   &key_dn_cnt, &key_dn_was_pressed, ControlDq_OnFreqDown);

    key_on_release(key_is_pressed(KEY_CALIB_GPIO_Port, KEY_CALIB_Pin),
                   &key_calib_cnt, &key_calib_was_pressed, on_calib_trigger);
}

uint8_t Control_IsEnabled(void)
{
    return control_enabled;
}

void Control_PwmStop(void)
{
    ControlDq_PwmStop();
}
