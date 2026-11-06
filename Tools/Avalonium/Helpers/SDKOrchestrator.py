import socket
import time
import sys
import argparse

def send_command(host, port, command, timeout=120):
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(timeout)
        client.connect((host, port))
        client.sendall((command + "\n").encode())
        
        time.sleep(2)
        
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
    parser = argparse.ArgumentParser(description="Avalonium SDK / Reflection Kit Orchestrator")
    parser.add_argument("command", nargs="?", default="GENSDK",
                        help="Command to run: GENSDK (C++ SDK; Reflection lands beside it)")
    parser.add_argument("output_path", nargs="?", default="",
                        help="Optional custom output directory path")
    parser.add_argument("--host", default="localhost", help="Avalonium IOCP host (default: localhost)")
    parser.add_argument("--port", type=int, default=20030, help="Avalonium IOCP port (default: 20030)")
    parser.add_argument("--timeout", type=int, default=300, help="Socket timeout in seconds (default: 300)")

    args = parser.parse_args()

    # Handle legacy positional arguments if user passed host/port style:
    cmd_upper = args.command.upper()
    if cmd_upper not in ("GENSDK",) and "." in args.command:
        # User might have passed host as first argument: e.g. "127.0.0.1"
        host = args.command
        port = int(args.output_path) if args.output_path.isdigit() else args.port
        command = "GENSDK"
        output_arg = ""
    else:
        host = args.host
        port = args.port
        command = cmd_upper if cmd_upper in ("GENSDK",) else "GENSDK"
        output_arg = (" " + args.output_path) if args.output_path else ""

    print(f"Connecting to Avalonium at {host}:{port}")

    print("\nSending INIT...")
    response = send_command(host, port, "INIT", timeout=30)
    print(response)

    print(f"\nSending {command}{output_arg} (Writing SDK/Reflection output)...")
    response = send_command(host, port, f"{command}{output_arg}", timeout=args.timeout)
    print(response)
