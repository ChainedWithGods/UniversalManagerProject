import socket
import time
import sys
import argparse

def send_command(host, port, command, timeout=30):
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(timeout)
        client.connect((host, port))
        client.sendall((command + "\n").encode())
        
        if command.startswith("USMAP"):
            time.sleep(2)
        else:
            time.sleep(0.5)
        
        response = b""
        while True:
            try:
                chunk = client.recv(4096)
                if not chunk:
                    break
                response += chunk
                time.sleep(0.1)
            except socket.timeout:
                break
        
        client.close()
        return response.decode()
    except Exception as e:
        return f"Error: {e}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Avalonium USMAP Generator Orchestrator")
    parser.add_argument("output_path", nargs="?", default="",
                        help="Optional custom output .usmap file path")
    parser.add_argument("--host", default="localhost", help="Avalonium IOCP host (default: localhost)")
    parser.add_argument("--port", type=int, default=20030, help="Avalonium IOCP port (default: 20030)")
    parser.add_argument("--timeout", type=int, default=120, help="Socket timeout in seconds (default: 120)")

    args = parser.parse_args()

    # Handle legacy positional arguments if user passed host/port style:
    if args.output_path and ("." in args.output_path and not args.output_path.lower().endswith(".usmap")):
        host = args.output_path
        port = args.port
        output_arg = ""
    else:
        host = args.host
        port = args.port
        output_arg = (" " + args.output_path) if args.output_path else ""

    print(f"Connecting to Avalonium at {host}:{port}")

    print("\nSending INIT...")
    response = send_command(host, port, "INIT", timeout=30)
    print(response)

    print(f"\nSending USMAP{output_arg}...")
    response = send_command(host, port, f"USMAP{output_arg}", timeout=args.timeout)
    print(response)
