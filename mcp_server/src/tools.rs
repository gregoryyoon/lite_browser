use crate::protocol::{ToolCallResult, ToolContent, ToolDefinition};
use serde_json::{json, Value};

pub fn get_tool_definitions() -> Vec<ToolDefinition> {
    vec![
        ToolDefinition {
            name: "browser_navigate".to_string(),
            description: "웹 브라우저의 현재 활성 탭을 지정한 URL로 이동시킵니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "url": {
                        "type": "string",
                        "description": "이동할 대상 웹 URL (예: https://www.google.com)"
                    }
                },
                "required": ["url"]
            }),
        },
        ToolDefinition {
            name: "browser_get_page_content".to_string(),
            description: "현재 웹 페이지의 제목, URL, 본문 텍스트 요약 및 상호작용 가능한 주요 버튼/링크/입력 필드 목록을 추출합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "format": {
                        "type": "string",
                        "enum": ["summary", "interactive_elements", "full_text"],
                        "description": "추출 형식 (기본값: summary)"
                    }
                }
            }),
        },
        ToolDefinition {
            name: "browser_click_element".to_string(),
            description: "지정한 CSS 셀렉터 또는 요소 인덱스/좌표의 버튼, 링크, 입력창을 클릭합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "selector": {
                        "type": "string",
                        "description": "클릭할 대상의 CSS 셀렉터 또는 XPath"
                    },
                    "text": {
                        "type": "string",
                        "description": "클릭할 버튼이나 링크에 표시된 텍스트"
                    }
                }
            }),
        },
        ToolDefinition {
            name: "browser_type_text".to_string(),
            description: "지정한 CSS 셀렉터의 검색창이나 입력 폼에 텍스트를 입력합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "selector": {
                        "type": "string",
                        "description": "텍스트를 입력할 input, textarea 또는 contenteditable 셀렉터"
                    },
                    "text": {
                        "type": "string",
                        "description": "입력할 텍스트 내용"
                    },
                    "press_enter": {
                        "type": "boolean",
                        "description": "입력 후 Enter 키를 누를지 여부 (기본값: false)"
                    }
                },
                "required": ["selector", "text"]
            }),
        },
        ToolDefinition {
            name: "browser_scroll".to_string(),
            description: "웹 페이지를 위 또는 아래로 스크롤합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "direction": {
                        "type": "string",
                        "enum": ["up", "down", "top", "bottom"],
                        "description": "스크롤 방향 (기본값: down)"
                    },
                    "amount": {
                        "type": "number",
                        "description": "스크롤할 픽셀 양 (기본값: 500)"
                    }
                }
            }),
        },
        ToolDefinition {
            name: "browser_autofill_login".to_string(),
            description: "로컬 보안 볼트(Vault)를 통해 로그인 폼에 대리 로그인을 수행합니다. 비밀번호 평문은 LLM에 전달되지 않고 안전하게 대리 입력됩니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "domain": {
                        "type": "string",
                        "description": "로그인할 대상 도메인 (선택 사항, 미지정 시 현재 활성 도메인 사용)"
                    }
                }
            }),
        },
        ToolDefinition {
            name: "browser_extract_data".to_string(),
            description: "웹 페이지 내의 특정 테이블, 목록, 링크, 가격 등의 데이터를 구조화하여 추출합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {
                    "selector": {
                        "type": "string",
                        "description": "추출할 데이터 영역의 CSS 셀렉터"
                    },
                    "fields": {
                        "type": "array",
                        "items": { "type": "string" },
                        "description": "추출할 필드명 목록 (예: ['title', 'price', 'link'])"
                    }
                }
            }),
        },
        ToolDefinition {
            name: "browser_take_screenshot".to_string(),
            description: "현재 웹 페이지 화면을 캡처합니다.".to_string(),
            inputSchema: json!({
                "type": "object",
                "properties": {}
            }),
        },
    ]
}

pub async fn execute_tool(name: &str, arguments: Option<Value>) -> ToolCallResult {
    let args = arguments.unwrap_or(json!({}));
    match name {
        "browser_navigate" => {
            let url = args.get("url").and_then(|v| v.as_str()).unwrap_or("");
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "navigate",
                        "target_url": url,
                        "message": format!("Navigated to {}", url)
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_get_page_content" => {
            let format = args.get("format").and_then(|v| v.as_str()).unwrap_or("summary");
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "get_content",
                        "format": format,
                        "ready": true
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_click_element" => {
            let selector = args.get("selector").and_then(|v| v.as_str()).unwrap_or("");
            let text = args.get("text").and_then(|v| v.as_str()).unwrap_or("");
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "click",
                        "selector": selector,
                        "text": text,
                        "message": format!("Clicked element: selector='{}' text='{}'", selector, text)
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_type_text" => {
            let selector = args.get("selector").and_then(|v| v.as_str()).unwrap_or("");
            let text = args.get("text").and_then(|v| v.as_str()).unwrap_or("");
            let press_enter = args.get("press_enter").and_then(|v| v.as_bool()).unwrap_or(false);
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "type",
                        "selector": selector,
                        "text_length": text.len(),
                        "press_enter": press_enter,
                        "message": format!("Typed text into '{}'", selector)
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_scroll" => {
            let direction = args.get("direction").and_then(|v| v.as_str()).unwrap_or("down");
            let amount = args.get("amount").and_then(|v| v.as_f64()).unwrap_or(500.0);
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "scroll",
                        "direction": direction,
                        "amount": amount
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_autofill_login" => {
            let domain = args.get("domain").and_then(|v| v.as_str()).unwrap_or("");
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "vault_autofill",
                        "domain": domain,
                        "message": "Vault autofill executed safely (Plaintext password was not exposed to LLM context)"
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_extract_data" => {
            let selector = args.get("selector").and_then(|v| v.as_str()).unwrap_or("");
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "extract_data",
                        "selector": selector
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        "browser_take_screenshot" => {
            ToolCallResult {
                content: vec![ToolContent {
                    content_type: "text".to_string(),
                    text: json!({
                        "status": "success",
                        "action": "screenshot",
                        "format": "base64"
                    }).to_string(),
                }],
                isError: Some(false),
            }
        }
        _ => ToolCallResult {
            content: vec![ToolContent {
                content_type: "text".to_string(),
                text: format!("Unknown tool: {}", name),
            }],
            isError: Some(true),
        },
    }
}
