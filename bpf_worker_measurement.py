from bcc import BPF
import subprocess
import argparse
import os
import sys
from pathlib import Path
program = """
#include <uapi/linux/ptrace.h>

BPF_HASH(start, u32, u64);
BPF_HISTOGRAM(dist);

int trace_start(struct pt_regs *ctx) {
    u32 tid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    start.update(&tid, &ts);
    return 0;
}

int trace_end(struct pt_regs *ctx) {
    u32 tid = bpf_get_current_pid_tgid();
    u64 *tsp = start.lookup(&tid);
    if (tsp == 0) {
        return 0;
    }

    u64 delta = bpf_ktime_get_ns() - *tsp;
    dist.increment(bpf_log2l(delta / 1000));  // convert to us
    start.delete(&tid);
    return 0;
}
"""

module_name = Path("wq_new_worker.ko")


def insert_module(delay=100, nr_work_max=500):
    try:
        subprocess.run(['sudo', 'insmod', module_name,
                        f'delay={delay}', f'nr_work_max={nr_work_max}'],
                       check=True)
        print(f"Module '{module_name}' inserted successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error inserting module: {e}")


def remove_module():
    try:
        subprocess.run(['sudo', 'rmmod', module_name.stem], check=True)
        print(f"Module '{module_name}' removed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error removing module: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Trace create_worker latency and manage kernel module.")
    # parser.add_argument(
    #     "--module-path",
    #     type=lambda p: Path(p) if Path(p).is_file() else parser.error(
    #         f"File '{p}' does not exist."),
    #     required=True,
    #     help="Path to the kernel module to insert"
    parser.add_argument(
        "--delay",
        type=int,
        default=100,
        help="Delay value to pass to the kernel module (default: 100)"
    )
    parser.add_argument(
        "--nr-work-max",
        type=int,
        default=500,
        help="nr_work_max value to pass to the kernel module (default: 500)"
    )

    args = parser.parse_args()

    b: BPF = BPF(text=program)
    b.attach_kprobe(event="create_worker", fn_name="trace_start")
    b.attach_kretprobe(event="create_worker", fn_name="trace_end")

    print("Tracing create_worker... Hit Ctrl-C to end.")

    insert_module(delay=args.delay, nr_work_max=args.nr_work_max)

    print("\nLatency histogram:")
    b["dist"].print_log2_hist("us")

    remove_module()
