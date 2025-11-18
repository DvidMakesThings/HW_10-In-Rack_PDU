/**
 * @file power_manager.c
 * @author
 *  DvidMakesThings - David Sipos
 * @defgroup util3 3. Power Manager
 * @ingroup utils
 * @brief Implementation of BUT_PWR long-press handling for peripherals rail + MCU reset.
 * @{
 * @version 1.3
 * @date 2025-09-08
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "power_manager.h"
#include "../CONFIG.h" /* expects BUT_PWR, VREG_EN, INFO_PRINT/WARNING_PRINT/ERROR_PRINT/DEBUG_PRINT */
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

/* ---- Internal state ---- */
static volatile bool s_power_on = true;
static volatile bool s_toggle_pending = false;

static pm_callback_t s_before_off = NULL;
/* kept for interface compatibility; not used in hard-reset path */
static pm_callback_t s_after_on = NULL;

/* ---- Helpers ---- */

static inline uint32_t _now_ms(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }

static void _apply_power(bool on) {
    gpio_put(VREG_EN, on ? 1 : 0);
    s_power_on = on ? true : false;
    if (on) {
        INFO_PRINT("Power rails ON (VREG_EN=1)\n");
    } else {
        INFO_PRINT("Power rails OFF (VREG_EN=0)\n");
    }
}

static inline bool _btn_pressed_level(void) {
    const int lvl = gpio_get(BUT_PWR);
    return PWR_ACTIVE_LOW ? (lvl == 0) : (lvl != 0);
}

/* ---- Public API ---- */

void power_manager_init(void) {
    /* Rail enable default ON */
    gpio_init(VREG_EN);
    gpio_set_dir(VREG_EN, GPIO_OUT);
    gpio_put(VREG_EN, 1);
    s_power_on = true;

    /* Ensure button reads valid when rails are OFF */
    gpio_init(BUT_PWR);
    gpio_set_dir(BUT_PWR, GPIO_IN);
#if PWR_CONFIGURE_PULLS
    if (PWR_ACTIVE_LOW)
        gpio_pull_up(BUT_PWR);
    else
        gpio_pull_down(BUT_PWR);
#endif

    INFO_PRINT("Power_manager_init: rails ON, button configured\n");
}

void power_manager_set_callbacks(pm_callback_t before_off, pm_callback_t after_on) {
    s_before_off = before_off;
    s_after_on = after_on; /* not used when we hard reset after power-on */
}

void power_manager_request_toggle_from_isr(void) { s_toggle_pending = true; }

/**
 * @brief Handle power button state machine.
 *        - If rails are ON and ISR requested toggle → call before_off, turn rails OFF.
 *        - If rails are OFF and button held long → turn rails ON and hard-reset MCU.
 * @return void
 */
void power_manager_process(void) {
    /* --- ON-state: handle pending toggle requested from ISR --- */
    if (s_power_on && s_toggle_pending) {
        s_toggle_pending = false;

        if (s_before_off) {
            s_before_off();
        }
        _apply_power(false); /* turn peripherals OFF */
        /* Do not reset here; wait for user press while OFF to power back and reset. */
        return;
    }

    /* --- OFF-state: poll button for a long-press to power ON + reset MCU --- */
    if (!s_power_on) {
        static bool off_pressed = false;
        static uint32_t off_press_ms = 0;

        const uint32_t now = _now_ms();
        const bool pressed = _btn_pressed_level();

        if (pressed) {
            if (!off_pressed) {
                off_pressed = true;
                off_press_ms = now;
            } else if ((uint32_t)(now - off_press_ms) >= PWR_LONG_MS) {
                INFO_PRINT("OFF-state long-press detected; powering ON and resetting MCU\n");

                /* 1) Re-assert peripherals rail */
                _apply_power(true);

                /* 2) Short settle for rails (SPI/I2C pull-ups, PHY strap pins, etc.) */
                sleep_ms(100);

                /* 3) Park core1 before reboot to avoid concurrent bus/PIO activity */
                multicore_reset_core1();
                sleep_ms(5);

                /* 4) Flush stdio and force USB CDC disconnect so host closes the COM port */
                stdio_flush();
                if (tud_ready()) {
                    tud_task();       /* service one USB frame */
                    tud_disconnect(); /* logically unplug */
                }
                sleep_ms(120); /* give host time to notice */

                /* 5) Hard reset: pc=0, sp=0, delay_ms=5 => normal boot in ~5 ms */
                watchdog_reboot(0, 0, 500);
                for (;;) {
                    tight_loop_contents();
                }
            }
        } else {
            off_pressed = false;
        }
    }
}

bool power_is_on(void) { return s_power_on; }

/** @} */ /* end of util3 */
