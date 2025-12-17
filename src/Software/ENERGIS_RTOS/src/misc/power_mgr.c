/**
 * @file src/misc/power_mgr.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-12-10
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
 * @brief Tracks current PWR_LED state to avoid redundant I2C writes.
 * true = LED is ON, false = LED is OFF.
 */
static volatile bool s_pwr_led_state = false;

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
 * @details Uses SwitchTask API to enqueue all-off command.
 * Non-blocking operation prevents watchdog starvation.
 *
 * @return None
 */
static void turn_off_all_relays(void) {
    /* Use non-blocking SwitchTask API with short timeout */
    (void)Switch_AllOff();
}

/**
 * @brief Turn off all display LEDs except PWR_LED.
 *
 * @details Uses SwitchTask-controlled display LED APIs instead of direct MCP access.
 * This guarantees serialized I2C access and avoids bus contention during SNMP stress.
 *
 * @return None
 */
static void turn_off_leds_except_pwr(void) {
    /* FAULT and ETH LEDs OFF */
    (void)Switch_SetFaultLed(false, 50);
    (void)Switch_SetEthLed(false, 50);

    /* PWR LED ON */
    (void)Switch_SetPwrLed(true, 50);
}

/**
 * @brief Turn off all selection LEDs.
 *
 * @details Uses SwitchTask API to ensure selection MCP access is serialized.
 *
 * @return None
 */
static void turn_off_selection_leds(void) { (void)Switch_SelectAllOff(50); }

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

/**
 * @brief Set PWR_LED state with change tracking to avoid redundant I2C writes.
 *
 * @param on true to turn LED ON, false to turn OFF
 * @return None
 */
static void set_pwr_led_tracked(bool on) {
    if (s_pwr_led_state != on) {
        /* Routed via MCP driver → SwitchTask when ready */
        Switch_SetPwrLed(true, 10);
        s_pwr_led_state = on;
    }
}

/* ##################################################################### */
/*                          Public API Functions                         */
/* ##################################################################### */

/**
 * @brief Initialize the power manager subsystem.
 */
void Power_Init(void) {
    s_power_state = PWR_STATE_RUN;
    s_pwr_led_state = false;
    s_led_anim.last_update_ms = 0;
    s_led_anim.phase = 0;
    s_led_anim.direction = 0;

    /* Set initial PWR_LED state */
    set_pwr_led_tracked(true);

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
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_HEALTH, ERR_SEV_WARNING, ERR_FID_POWER_MGR, 0x0);
        WARNING_PRINT_CODE(errorcode, "%s Already in STANDBY\r\n", POWER_MGR_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
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

    /* 6) Track PWR_LED as ON (set by turn_off_leds_except_pwr) */
    s_pwr_led_state = true;

    /* 7) Atomically update power state */
    s_power_state = PWR_STATE_STANDBY;

    INFO_PRINT("%s STANDBY mode active\r\n", POWER_MGR_TAG);

    Logger_MutePush();
}

/**
 * @brief Exit standby mode and return to normal operation.
 */
void Power_ExitStandby(void) {
    if (s_power_state == PWR_STATE_RUN) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_HEALTH, ERR_SEV_WARNING, ERR_FID_POWER_MGR, 0x1);
        WARNING_PRINT_CODE(errorcode, "%s Already in RUN mode\r\n", POWER_MGR_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
        return;
    }

    Logger_MutePop();

    INFO_PRINT("%s Exiting STANDBY mode\r\n", POWER_MGR_TAG);

    /* 1) Release W5500 from reset */
    w5500_release_reset();

    /* 2) Set PWR_LED to solid ON (normal state) - only write if changed */
    set_pwr_led_tracked(true);

    /* 3) ETH_LED will be controlled by NetTask link detection */
    Switch_SetEthLed(false, 10);

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
 *  - PWR_LED state is maintained (no I2C writes needed - already set).
 *  - PROC_LED (GPIO28) PWM output is forced to 0% duty (off).
 *  - CRITICAL: This function does NO I2C operations in RUN mode to avoid
 *    mutex contention with SwitchTask during SNMP stress testing.
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

    /* RUN mode: PWR_LED is already set, PROC_LED off - NO I2C OPERATIONS!
     *
     * CRITICAL: Previously this function called Switch_SetPwrLed(true) every
     * ButtonTask iteration, requiring the display MCP mutex. During SNMP
     * stress testing, SwitchTask frequently holds this mutex for display
     * LED updates, causing ButtonTask to block for 4-6 seconds waiting.
     *
     * Fix: PWR_LED is set once during Power_Init() and Power_ExitStandby().
     * No need to continuously rewrite it. */
    if (s_power_state != PWR_STATE_STANDBY) {
        /* Reset blink state for next standby entry */
        s_last_toggle_ms = 0;
        s_led_on = true;

        /* Keep PROC_LED off when not in standby (PWM only, no I2C) */
        if (s_pwm_init) {
            pwm_set_gpio_level(PROC_LED, 0);
        }

        return;
    }

    /* STANDBY mode - I2C operations are acceptable here since there's
     * no SNMP traffic (W5500 is in reset) */

    /* 0.5 Hz blink for PWR_LED:
     * toggle every half_period_ms (1 s) → 2 s full cycle. */
    if (s_last_toggle_ms == 0U) {
        s_last_toggle_ms = now_ms;
        s_led_on = true;
        set_pwr_led_tracked(true);
    } else if ((now_ms - s_last_toggle_ms) >= half_period_ms) {
        s_last_toggle_ms = now_ms;
        s_led_on = !s_led_on;
        set_pwr_led_tracked(s_led_on);
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