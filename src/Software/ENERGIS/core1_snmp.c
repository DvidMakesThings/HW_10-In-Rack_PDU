#include "core1_task.h"
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

void handle_snmp_request(netsnmp_session *session);

void core1_task(void) {
    char header[128];
    int header_len;
    uint16_t http_port = 80;
    uint16_t snmp_port = 161;
    char request_buffer[256];

    uint8_t http_server_socket = 0;

    // Initialize HTTP socket
    while (socket(http_server_socket, Sn_MR_TCP, http_port, 0) != http_server_socket) {
        ERROR_PRINT("HTTP Socket creation failed. Retrying...\n");
        sleep_ms(500);
    }

    if (listen(http_server_socket) != SOCK_OK) {
        ERROR_PRINT("HTTP Listen failed\n");
        close(http_server_socket);
        return;
    }

    INFO_PRINT("Ethernet HTTP server started on port %d\n", http_port);

    // Initialize SNMP
    netsnmp_session session, *ss;
    init_snmp("snmpd");
    snmp_sess_init(&session);
    session.peername = strdup("localhost");
    session.version = SNMP_VERSION_1;
    session.community = (u_char *)"public";
    session.community_len = strlen((char *)session.community);
    ss = snmp_open(&session);

    if (!ss) {
        snmp_perror("snmp_open");
        return;
    }

    uint8_t last_http_status = 0xFF;

    while (1) {
        // Handle HTTP server
        uint8_t http_status = getSn_SR(http_server_socket);

        if (http_status != last_http_status) {
            DEBUG_PRINT("HTTP Socket Status Changed: 0x%02X\n", http_status);
            last_http_status = http_status;
        }

        switch (http_status) {
        case SOCK_ESTABLISHED:
            INFO_PRINT("HTTP Client connected.\n");

            if (getSn_IR(http_server_socket) & Sn_IR_CON) {
                setSn_IR(http_server_socket, Sn_IR_CON);
                DEBUG_PRINT("HTTP Connection established flag cleared.\n");
            }

            int32_t recv_len =
                recv(http_server_socket, (uint8_t *)request_buffer, sizeof(request_buffer) - 1);
            if (recv_len > 0) {
                request_buffer[recv_len] = '\0'; // Null-terminate request
                DEBUG_PRINT("HTTP Request: %s\n", request_buffer);

                const char *page_content = get_page_content(request_buffer);
                int page_size = strlen(page_content);

                header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: %d\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      page_size);

                INFO_PRINT("Sending HTTP response. Page size: %d bytes\n", page_size);

                send(http_server_socket, (uint8_t *)header, header_len);
                send(http_server_socket, (uint8_t *)page_content, page_size);
                close(http_server_socket);
                INFO_PRINT("HTTP Connection closed. Waiting for next request...\n");

                // Ensure socket is released before reinitialization
                sleep_ms(50);

                // Reinitialize socket
                while (socket(http_server_socket, Sn_MR_TCP, http_port, 0) != http_server_socket) {
                    ERROR_PRINT("HTTP Socket re-initialization failed. Retrying...\n");
                    sleep_ms(500);
                }

                listen(http_server_socket);
            }
            break;

        case SOCK_CLOSE_WAIT:
            INFO_PRINT("HTTP Client disconnected, waiting for final data...\n");

            // Ensure all data is received before closing
            while (recv(http_server_socket, (uint8_t *)request_buffer, sizeof(request_buffer)) > 0)
                ;

            INFO_PRINT("Closing HTTP socket...\n");
            close(http_server_socket);

            sleep_ms(50); // Short delay to ensure socket is fully released

            // Reinitialize socket
            while (socket(http_server_socket, Sn_MR_TCP, http_port, 0) != http_server_socket) {
                ERROR_PRINT("HTTP Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(http_server_socket);
            break;

        case SOCK_CLOSED:
            WARNING_PRINT("HTTP Socket unexpectedly closed, restarting...\n");

            // Ensure socket is fully closed before reinitializing
            sleep_ms(50);

            while (socket(http_server_socket, Sn_MR_TCP, http_port, 0) != http_server_socket) {
                ERROR_PRINT("HTTP Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(http_server_socket);
            break;

        default:
            break; // Avoid log spam
        }

        // Handle SNMP requests
        handle_snmp_request(ss);

        sleep_ms(10);
    }

    snmp_close(ss);
}

void handle_snmp_request(netsnmp_session *session) {
    netsnmp_pdu *pdu, *response;
    int status;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, OID_LENGTH(anOID));

    status = snmp_synch_response(session, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        for (netsnmp_variable_list *vars = response->variables; vars; vars = vars->next_variable) {
            print_variable(vars->name, vars->name_length, vars);
        }
    } else {
        if (status == STAT_SUCCESS)
            fprintf(stderr, "Error in packet\nReason: %s\n", snmp_errstring(response->errstat));
        else if (status == STAT_TIMEOUT)
            fprintf(stderr, "Timeout: No response from %s.\n", session->peername);
        else
            snmp_sess_perror("snmp_synch_response", session);
    }

    if (response)
        snmp_free_pdu(response);
}