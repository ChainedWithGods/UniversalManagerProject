#!/usr/bin/env python3
import socket
import sys
import argparse

HOST = '127.0.0.1'
PORT = 20030
BUFFER_SIZE = 65536
DEFAULT_TIMEOUT = 30.0


def configure_stdio():

    for stream_name in ("stdout", "stderr"):
        stream = getattr(sys, stream_name, None)
        if stream is not None and hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")


def decode_response(data):
    for encoding in ("utf-8", "mbcs"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("latin-1", errors="replace")

def send_command(command, timeout=DEFAULT_TIMEOUT):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        
        sock.connect((HOST, PORT))
        
        sock.sendall((command + "\n").encode('utf-8'))
        
        response = b''
        while True:
            chunk = sock.recv(BUFFER_SIZE)
            if not chunk:
                break
            response += chunk
            if response.endswith(b'\n'):
                break
        
        sock.close()
        return decode_response(response)
        
    except socket.timeout:
        return "ERROR: Connection timeout (increase timeout with --timeout)\n"
    except ConnectionRefusedError:
        return "ERROR: Connection refused. is the DLL injected? Maybe the game is not running?\n"
    except Exception as e:
        return f"ERROR: {str(e)}\n"

def main():
    configure_stdio()
    parser = argparse.ArgumentParser(
        description='Avalonium TCP Client',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    
    parser.add_argument('command', nargs='+', help='Command and arguments to send')
    parser.add_argument('-o', '--output', help='Save output to file')
    parser.add_argument('-q', '--quiet', action='store_true', help='Suppress header output')
    parser.add_argument('-t', '--timeout', type=float, default=DEFAULT_TIMEOUT, 
                       help=f'Socket timeout in seconds (default: {DEFAULT_TIMEOUT})')
    
    args = parser.parse_args()
    
    command = ' '.join(args.command)
    
    if not args.quiet:
        print(f"Sending: {command}")
        print("-" * 60)
    
    response = send_command(command, timeout=args.timeout)
    
    if args.output:
        try:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(response)
            if not args.quiet:
                print(f"Output saved to: {args.output}")
        except Exception as e:
            print(f"ERROR:Damn it, failed to write to file: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        print(response, end='')

if __name__ == '__main__':
    main()
