import asyncio
import base64
import json
from pathlib import Path
from typing import Dict, Any

import requests

HOST = "0.0.0.0"
PORT = 5418
OPENAI_BASE = "https://api.chatanywhere.tech/v1/chat/completions"
OPENAI_KEY = "sk-lBmOxRXJ0Yrk5fNsfDF39DaD0uo3tCv8cQCQmZPHkbPKrJug"
OPENAI_MODEL = "gpt-4o-mini"
PROMPT_FILE = Path(__file__).with_name("prompt.json")
AUDIO_DIR = Path(__file__).with_name("audio_cache")
AUDIO_DIR.mkdir(exist_ok=True)

latest_state: Dict[str, Any] = {
    "online": False,
    "satiety": 0,
    "mood": 50,
    "statusText": "未连接ESP32",
}


def load_prompt() -> str:
    if PROMPT_FILE.exists():
        return PROMPT_FILE.read_text(encoding="utf-8").strip()
    default_prompt = '{"system":"你是千早爱音风格的陪伴AI，回复简短温柔。"}'
    PROMPT_FILE.write_text(default_prompt, encoding="utf-8")
    return default_prompt


def call_llm(text: str) -> str:
    prompt = load_prompt()
    payload = {
        "model": OPENAI_MODEL,
        "messages": [
            {"role": "system", "content": prompt},
            {"role": "user", "content": text},
        ],
        "temperature": 0.7,
    }
    resp = requests.post(
        OPENAI_BASE,
        headers={"Authorization": f"Bearer {OPENAI_KEY}", "Content-Type": "application/json"},
        json=payload,
        timeout=30,
    )
    resp.raise_for_status()
    return resp.json()["choices"][0]["message"]["content"]


def fake_stt(_: bytes) -> str:
    # 保持服务端轻量：可替换为 whisper/faster-whisper。
    return "千早爱音，今天怎么样？"


def fake_tts(text: str) -> bytes:
    # 简化实现：返回 UTF-8 文本 bytes，ESP32 端可按需替换为真实 TTS 音频播放。
    return text.encode("utf-8")


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    peer = writer.get_extra_info("peername")
    try:
        while not reader.at_eof():
            line = await reader.readline()
            if not line:
                break
            msg = json.loads(line.decode("utf-8").strip())
            mtype = msg.get("type")

            if mtype == "heartbeat":
                latest_state["online"] = True
                latest_state["statusText"] = "ESP32在线"
                writer.write(b'{"ok":true}\n')
                await writer.drain()

            elif mtype == "status":
                latest_state["online"] = True
                latest_state["satiety"] = int(msg.get("satiety", 0))
                latest_state["mood"] = int(msg.get("mood", 50))
                writer.write(b'{"ok":true}\n')
                await writer.drain()

            elif mtype == "event":
                latest_state["statusText"] = f"事件:{msg.get('name', 'unknown')}"
                writer.write(b'{"ok":true}\n')
                await writer.drain()

            elif mtype == "audio_trigger":
                # 示例流程：实际项目中此处应接收10秒PCM/OPUS音频数据。
                stt_text = fake_stt(b"")
                if "千早爱音" in stt_text or "粉色奶龙" in stt_text:
                    ai_text = call_llm(stt_text)
                    tts_bin = fake_tts(ai_text)
                    writer.write((json.dumps({"type": "ai_reply", "text": ai_text, "audio_b64": base64.b64encode(tts_bin).decode("ascii")}) + "\n").encode("utf-8"))
                else:
                    writer.write((json.dumps({"type":"ai_reply","text":"未检测到关键词"}, ensure_ascii=False)+"\n").encode("utf-8"))
                await writer.drain()

            elif mtype == "app_get":
                writer.write((json.dumps({"type": "app_state", **latest_state}) + "\n").encode("utf-8"))
                await writer.drain()

    except Exception as exc:
        print(f"[{peer}] error: {exc}")
    finally:
        writer.close()
        await writer.wait_closed()


async def main():
    server = await asyncio.start_server(handle_client, HOST, PORT)
    addr = ", ".join(str(s.getsockname()) for s in server.sockets or [])
    print(f"ESP32 TCP server listening on {addr}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
