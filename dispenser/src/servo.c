#include "servo.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(servo, LOG_LEVEL_INF);

#define SERVO_PERIOD_US 20000U
#define SERVO_CLOSED_US 1000U
#define SERVO_OPEN_US 2000U

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));

void servo_init(void)
{
    if (!pwm_is_ready_dt(&servo)) {
        LOG_ERR("Servo PWM not ready");
    }
    servo_close();
}

void servo_open(void)
{
    pwm_set_dt(&servo, PWM_USEC(SERVO_PERIOD_US), PWM_USEC(SERVO_OPEN_US));
}

void servo_close(void)
{
    pwm_set_dt(&servo, PWM_USEC(SERVO_PERIOD_US), PWM_USEC(SERVO_CLOSED_US));
}