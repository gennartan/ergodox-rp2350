#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "configurator_mode.h"
#include "configurator_network.h"

LOG_MODULE_REGISTER(configurator);

#define CONFIGURATOR_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(ergodox_configurator)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(ergodox_configurator) <= 1,
    "Only one configurator boot chord may be defined");

#if DT_HAS_COMPAT_STATUS_OKAY(ergodox_configurator)
#define CHORD_GPIO(node_id, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx),
static const struct gpio_dt_spec chord[] = {
    DT_FOREACH_PROP_ELEM(CONFIGURATOR_NODE, gpios, CHORD_GPIO)
};
BUILD_ASSERT(ARRAY_SIZE(chord) > 0, "Configurator chord must not be empty");
#endif

int configurator_mode_requested(bool *requested)
{
    *requested = false;
#if DT_HAS_COMPAT_STATUS_OKAY(ergodox_configurator)
    for (size_t i = 0; i < ARRAY_SIZE(chord); ++i) {
        if (!gpio_is_ready_dt(&chord[i])) {
            return -ENODEV;
        }
        int ret = gpio_pin_configure_dt(&chord[i], GPIO_INPUT);
        if (ret < 0) {
            return ret;
        }
    }

    /* Allow pulls to settle, then require the entire chord for 20 ms. */
    k_sleep(K_MSEC(1));
    for (int sample = 0; sample <= 4; ++sample) {
        for (size_t i = 0; i < ARRAY_SIZE(chord); ++i) {
            int ret = gpio_pin_get_dt(&chord[i]);
            if (ret < 0) {
                return ret;
            }
            if (ret == 0) {
                return 0;
            }
        }
        if (sample < 4) {
            k_sleep(K_MSEC(5));
        }
    }
    *requested = true;
#endif
    return 0;
}

void configurator_mode_run(void)
{
    int ret = configurator_network_start();

    if (ret != 0) {
        LOG_ERR("Configurator startup failed: %d; reboot to retry", ret);
    }
    for (;;) {
        k_sleep(K_FOREVER);
    }
}
