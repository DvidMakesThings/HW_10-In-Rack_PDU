/**
 * @file src/misc/power_mgr.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-12-10
 *
 * @details
 * Implementation of centralized power state management including standby mode entry/exit
 * logic, hardware peripheral control coordination, and LED animation for standby indication.
 *
 * This module coordinates multiple hardware peripherals (W5500, MCP23017, relays) and
 * provides thread-safe state query for task behavior adaptation.
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
 * @brief Disable all relay outputs using SwitchTask API.
 *
 * Enqueues a non-blocking command to turn off all relay outputs via the
 * SwitchTask message queue. This ensures serialized access to relay control
 * hardware and prevents I2C bus contention.
 *
 * @return None
 */
static void turn_off_all_relays(void) {
    /* Use non-blocking SwitchTask API with short timeout */
    (void)Switch_AllOff();
}

/**
 * @brief Disable display LEDs except PWR_LED for standby indication.
 *
 * Turns off FAULT and ETH LEDs while keeping PWR_LED on. Uses SwitchTask API
 * to ensure serialized I2C access to the MCP23017 display controller. This prevents
 * bus contention that can occur during concurrent access from multiple tasks.
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
 * @brief Disable all channel selection LEDs.
 *
 * Turns off all selection LEDs using SwitchTask API with 50ms timeout.
 * Ensures serialized access to the selection LED MCP23017 controller.
 *
 * @return None
 */
static void turn_off_selection_leds(void) { (void)Switch_SelectAllOff(50); }

/**
 * @brief Assert W5500 hardware reset to disable Ethernet.
 *
 * Drives the W5500 RESET pin low, forcing the Ethernet controller into
 * reset state. This disables the PHY, terminates all network connections,
 * and stops all traffic. Power consumption is reduced as the chip enters
 * a minimal state.
 *
 * @return None
 */
static void w5500_hold_reset(void) { gpio_put(W5500_RESET, 0); }

/**
 * @brief Deassert W5500 hardware reset to enable Ethernet.
 *
 * Drives the W5500 RESET pin high, allowing the Ethernet controller to
 * exit reset state and begin initialization. NetTask will detect the
 * state change via Power_GetState() and reinitialize network services.
 *
 * @return None
 */
static void w5500_release_reset(void) { gpio_put(W5500_RESET, 1); }

/**
 * @brief Set PWR_LED state with change tracking.
 *
 * Updates PWR_LED state only if it differs from the current tracked state,
 * avoiding redundant I2C write operations that can cause bus contention.
 *
 * @param on true to enable LED, false to disable LED
 *
 * @return None
 */
static void set_pwr_led_tracked(bool on) {
    if (s_pwr_led_state != on) {
        /* Routed via MCP driver → SwitchTask when ready */
        Switch_SetPwrLed(on, 10);
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
    /* Initialize power state to normal operation */
    s_power_state = PWR_STATE_RUN;
    s_pwr_led_state = false;

    /* Clear LED animation state */
    s_led_anim.last_update_ms = 0;
    s_led_anim.phase = 0;
    s_led_anim.direction = 0;

    /* Enable PWR_LED for normal operation indication */
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
    /* Prevent redundant standby entry */
    if (s_power_state == PWR_STATE_STANDBY) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_HEALTH, ERR_SEV_WARNING, ERR_FID_POWER_MGR, 0x0);
        WARNING_PRINT_CODE(errorcode, "%s Already in STANDBY\r\n", POWER_MGR_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
        return;
    }

    INFO_PRINT("%s Entering STANDBY mode\r\n", POWER_MGR_TAG);

    /* Disable all relay outputs for safety and power savings */
    turn_off_all_relays();

    /* Clear all selection LEDs */
    turn_off_selection_leds();

    /* Set display LEDs to standby pattern */
    turn_off_leds_except_pwr();

    /* Update state atomically before W5500 reset to allow NetTask
     * to detect the transition and skip network operations immediately */
    s_power_state = PWR_STATE_STANDBY;

    /* Assert W5500 reset to disable networking */
    w5500_hold_reset();

    /* Initialize LED breathing animation state */
    s_led_anim.last_update_ms = to_ms_since_boot(get_absolute_time());
    s_led_anim.phase = 0;
    s_led_anim.direction = 0;

    /* Track PWR_LED state (enabled by turn_off_leds_except_pwr) */
    s_pwr_led_state = true;

    INFO_PRINT("%s STANDBY mode active\r\n", POWER_MGR_TAG);

    /* Suppress normal logging output during standby */
    Logger_MutePush();
}

/**
 * @brief Exit standby mode and return to normal operation.
 */
void Power_ExitStandby(void) {
    /* Prevent redundant wake operation */
    if (s_power_state == PWR_STATE_RUN) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_HEALTH, ERR_SEV_WARNING, ERR_FID_POWER_MGR, 0x1);
        WARNING_PRINT_CODE(errorcode, "%s Already in RUN mode\r\n", POWER_MGR_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
        return;
    }

    /* Restore normal logging output */
    Logger_MutePop();

    INFO_PRINT("%s Exiting STANDBY mode\r\n", POWER_MGR_TAG);

    /* Deassert W5500 reset to enable Ethernet controller */
    w5500_release_reset();

    /* Set PWR_LED to solid on for normal operation */
    set_pwr_led_tracked(true);

    /* Clear ETH_LED (NetTask will control it based on link status) */
    Switch_SetEthLed(false, 10);

    /* Restore relay state from configured startup preset if available
     * This may include powering an external network switch */
    (void)UserOutput_ApplyStartupPreset();

    /* Update power state atomically */
    s_power_state = PWR_STATE_RUN;

    INFO_PRINT("%s RUN mode active, network will reinitialize\r\n", POWER_MGR_TAG);

    /* Reconfigure PROC_LED PWM for normal operation indication */
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
    /* Standby LED pattern period configuration */
    static const uint32_t s_standby_period_ms = 2000U;
    const uint32_t half_period_ms = s_standby_period_ms / 2U;

    /* PWR_LED blink state */
    static uint32_t s_last_toggle_ms = 0;
    static bool s_led_on = true;

    /* PROC_LED PWM state */
    static bool s_pwm_init = false;
    static uint32_t s_hb_start_ms = 0;
    static uint32_t s_pwm_slice = 0;

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    /* RUN mode: LEDs already configured, no updates needed */
    if (s_power_state != PWR_STATE_STANDBY) {
        /* Reset blink state for clean standby entry */
        s_last_toggle_ms = 0;
        s_led_on = true;

        /* Disable PROC_LED heartbeat */
        if (s_pwm_init) {
            pwm_set_gpio_level(PROC_LED, 0);
        }

        return;
    }

    /* STANDBY mode: Implement PWR_LED blink and PROC_LED heartbeat */

    /* Toggle PWR_LED at 0.5 Hz (1s on / 1s off) */
    if (s_last_toggle_ms == 0U) {
        /* Initialize blink timing */
        s_last_toggle_ms = now_ms;
        s_led_on = true;
        set_pwr_led_tracked(true);
    } else if ((now_ms - s_last_toggle_ms) >= half_period_ms) {
        /* Toggle LED state */
        s_last_toggle_ms = now_ms;
        s_led_on = !s_led_on;
        set_pwr_led_tracked(s_led_on);
    }

    /* Initialize PROC_LED PWM on first standby LED service call */
    if (!s_pwm_init) {
        gpio_set_function(PROC_LED, GPIO_FUNC_PWM);
        s_pwm_slice = pwm_gpio_to_slice_num(PROC_LED);

        /* Configure PWM for smooth brightness control */
        pwm_set_wrap(s_pwm_slice, 65535U);
        pwm_set_clkdiv(s_pwm_slice, 8.0f);
        pwm_set_enabled(s_pwm_slice, true);

        s_pwm_init = true;
        s_hb_start_ms = now_ms;
    }

    if (s_hb_start_ms == 0U) {
        s_hb_start_ms = now_ms;
    }

    /* Generate heartbeat breathing pattern using triangle wave */
    uint32_t elapsed = now_ms - s_hb_start_ms;
    uint32_t t = elapsed % s_standby_period_ms;
    uint32_t duty;

    if (t < half_period_ms) {
        /* Fade up brightness during first half */
        duty = (t * 65535U) / half_period_ms;
    } else {
        /* Fade down brightness during second half */
        duty = ((s_standby_period_ms - t) * 65535U) / half_period_ms;
    }

    /* Apply calculated duty cycle to PROC_LED */
    pwm_set_gpio_level(PROC_LED, (uint16_t)duty);
}