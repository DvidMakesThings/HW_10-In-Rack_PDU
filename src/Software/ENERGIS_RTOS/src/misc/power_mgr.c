/**
 * @file src/misc/power_mgr.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-17
 *
 * @details Implements centralized power state management for ENERGIS standby mode.
 * Provides atomic state transitions and hardware control for entering/exiting standby.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"

/* =====================  Module Tag for Logging  =========================== */
#define POWER_MGR_TAG "[PowerMgr]"

/* =====================  Internal State  ==================================== */
/**
 * @brief Current system power state (atomic access required)
 */
static volatile power_state_t s_power_state = PWR_STATE_RUN;

/**
 * @brief Standby LED animation state
 */
static struct {
    uint32_t last_update_ms; /**< Timestamp of last LED update */
    uint8_t phase;           /**< Current phase in breathing cycle 0-255 */
    uint8_t direction;       /**< 0=fading in, 1=fading out */
} s_led_anim = {0, 0, 0};

/* ##################################################################### */
/*                          Internal Helpers                             */
/* ##################################################################### */

/**
 * @brief Turn off all relay outputs and mirror LEDs.
 *
 * @details Iterates through all 8 channels and sets them to OFF state.
 * This also turns off the corresponding channel LEDs on the display MCP.
 *
 * @return None
 */
static void turn_off_all_relays(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        mcp_set_channel_state(ch, 0);
    }
}

/**
 * @brief Turn off all display LEDs except PWR_LED.
 *
 * @details Directly manipulates the MCP23017 display port to clear all
 * outputs except the PWR_LED bit. This ensures a clean LED state when
 * entering standby.
 *
 * @return None
 */
static void turn_off_leds_except_pwr(void) {
    mcp23017_t *disp = mcp_display();
    if (!disp)
        return;

    /* Port A: channel LEDs 0-7 → all OFF */
    mcp_write_mask(disp, 0, 0xFFu, 0x00u);

    /* Port B: FAULT_LED, ETH_LED, PWR_LED are bits 0, 1, 2
     * Keep PWR_LED (bit 2) ON, turn off FAULT_LED and ETH_LED */
    mcp_write_mask(disp, 1, 0xFFu, (1u << (PWR_LED - 8)));
}

/**
 * @brief Turn off all selection LEDs.
 *
 * @details Clears all selection indicator LEDs on the selection MCP.
 *
 * @return None
 */
static void turn_off_selection_leds(void) {
    mcp23017_t *sel = mcp_selection();
    if (!sel)
        return;

    mcp_write_mask(sel, 0, 0xFFu, 0x00u);
    mcp_write_mask(sel, 1, 0xFFu, 0x00u);
}

/**
 * @brief Hold W5500 in reset (drive RESET pin low).
 *
 * @details Asserts the W5500 hardware reset pin, forcing the Ethernet
 * controller into reset state. PHY link will go down and all network
 * activity will cease.
 *
 * @return None
 */
static void w5500_hold_reset(void) { gpio_put(W5500_RESET, 0); }

/**
 * @brief Release W5500 from reset (drive RESET pin high).
 *
 * @details Deasserts the W5500 hardware reset pin, allowing the chip
 * to come out of reset. NetTask will detect link-up and reinitialize.
 *
 * @return None
 */
static void w5500_release_reset(void) { gpio_put(W5500_RESET, 1); }

/* ##################################################################### */
/*                          Public API Functions                         */
/* ##################################################################### */

/**
 * @brief Initialize the power manager subsystem.
 */
void Power_Init(void) {
    s_power_state = PWR_STATE_RUN;
    s_led_anim.last_update_ms = 0;
    s_led_anim.phase = 0;
    s_led_anim.direction = 0;

    INFO_PRINT("%s Power manager initialized (state=RUN)\r\n", POWER_MGR_TAG);
}

/**
 * @brief Query the current system power state.
 */
power_state_t Power_GetState(void) { return s_power_state; }

/**
 * @brief Enter standby mode.
 */
void Power_EnterStandby(void) {
    if (s_power_state == PWR_STATE_STANDBY) {
        WARNING_PRINT("%s Already in STANDBY\r\n", POWER_MGR_TAG);
        return;
    }

    INFO_PRINT("%s Entering STANDBY mode\r\n", POWER_MGR_TAG);

    /* 1) Turn off all relay outputs */
    turn_off_all_relays();

    /* 2) Turn off selection LEDs */
    turn_off_selection_leds();

    /* 3) Turn off all display LEDs except PWR_LED */
    turn_off_leds_except_pwr();

    /* 4) Hold W5500 in reset */
    w5500_hold_reset();

    /* 5) Initialize LED animation state */
    s_led_anim.last_update_ms = to_ms_since_boot(get_absolute_time());
    s_led_anim.phase = 0;
    s_led_anim.direction = 0;

    /* 6) Atomically update power state */
    s_power_state = PWR_STATE_STANDBY;

    INFO_PRINT("%s STANDBY mode active\r\n", POWER_MGR_TAG);

    Logger_MutePush();
}

/**
 * @brief Exit standby mode and return to normal operation.
 */
void Power_ExitStandby(void) {
    if (s_power_state == PWR_STATE_RUN) {
        WARNING_PRINT("%s Already in RUN mode\r\n", POWER_MGR_TAG);
        return;
    }

    Logger_MutePop();

    INFO_PRINT("%s Exiting STANDBY mode\r\n", POWER_MGR_TAG);

    /* 1) Release W5500 from reset */
    w5500_release_reset();

    /* 2) Set PWR_LED to solid ON (normal state) */
    setPowerGood(true);

    /* 3) ETH_LED will be controlled by NetTask link detection */
    setNetworkLink(false);

    /* 4) Atomically update power state */
    s_power_state = PWR_STATE_RUN;

    INFO_PRINT("%s RUN mode active, network will reinitialize\r\n", POWER_MGR_TAG);

    /* Note: Relays remain OFF; user must explicitly turn them on
     * NetTask will detect the state change and call net_reinit_from_cache()
     * when it sees the link come back up */
    
    static uint32_t s_pwm_slice = 0;
    s_pwm_slice = pwm_gpio_to_slice_num(PROC_LED);
    pwm_set_wrap(s_pwm_slice, 65535U);
    pwm_set_clkdiv(s_pwm_slice, 8.0f);
    pwm_set_enabled(s_pwm_slice, true);
}

/**
 * @brief Service LED indication in RUN and STANDBY modes.
 *
 * @details
 * In RUN mode:
 *  - PWR_LED is kept solid ON via setPowerGood(true).
 *  - PROC_LED (GPIO28) PWM output is forced to 0% duty (off).
 *
 * In STANDBY mode:
 *  - PWR_LED blinks at 0.5 Hz (1 s ON / 1 s OFF, 2 s total period).
 *  - PROC_LED outputs a synchronized "heartbeat" brightness pulse using PWM
 *    on GPIO28 with the same 2 s total period as the PWR_LED blink.
 *
 * The blink/heartbeat period is controlled by a single configuration constant
 * inside this function; change that to adjust both together.
 */
void Power_ServiceStandbyLED(void) {
    /* Configuration knob: common blink + heartbeat period (ms).
     * 2000 ms → 0.5 Hz (1 s ON / 1 s OFF). */
    static const uint32_t s_standby_period_ms = 2000U;
    const uint32_t half_period_ms = s_standby_period_ms / 2U;

    static uint32_t s_last_toggle_ms = 0;
    static bool s_led_on = true;

    /* PROC_LED PWM state */
    static bool s_pwm_init = false;
    static uint32_t s_hb_start_ms = 0;
    static uint32_t s_pwm_slice = 0;

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    /* RUN mode: solid PWR_LED, PROC_LED off */
    if (s_power_state != PWR_STATE_STANDBY) {
        setPowerGood(true);
        s_last_toggle_ms = 0;
        s_led_on = true;

        if (s_pwm_init) {
            /* Keep PROC_LED off when not in standby */
            pwm_set_gpio_level(PROC_LED, 0);
        }

        return;
    }

    /* STANDBY mode */

    /* 0.5 Hz blink for PWR_LED:
     * toggle every half_period_ms (1 s) → 2 s full cycle. */
    if (s_last_toggle_ms == 0U) {
        s_last_toggle_ms = now_ms;
        s_led_on = true;
        setPowerGood(true);
    } else if ((now_ms - s_last_toggle_ms) >= half_period_ms) {
        s_last_toggle_ms = now_ms;
        s_led_on = !s_led_on;
        setPowerGood(s_led_on);
    }

    /* Lazy init of PWM for PROC_LED (GPIO28) */
    if (!s_pwm_init) {
        gpio_set_function(PROC_LED, GPIO_FUNC_PWM);
        s_pwm_slice = pwm_gpio_to_slice_num(PROC_LED);

        /* 16-bit wrap for fine brightness steps; clkdiv gives a few hundred Hz PWM */
        pwm_set_wrap(s_pwm_slice, 65535U);
        pwm_set_clkdiv(s_pwm_slice, 8.0f);
        pwm_set_enabled(s_pwm_slice, true);

        s_pwm_init = true;
        s_hb_start_ms = now_ms;
    }

    if (s_hb_start_ms == 0U) {
        s_hb_start_ms = now_ms;
    }

    /* Heartbeat envelope: triangle wave 0..max..0 over s_standby_period_ms.
     * Same total period as the PWR_LED blink (shared config above). */
    uint32_t elapsed = now_ms - s_hb_start_ms;
    uint32_t t = elapsed % s_standby_period_ms;
    uint32_t duty;

    if (t < half_period_ms) {
        /* Fade up during first half period */
        duty = (t * 65535U) / half_period_ms;
    } else {
        /* Fade down during second half period */
        duty = ((s_standby_period_ms - t) * 65535U) / half_period_ms;
    }

    pwm_set_gpio_level(PROC_LED, (uint16_t)duty);
}
