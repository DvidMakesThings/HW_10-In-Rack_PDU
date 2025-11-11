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
 * Pages should be embedded directly from your HTML files.
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
extern const char help_html[];
extern const char user_manual_html[];
extern const char programming_manual_html[];

/**
 * @brief Gets the HTML content for a requested page
 * @param request The HTTP request line (e.g., "GET /api/control.html HTTP/1.1")
 * @return Pointer to the HTML content, or control.html as default
 * @note Returns appropriate HTML page based on request path
 */
const char *get_page_content(const char *request);

#endif // PAGE_CONTENT_H

/** @} */