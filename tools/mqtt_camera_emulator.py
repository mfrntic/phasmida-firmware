#!/usr/bin/env python3
"""MQTT camera emulator for testing command/ack flow.

Emulates the timer_camera MQTT behavior for command topic handling:
- Subscribes to: phasmida/<slug>/cmd
- Publishes ACK to: phasmida/<slug>/cmd/ack
- Supports: set-camera-quality

Example:
python tools/mqtt_camera_emulator.py --slug 3c8a1fd7851c
"""

import argparse
import json
import os
import re
import signal
import sys
import time
import uuid
from dataclasses import dataclass

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Missing dependency: paho-mqtt")
    print("Install with: pip install paho-mqtt")
    sys.exit(2)


VALID_FRAME_SIZES = {5, 8, 9, 10}  # QVGA, VGA, SVGA, XGA (esp32-camera enum values)


@dataclass
class CameraState:
    jpeg_quality: int = 12
    frame_size: int = 9


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Emulate timer_camera MQTT command handler")
    parser.add_argument("--host", default="api.phasmida.eu", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--slug", required=True, help="MAC slug, 12 lowercase hex chars")
    parser.add_argument(
        "--api-key",
        default=None,
        help="MQTT API key (required if --password not provided; can also use PHASMIDA_MQTT_API_KEY env)",
    )
    parser.add_argument("--username", default=None, help="MQTT username override (default: slug)")
    parser.add_argument("--password", default=None, help="MQTT password override (default: api-key)")
    parser.add_argument("--client-id", default=None, help="MQTT clientId override (default: phasmida-<slug>)")
    parser.add_argument("--qos", type=int, default=1, choices=[0, 1], help="MQTT QoS for subscribe/publish")
    parser.add_argument("--keepalive", type=int, default=30, help="MQTT keepalive seconds")
    return parser.parse_args()


def now_ms() -> int:
    return int(time.time() * 1000)


def main() -> int:
    args = parse_args()

    if not re.fullmatch(r"[0-9a-f]{12}", args.slug):
        print("Invalid --slug. Expected 12 lowercase hex chars, e.g. 64e833123abc")
        return 2

    username = args.username if args.username is not None else args.slug
    env_api_key = os.getenv("PHASMIDA_MQTT_API_KEY")
    api_key = args.api_key if args.api_key is not None else env_api_key
    password = args.password if args.password is not None else api_key
    client_id = args.client_id if args.client_id is not None else f"phasmida-{args.slug}"

    if password is None or not str(password).strip():
        print("Missing MQTT credential: provide --password, --api-key, or PHASMIDA_MQTT_API_KEY")
        return 2

    cmd_topic = f"phasmida/{args.slug}/cmd"
    ack_topic = f"phasmida/{args.slug}/cmd/ack"

    state = CameraState()
    shutdown = {"value": False}

    def log(msg: str) -> None:
        print(msg, flush=True)

    try:
        client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
    except AttributeError:
        client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)

    client.username_pw_set(username=username, password=password)

    def publish_ack(cmd_id: str, status: str, error_code: str = None, result: dict = None) -> None:
        ack = {
            "cmdId": cmd_id,
            "status": status,
            "ts": now_ms(),
        }
        if error_code is not None:
            ack["error"] = {"code": error_code}
        if result is not None:
            ack["result"] = result

        payload = json.dumps(ack, separators=(",", ":"))
        client.publish(ack_topic, payload, qos=args.qos, retain=False)
        log(f"[EMULATOR] ACK -> {payload}")

    def on_connect(c: mqtt.Client, _userdata, _flags, rc, _properties=None):
        rc_value = getattr(rc, "value", rc)
        if rc_value == 0 or str(rc).lower() == "success":
            log(f"[EMULATOR] Connected to {args.host}:{args.port} as clientId='{client_id}' username='{username}'")
            c.subscribe(cmd_topic, qos=args.qos)
            log(f"[EMULATOR] Subscribed: {cmd_topic}")
        else:
            log(f"[EMULATOR] Connect failed: rc={rc}")
            if rc_value == 5 or "not authorized" in str(rc).lower():
                log("[EMULATOR] Broker denied auth (rc=5). Check ACL/registration for this slug.")

    def on_message(_c: mqtt.Client, _userdata, msg: mqtt.MQTTMessage):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
            cmd = json.loads(payload)
        except Exception as ex:
            log(f"[EMULATOR] Invalid JSON on {msg.topic}: {ex}")
            return

        cmd_id = str(cmd.get("cmdId", ""))
        cmd_type = str(cmd.get("type", ""))
        log(f"[EMULATOR] CMD <- topic={msg.topic} payload={payload}")

        if not cmd_id or not cmd_type:
            log("[EMULATOR] CMD invalid: missing cmdId/type")
            return

        if cmd_type != "set-camera-quality":
            publish_ack(cmd_id, "rejected", error_code="unsupported_command")
            return

        params = cmd.get("params", {}) if isinstance(cmd.get("params", {}), dict) else {}
        jpeg_quality = int(params.get("jpegQuality", -1))
        frame_size = int(params.get("frameSize", -1))

        if jpeg_quality < 0 or jpeg_quality > 63:
            publish_ack(cmd_id, "rejected", error_code="invalid_jpeg_quality")
            return

        if frame_size not in VALID_FRAME_SIZES:
            publish_ack(cmd_id, "rejected", error_code="invalid_frame_size")
            return

        state.jpeg_quality = jpeg_quality
        state.frame_size = frame_size

        publish_ack(
            cmd_id,
            "ok",
            result={
                "jpegQuality": state.jpeg_quality,
                "frameSize": state.frame_size,
                "appliedAt": now_ms(),
            },
        )
        log(f"[EMULATOR] Applied quality={state.jpeg_quality} frameSize={state.frame_size}")

    client.on_connect = on_connect
    client.on_message = on_message

    def stop_handler(_sig, _frame):
        shutdown["value"] = True

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)

    client.connect(args.host, args.port, keepalive=args.keepalive)
    client.loop_start()

    log("[EMULATOR] Running. Press Ctrl+C to stop.")
    while not shutdown["value"]:
        time.sleep(0.1)

    log("[EMULATOR] Stopping...")
    client.loop_stop()
    client.disconnect()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
