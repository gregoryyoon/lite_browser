// Copyright (c) 2026 Lite Browser. All rights reserved.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_MCP_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_MCP_H_

#include <windows.h>
#include <stddef.h>

// Initialize MCP Server connection / process
void mcp_server_init(void);

// Shutdown MCP Server process
void mcp_server_shutdown(void);

// Send JSON-RPC request to MCP Server and get response
int mcp_send_request(const char* json_req, char* out_resp, size_t max_len);

// List available MCP tools as JSON
void mcp_get_tools_json(char* out_buf, size_t max_len);

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_MCP_H_
