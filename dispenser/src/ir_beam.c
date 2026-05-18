#include "ir_beam.h"
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec ir_sensor = GPIO_DT_SPEC_GET(DT_NODELABEL(ir_sensor), gpios);
static struct gpio_callback ir_cb_data;
static volatile int pill_count = 0;

static void ir_beam_triggered(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    pill_count++;
    printk("Pill detected! Total count: %d\n", pill_count);
}

void ir_beam_init(void)
{
    if (!gpio_is_ready_dt(&ir_sensor)) {
        printk("IR sensor GPIO not ready\n");
        return;
    }
    gpio_pin_configure_dt(&ir_sensor, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&ir_sensor, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&ir_cb_data, ir_beam_triggered, BIT(ir_sensor.pin));
    gpio_add_callback(ir_sensor.port, &ir_cb_data);
}

int ir_beam_get_count(void)
{
    return pill_count;
}