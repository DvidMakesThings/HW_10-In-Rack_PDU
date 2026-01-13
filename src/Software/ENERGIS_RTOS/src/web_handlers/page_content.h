/**
 * @file src/web_handlers/page_content.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui04 4. Page Content
 * @ingroup webhandlers
 * @brief HTML page content storage and retrieval for web interface
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details
 * This module provides a simple routing layer for HTML page content stored in flash memory.
 * It maps HTTP request paths to appropriate HTML content arrays and returns their sizes.
 *
 * Storage Strategy:
 * - Frequently accessed pages (control, settings, help) stored as gzip-compressed blobs
 * - Infrequently accessed pages (manuals) stored as plain HTML strings
 * - All content stored in flash memory to conserve RAM
 * - Gzip compression reduces transfer size and mitigates W5500 TX buffer constraints
 *
 * Supported Pages:
 * - Control page (control.html, root /) - Main relay control interface (gzipped)
 * - Settings page (settings.html) - Network and device configuration (gzipped)
 * - Help page (help.html) - Quick reference and links (gzipped)
 * - User Manual (user_manual.html) - PDF iframe embed (plain HTML)
 * - Automation Manual (automation_manual.html) - PDF iframe embed (plain HTML)
 *
 * Usage Pattern:
 * 1. HTTP server calls get_page_content() to retrieve pointer to HTML data
 * 2. HTTP server calls get_page_length() to determine Content-Length and encoding
 * 3. HTTP server sends appropriate Content-Encoding header based on gzip flag
 * 4. Content is transmitted using chunked send to avoid buffer overflow
 *
 * @note All page content is const and stored in flash; no runtime modifications.
 * @note Gzipped pages use external const arrays from html/*_gz.h headers.
 * @note Default fallback is control.html for unrecognized paths.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PAGE_CONTENT_H
#define PAGE_CONTENT_H

#include "../CONFIG.h"

/* HTML page declarations */
/** @name Content Declarations
 * @{
 */
extern const char control_html[];
extern const char settings_html[];
extern const char user_manual_html[];
extern const char automation_manual_html[];
/** @} */

/** @name Public API
 * @{
 */
/**
 * @brief Route HTTP request to appropriate HTML page content.
 *
 * Maps HTTP GET request paths to corresponding HTML content stored in flash memory.
 * Supports both plain HTML strings and gzip-compressed blobs. Always returns a valid
 * pointer (defaults to control page for unrecognized paths).
 *
 * Routing Table:
 * - GET /settings.html → settings_gz (gzipped)
 * - GET /help.html → help_gz (gzipped)
 * - GET /user_manual.html → user_manual_html (plain HTML)
 * - GET /automation_manual.html → automation_manual_html (plain HTML)
 * - GET /control.html → control_gz (gzipped)
 * - GET / → control_gz (gzipped, default)
 * - All other paths → control_gz (gzipped, fallback)
 *
 * @param request HTTP request line (e.g., "GET /control.html HTTP/1.1")
 * @return Pointer to HTML content in flash memory (never NULL)
 *
 * @note Returned pointer is const and points to flash memory.
 * @note Use get_page_length() to determine content size and encoding.
 */
const char *get_page_content(const char *request);

/**
 * @brief Get content length and encoding for a requested HTML page.
 *
 * Determines the byte length of the HTML content for a given HTTP request path
 * and indicates whether the content is gzip-compressed. This information is used
 * by the HTTP server to set Content-Length and Content-Encoding headers.
 *
 * Length Calculation:
 * - Gzipped pages: Uses precompiled *_gz_len constants from header files
 * - Plain HTML pages: Uses strlen() to measure string length
 * - Default fallback: Returns control_gz length
 *
 * @param request HTTP request line (e.g., "GET /help.html HTTP/1.1")
 * @param is_gzip Output flag indicating compression; set to 1 if gzipped, 0 if plain HTML (may be
 * NULL)
 *
 * @return Content length in bytes (always positive)
 *
 * @note If is_gzip is NULL, the function still works but does not report encoding.
 * @note Always returns a valid length; falls back to control_gz on unrecognized paths.
 */
int get_page_length(const char *request, int *is_gzip);
/** @} */
#endif // PAGE_CONTENT_H

/** @} */