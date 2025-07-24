from bcc import BPF
import subprocess
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
program_worker_creation_measurement = """
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

program_worker_die_measurement = """
#include <uapi/linux/ptrace.h>

BPF_HASH(start, u32, u64);
BPF_HISTOGRAM(duration_us);

TRACEPOINT_PROBE(workqueue, workqueue_worker_dying_start) {
    u32 pid = args->pid;
    u64 ts = bpf_ktime_get_ns();
    start.update(&pid, &ts);
    return 0;
}

TRACEPOINT_PROBE(workqueue, workqueue_worker_dying_end) {
    u32 pid = args->pid;
    u64 *tsp = start.lookup(&pid);
    if (tsp == 0) {
        return 0;   // missed start
    }

    u64 delta = bpf_ktime_get_ns() - *tsp;
    u64 delta_us = delta / 1000;
    duration_us.increment(bpf_log2l(delta_us));

    start.delete(&pid);
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

def gen_hist_plot(buckets: list, counts: list, labels: list, type_measurement: str, save_plot: bool = False):
    # Affichage du graphique
    plt.figure(figsize=(10, 6))
    plt.bar(labels, counts, width=0.7)
    plt.xlabel("Latence (µs)")
    plt.ylabel("Nombre d'appels")
    plt.title("Histogramme des latences de create_worker")
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.grid(True, axis="y", linestyle="--", alpha=0.5)
    if save_plot:
        from datetime import datetime
        d = datetime.now().strftime('%Y%m%d_%H%M%S')
        fname = f"worker_{type_measurement}_histogram_{d}.png"
        plt.savefig(fname)
        print(f"Histogram plot saved as '{fname}'")
    plt.show()
    

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
        "--type-measurement",
        choices=["creation", "die"],
        default="creation",
        help="Type of worker measurement: 'creation' (default) or 'die'"
    )

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

    parser.add_argument(
        "--save-json",
        type=Path,
        help="Save histogram data to a JSON file"
    )
    
    parser.add_argument(
        "--load-json", 
        type=Path, 
        help="Load histogram data from JSON file instead of tracing"
    )
    
    parser.add_argument(
        "--save-plot",
        action="store_true",
        default=False,
        help="Save the histogram plot as a PNG file"
    )
    
    args = parser.parse_args()

    if args.load_json:
        import json
        with args.load_json.open("r") as f:
            data = json.load(f)
        buckets = data["buckets"]
        counts = data["counts"]
        labels = data["labels"]
        gen_hist_plot(buckets, counts, labels, args.type_measurement, args.save_plot)
        exit(0)
    else:
        b: BPF = None
        match args.type_measurement:
            case "creation":
                b = BPF(text=program_worker_creation_measurement)
                b.attach_kprobe(event="create_worker", fn_name="trace_start")
                b.attach_kretprobe(event="create_worker", fn_name="trace_end")

                insert_module(delay=args.delay, nr_work_max=args.nr_work_max)
                remove_module()
            case 'die':
                b = BPF(text=program_worker_die_measurement)
                try:
                    print("Tracing workqueue_worker_dying_start/end... Hit Ctrl-C to end.")
                    b.trace_print(fmt="pid {1}, msg = {5}")
                except KeyboardInterrupt:
                    print("Tracing stopped by user.")
                
        
        b["dist"].print_log2_hist("us")
        hist = b.get_table("dist")
        

        buckets = []
        counts = []
        for k, v in sorted(hist.items(), key=lambda kv: kv[0].value):
            if v.value == 0:
                continue
            buckets.append(k.value)
            counts.append(v.value)
        # Convertit les buckets log2 -> valeurs réelles en microsecondes
        labels = [f"{1 << b} µs" for b in buckets]
        
        if args.save_json:
            import json
            data = {"buckets": buckets, "counts": counts, "labels": labels}
            with args.save_json.open("w") as f:
                json.dump(data, f)
            print(f"Histogram data saved to {args.save_json}")

        gen_hist_plot(buckets, counts, labels, args.type_measurement, args.save_plot)