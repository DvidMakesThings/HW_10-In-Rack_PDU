/**
 * @file src/web_handlers/page_content.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui5 5. Page Content
 * @ingroup webhandlers
 * @brief HTML page content storage and retrieval
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Stores HTML pages as const char arrays in flash memory.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PAGE_CONTENT_H
#define PAGE_CONTENT_H

#include "../CONFIG.h"

/* HTML page declarations */
extern const char control_html[];
extern const char settings_html[];
extern const char user_manual_html[];
extern const char automation_manual_html[];

/**
 * @brief Gets the HTML content for a requested page
 * @param request The HTTP request line (e.g., "GET /api/control.html HTTP/1.1")
 * @return Pointer to the HTML content, or control.html as default
 * @note Returns appropriate HTML page based on request path
 */
const char *get_page_content(const char *request);

/**
 * @brief Gets the content length for a requested HTML page.
 *
 * @param request  HTTP request line (for example, "GET /help.html HTTP/1.1").
 * @param is_gzip  Optional output flag; set to 1 if the selected page is gzipped, 0 otherwise.
 *
 * @return Content length in bytes for the selected page.
 *
 * @details
 * - Uses strlen() for pages stored as standard C strings.
 * - Uses sizeof() for the Help page, which is stored as a gzip-compressed blob.
 * - Falls back to the control page if the request does not match any known HTML endpoint.
 */
int get_page_length(const char *request, int *is_gzip);

/**
 * @brief Gets the content length for a requested HTML page.
 *
 * @param request  HTTP request line (for example, "GET /help.html HTTP/1.1").
 * @param is_gzip  Optional output flag; set to 1 if the selected page is gzipped, 0 otherwise.
 *
 * @return Content length in bytes for the selected page.
 *
 * @details
 * - Uses strlen() for pages stored as standard C strings.
 * - Uses help_gz_len for the Help page, which is stored as a gzip-compressed blob.
 * - Falls back to the control page if the request does not match any known HTML endpoint.
 */
int get_page_length(const char *request, int *is_gzip);
#endif // PAGE_CONTENT_H

/** @} */