/**
 * @file control_handler.h
 * @author DvidMakesThings - David Sipos
 * @brief Helper functions for handling form data in HTTP requests.
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef FORM_HELPERS_H
#define FORM_HELPERS_H

#include <stddef.h>

/**
 * @brief Extract key=value from URL‐encoded body (in‐place), returns NULL if missing.
 * @param c The hexadecimal character.
 * @return The integer value of the character, or -1 if invalid.
 */
char *get_form_value(const char *body, const char *key);

/**
 * @brief Decode '+'→' ' and "%XX"→char, in‐place.
 * @param c The hexadecimal character.
 */
void urldecode(char *s);

#endif // FORM_HELPERS_H
