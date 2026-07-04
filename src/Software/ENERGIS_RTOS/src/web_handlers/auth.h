/**
 * @file src/web_handlers/auth.h
 * @brief HTTP session authentication for ENERGIS PDU web interface.
 *
 * Provides cookie-based session authentication with configurable password
 * and 10-minute sliding expiry. Password and enable flag are persisted
 * in EEPROM via the StorageTask preferences system.
 */

#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>

/* auth_config_t is defined in EEPROM_MemoryMap.h (included via CONFIG.h) */

/** Session expiry time in milliseconds (10 minutes). */
#define AUTH_SESSION_EXPIRY_MS (10 * 60 * 1000)

/** Session token length (hex string, 16 chars + null). */
#define AUTH_TOKEN_LEN 16

/**
 * @brief Initialize the auth module. Call once at startup.
 *
 * Loads auth config from EEPROM. If EEPROM is empty/corrupt,
 * applies defaults (enabled=1, password="admin").
 */
void auth_init(void);

/**
 * @brief Check if authentication is enabled.
 * @return true if password protection is active.
 */
bool auth_is_enabled(void);

/**
 * @brief Check if an HTTP request carries a valid session cookie.
 *
 * Parses the Cookie header for "session=TOKEN" and validates against
 * the active session. Refreshes the 10-minute expiry on success.
 *
 * @param http_request Full HTTP request buffer (headers + body).
 * @return true if request is authenticated (or auth is disabled).
 */
bool auth_check_request(const char *http_request);

/**
 * @brief Attempt login with the given password.
 *
 * On success, generates a new session token and starts the 10-minute timer.
 *
 * @param password Candidate password string.
 * @return Pointer to session token string on success, NULL on failure.
 *         The returned pointer is valid until the next login call.
 */
const char *auth_login(const char *password);

/**
 * @brief Invalidate the current session (logout).
 */
void auth_logout(void);

/**
 * @brief Get the current auth configuration.
 * @param out Pointer to auth_config_t to fill.
 */
void auth_get_config(auth_config_t *out);

/**
 * @brief Update auth configuration and persist to EEPROM.
 *
 * @param enabled 1 to enable, 0 to disable password protection.
 * @param new_password New password (NULL to keep current). Max 31 chars.
 * @return true on success.
 */
bool auth_set_config(uint8_t enabled, const char *new_password);

/**
 * @brief Handle POST /api/auth (login endpoint).
 * @param sock W5500 socket number.
 * @param body URL-encoded POST body.
 */
void handle_auth_login(uint8_t sock, char *body);

/**
 * @brief Handle POST /api/auth/logout.
 * @param sock W5500 socket number.
 */
void handle_auth_logout(uint8_t sock);

/**
 * @brief Handle GET /api/auth/status.
 * @param sock W5500 socket number.
 * @param http_request Full HTTP request for cookie checking.
 */
void handle_auth_status(uint8_t sock, const char *http_request);

/**
 * @brief Handle POST /api/auth/config (change password / toggle).
 * @param sock W5500 socket number.
 * @param body URL-encoded POST body.
 * @param http_request Full HTTP request for session validation.
 */
void handle_auth_config(uint8_t sock, char *body, const char *http_request);

/**
 * @brief Send the login page HTML response.
 * @param sock W5500 socket number.
 */
void auth_send_login_page(uint8_t sock);

#endif /* AUTH_H */
