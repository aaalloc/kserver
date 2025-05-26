import socket
import threading
import time
import argparse

# Configuration
host = "your-server-host"  # Replace with your server host
port = 12345  # Replace with your server port
num_requests = 1000  # Total number of requests to send
num_threads = 50  # Number of threads to use for sending requests
requests_per_thread = num_requests // num_threads
# presnet in task.c in ww_call()
end_flag = "end___"


def send_request():
    response_times = []
    try:
        # Create a socket object
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Connect to the server
        s.connect((host, port))

        for _ in range(requests_per_thread):
            start_time = time.time()
            # Send data to the server
            s.sendall(b"Hello, server!")
            # Receive a response from the server
            response = s.recv(1024)
            if response.decode() == end_flag:
                end_time = time.time()
                response_times.append(end_time - start_time)

        # Close the connection
        s.close()
        print(
            f"Average response time: {sum(response_times) / len(response_times):.4f} seconds")
    except Exception as e:
        print(f"Request failed: {e}")


def stress_test():
    threads = []
    for i in range(num_threads):
        thread = threading.Thread(target=send_request)
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Stress test your server socket.")
    parser.add_argument("--host", type=str, default=host,
                        help="Host of the server to stress test")
    parser.add_argument("--port", type=int, default=port,
                        help="Port of the server to stress test")
    parser.add_argument("--num_requests", type=int,
                        default=num_requests, help="Total number of requests to send")
    parser.add_argument("--num_threads", type=int, default=num_threads,
                        help="Number of threads to use for sending requests")

    args = parser.parse_args()
    host = args.host
    port = args.port
    num_requests = args.num_requests
    num_threads = args.num_threads
    requests_per_thread = num_requests // num_threads

    print(
        f"Starting stress test on {host}:{port} with {num_requests} requests using {num_threads} threads...")
    start_time = time.time()
    stress_test()
    end_time = time.time()
    print(f"Stress test completed in {end_time - start_time:.2f} seconds.")
