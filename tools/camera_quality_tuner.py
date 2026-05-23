#!/usr/bin/env python3
"""Send set-camera-quality MQTT command to timer_camera and print ACK.

Usage example:
python tools/camera_quality_tuner.py --slug 64e833123abc --jpeg 12 --frame-size 10
"""

import argparse
import json
import re
import sys
import time
import uuid

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Missing dependency: paho-mqtt")
    print("Install with: pip install paho-mqtt")
    sys.exit(2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Tune camera quality over MQTT")
    parser.add_argument("--host", default="api.phasmida.eu", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--slug", required=True, help="Camera MAC slug (e.g. 64e833123abc)")
    parser.add_argument("--role", choices=["backend", "device"], default="backend", help="Auth role to use. backend is correct for dispatching commands to the camera.")
    parser.add_argument("--api-key", default=None, help="Password override. For device role this is the camera api key; for backend role this should be MQTT_BACKEND_PASSWORD.")
    parser.add_argument("--username", default=None, help="MQTT username override")
    parser.add_argument("--password", default=None, help="Password override (same value as --api-key when omitted)")
    parser.add_argument("--client-id", default=None, help="MQTT clientId override")
    parser.add_argument("--jpeg", type=int, required=True, help="JPEG quality 0..63 (lower = better)")
    parser.add_argument("--frame-size", type=int, required=True, choices=[5, 8, 9, 10], help="Frame size enum: 5=QVGA, 8=VGA, 9=SVGA, 10=XGA")
    parser.add_argument("--sharpness", type=int, default=None, help="Sensor sharpness -2..2")
    parser.add_argument("--denoise", type=int, default=None, help="Sensor denoise 0..8")
    parser.add_argument("--lenc", type=int, choices=[0, 1], default=None, help="Lens correction flag (0/1)")
    parser.add_argument("--raw-gma", type=int, choices=[0, 1], default=None, help="Raw gamma flag (0/1)")
    parser.add_argument("--aec2", type=int, choices=[0, 1], default=None, help="AEC2 flag (0/1)")
    parser.add_argument("--wpc", type=int, choices=[0, 1], default=None, help="White pixel correction flag (0/1)")
    parser.add_argument("--bpc", type=int, choices=[0, 1], default=None, help="Black pixel correction flag (0/1)")
    parser.add_argument("--gain-ceiling", type=int, choices=[0, 1, 2, 3, 4, 5, 6], default=None, help="Gain ceiling enum 0..6")
    parser.add_argument("--timeout", type=float, default=6.0, help="Seconds to wait for ACK")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not re.fullmatch(r"[0-9a-f]{12}", args.slug):
        print("Invalid --slug. Expected 12 lowercase hex chars from device MAC slug, e.g. 64e833123abc")
        return 2

    if args.jpeg < 0 or args.jpeg > 63:
        print("--jpeg must be in range 0..63")
        return 2

    if args.sharpness is not None and (args.sharpness < -2 or args.sharpness > 2):
        print("--sharpness must be in range -2..2")
        return 2

    if args.denoise is not None and (args.denoise < 0 or args.denoise > 8):
        print("--denoise must be in range 0..8")
        return 2

    cmd_topic = f"phasmida/{args.slug}/cmd"
    ack_topic = f"phasmida/{args.slug}/cmd/ack"
    cmd_id = str(uuid.uuid4())

    payload = {
        "cmdId": cmd_id,
        "type": "set-camera-quality",
        "params": {
            "jpegQuality": args.jpeg,
            "frameSize": args.frame_size,
        },
    }

    optional_params = {
        "sharpness": args.sharpness,
        "denoise": args.denoise,
        "lenc": args.lenc,
        "rawGma": args.raw_gma,
        "aec2": args.aec2,
        "wpc": args.wpc,
        "bpc": args.bpc,
        "gainCeiling": args.gain_ceiling,
    }
    for key, value in optional_params.items():
        if value is not None:
            payload["params"][key] = value

    device_client_id = f"phasmida-{args.slug}"
    if args.role == "backend":
        username = args.username if args.username is not None else "phasmida"
        password = args.password if args.password is not None else args.api_key
        client_id = args.client_id if args.client_id is not None else f"phasmida-backend-tuner-{uuid.uuid4().hex[:8]}"
    else:
        username = args.username if args.username is not None else args.slug
        password = args.password if args.password is not None else args.api_key
        client_id = args.client_id if args.client_id is not None else f"phasmida-tuner-{args.slug}"

    if not password:
        if args.role == "backend":
            print("Backend role requires --password or --api-key set to MQTT_BACKEND_PASSWORD")
        else:
            print("Device role requires --password or --api-key set to the camera api key")
        return 2

    try:
        client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
    except AttributeError:
        # Backward compatibility with older paho-mqtt versions.
        client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)

    client.username_pw_set(username=username, password=password)

    result = {"ack": None, "connected": False}

    def on_connect(c, _userdata, _flags, rc, _properties=None):
        rc_value = getattr(rc, "value", rc)
        rc_text = str(rc)
        if rc_value == 0 or rc_text.lower() == "success":
            result["connected"] = True
            c.subscribe(ack_topic, qos=1)
            c.publish(cmd_topic, json.dumps(payload), qos=1, retain=False)
        else:
            print(f"MQTT connect failed rc={rc}")
            if rc_value == 5 or "not authorized" in rc_text.lower():
                print("Broker returned Not Authorized (rc=5).")
                print(f"Used clientId='{client_id}', username='{username}', cmd topic='{cmd_topic}'.")
                print("If camera is online, try stopping its MQTT session first or use a dedicated tester clientId allowed by broker ACL.")
                print("Also verify this slug is registered in backend/broker ACL.")

    def on_message(_c, _userdata, msg):
        try:
            ack = json.loads(msg.payload.decode("utf-8", errors="replace"))
        except Exception:
            return
        if ack.get("cmdId") == cmd_id:
            result["ack"] = ack

    client.on_connect = on_connect
    client.on_message = on_message

    print(f"Connecting to {args.host}:{args.port} as clientId='{client_id}', username='{username}'")
    if client_id == device_client_id:
        print("Warning: clientId matches the real camera clientId. This will disconnect the camera MQTT session.")

    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()

    deadline = time.time() + args.timeout
    while time.time() < deadline and result["ack"] is None:
        time.sleep(0.05)

    client.loop_stop()
    client.disconnect()

    if not result["connected"]:
        print("Not connected to broker")
        print("Try this equivalent command if mosquitto clients are installed:")
        print(
            "mosquitto_pub"
            f" -h {args.host} -p {args.port}"
            f" -i {client_id} -u {username} -P {password}"
            f" -t {cmd_topic} -q 1 -m '{json.dumps(payload)}'"
        )
        return 1

    if result["ack"] is None:
        print("No ACK received in time")
        print("Published:", json.dumps(payload))
        return 1

    print("ACK:", json.dumps(result["ack"], ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
