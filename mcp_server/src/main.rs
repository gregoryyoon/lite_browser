mod protocol;
mod tools;

use protocol::{JsonRpcError, JsonRpcRequest, JsonRpcResponse};
use serde_json::json;
use std::io::{self, BufRead, Write};

#[tokio::main]
async fn main() {
    let stdin = io::stdin();
    let mut stdout = io::stdout();

    for line in stdin.lock().lines() {
        match line {
            Ok(line_str) => {
                let trimmed = line_str.trim();
                if trimmed.is_empty() {
                    continue;
                }

                let req: Result<JsonRpcRequest, _> = serde_json::from_str(trimmed);
                let response = match req {
                    Ok(request) => handle_request(request).await,
                    Err(e) => JsonRpcResponse {
                        jsonrpc: "2.0".to_string(),
                        id: None,
                        result: None,
                        error: Some(JsonRpcError {
                            code: -32700,
                            message: format!("Parse error: {}", e),
                            data: None,
                        }),
                    },
                };

                if let Ok(resp_json) = serde_json::to_string(&response) {
                    let _ = writeln!(stdout, "{}", resp_json);
                    let _ = stdout.flush();
                }
            }
            Err(_) => break,
        }
    }
}

async fn handle_request(req: JsonRpcRequest) -> JsonRpcResponse {
    let method = req.method.as_str();
    let id = req.id;

    match method {
        "initialize" => JsonRpcResponse {
            jsonrpc: "2.0".to_string(),
            id,
            result: Some(json!({
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {
                        "listChanged": false
                    }
                },
                "serverInfo": {
                    "name": "lite-browser-mcp",
                    "version": "0.1.0"
                }
            })),
            error: None,
        },
        "notifications/initialized" => JsonRpcResponse {
            jsonrpc: "2.0".to_string(),
            id,
            result: Some(json!({})),
            error: None,
        },
        "tools/list" => {
            let tools_list = tools::get_tool_definitions();
            JsonRpcResponse {
                jsonrpc: "2.0".to_string(),
                id,
                result: Some(json!({
                    "tools": tools_list
                })),
                error: None,
            }
        }
        "tools/call" => {
            let params = req.params.unwrap_or(json!({}));
            let name = params.get("name").and_then(|v| v.as_str()).unwrap_or("");
            let arguments = params.get("arguments").cloned();

            let tool_result = tools::execute_tool(name, arguments).await;
            JsonRpcResponse {
                jsonrpc: "2.0".to_string(),
                id,
                result: Some(json!(tool_result)),
                error: None,
            }
        }
        "ping" => JsonRpcResponse {
            jsonrpc: "2.0".to_string(),
            id,
            result: Some(json!({"status": "pong"})),
            error: None,
        },
        _ => JsonRpcResponse {
            jsonrpc: "2.0".to_string(),
            id,
            result: None,
            error: Some(JsonRpcError {
                code: -32601,
                message: format!("Method not found: {}", method),
                data: None,
            }),
        },
    }
}
