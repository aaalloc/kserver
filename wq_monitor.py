from enum import Enum
import sys
from dataclasses import dataclass
import time
import drgn
from drgn.helpers.linux.cpumask import for_each_cpu
from drgn.helpers.linux.idr import *
from drgn.helpers.linux.cpumask import for_each_cpu, for_each_possible_cpu
from drgn.helpers.linux.list import list_for_each_entry, list_empty
from drgn.helpers.linux.percpu import per_cpu_ptr
from argparse import ArgumentParser
import signal
import pprint


def err(s):
    print(s, file=sys.stderr, flush=True)
    sys.exit(1)


def cpumask_str(cpumask):
    output = ""
    base = 0
    v = 0
    for cpu in for_each_cpu(cpumask[0]):
        while cpu - base >= 32:
            output += f'{hex(v)} '
            base += 32
            v = 0
        v |= 1 << (cpu - base)
    if v > 0:
        output += f'{v:08x}'
    return output.strip()


worker_pool_idr = prog['worker_pool_idr']
wq_unbound_cpumask = prog['wq_unbound_cpumask']
cpumask_str_len = len(cpumask_str(wq_unbound_cpumask))
workqueues = prog['workqueues']

# depending on kernel version, prog['POOL_BH'] might cause an error
try:
    POOL_BH = prog['POOL_BH']
except KeyError:
    # for older kernels, POOL_BH is not defined
    POOL_BH = 0


WQ_NAME_LEN = prog['WQ_NAME_LEN'].value_()
WQ_UNBOUND = prog['WQ_UNBOUND']


try:
    WQ_BH = prog['WQ_BH']
except KeyError:
    # for older kernels, WQ_BH is not defined
    WQ_BH = 0

WQ_ORDERED = prog['__WQ_ORDERED']
# work items started execution
PWQ_STAT_STARTED = prog['PWQ_STAT_STARTED']
# work items completed execution
PWQ_STAT_COMPLETED = prog['PWQ_STAT_COMPLETED']
PWQ_NR_STATS = prog['PWQ_NR_STATS']
pp = pprint.PrettyPrinter(indent=4)


max_pool_id_len = 0
max_ref_len = 0
for pi, pool in idr_for_each(worker_pool_idr):
    pool = drgn.Object(prog, 'struct worker_pool', address=pool)
    max_pool_id_len = max(max_pool_id_len, len(f'{pi}'))
    max_ref_len = max(max_ref_len, len(f'{pool.refcnt.value_()}'))


@dataclass
class Worker():
    _worker: object
    task: object
    cpu: int
    pid: int
    desc: str

    WORKER_DIE = prog['WORKER_DIE']
    WORKER_IDLE = prog['WORKER_IDLE']
    WORKER_PREP = prog['WORKER_PREP']
    WORKER_CPU_INTENSIVE = prog['WORKER_CPU_INTENSIVE']
    WORKER_UNBOUND = prog['WORKER_UNBOUND']
    WORKER_REBOUND = prog['WORKER_REBOUND']
    WORKER_NOT_RUNNING = prog['WORKER_NOT_RUNNING']

    def __init__(self, _worker):
        self._worker = _worker
        self.task = _worker.task
        if self.task:
            self.cpu = self.task.thread_info.cpu.value_()
        else:
            self.cpu = -1
        self.pid = self.task.pid.value_() if self.task else -1
        self.desc = _worker.desc.string_().decode() if _worker.desc else 'unknown'

    def busy_or_idle(self) -> str:
        # there is a union on struct worker, if entry then it is idle, if hentry then it is busy
        if self._worker.entry.address_of_() != 0:
            return 'idle'
        elif self._worker.hentry.address_of_() != 0:
            return 'busy'
        else:
            return 'unknown'

    def is_running(self) -> bool:
        if self._worker.flags & self.WORKER_IDLE:
            return False
        return True

    def __str__(self):
        s = f'{self._worker.desc.string_().decode()}[PID={self.pid:5}, ID={self._worker.id.value_()}] cpu={self.cpu:3} '
        # NOTE: might intersting for debug, https://elixir.bootlin.com/linux/v6.15/source/kernel/workqueue_internal.h#L31
        # s += f'{self._worker.current_work.address_of_().value_():#x} '
        s += f'Running={self.is_running()} '

        return s


@dataclass
class WorkerPool():
    _pool: object
    id: int
    flags: int
    refcnt: int
    nice: int
    nr_idle: int
    nr_workers: int
    cpu: int
    cpus: str
    bh: bool
    unbound: bool
    show_active: bool
    nr_running: int = -1
    filter_name: str = None

    def __init__(self, _pool, name: str = None, show_active: bool = False):
        self._pool = _pool
        self.id = _pool.id.value_()
        self.flags = _pool.flags.value_()
        self.refcnt = _pool.refcnt.value_()
        self.nice = _pool.attrs.nice.value_()
        self.nr_idle = _pool.nr_idle.value_()
        self.nr_workers = _pool.nr_workers.value_()
        self.cpu = _pool.cpu.value_() if _pool.cpu >= 0 else -1
        self.cpus = cpumask_str(_pool.attrs.cpumask)
        self.bh = _pool.flags & POOL_BH
        self.unbound = _pool.cpu < 0
        self.nr_running = _pool.nr_running
        self.show_active = show_active

        # if we want to have info only related to a specific workqueue
        self.filter_name = name

    def show_runner_info(self) -> str:
        s = ''
        # iterate over idle_list
        for worker in list_for_each_entry('struct worker', self._pool.workers.address_of_(), 'node'):
            if worker.desc.string_().decode() == self.filter_name:
                s += f' ├─{Worker(worker)}\n'
        return s

    def __str__(self):
        # there is no active workers
        if self.show_active and self.nr_workers - self.nr_idle == 0:
            return ""
        s = f'pool[{self.id:0{max_pool_id_len}}] flags=0x{self.flags:02x} ref={self.refcnt:{max_ref_len}} nice={self.nice:3} '
        s += f'idle/workers={self.nr_idle:3}/{self.nr_workers:3} '
        s += f'nr_running={self.nr_running}    '
        if self.cpu >= 0:
            s += f'cpu={self.cpu:3}'
            if self.bh:
                s += ' bh'
        else:
            s += f'cpus={self.cpus}'
            # if self.bh:
            #     s += ' bh'
        s += '\n'
        s += self.show_runner_info()
        return s


@dataclass
class WorkQueue():
    _wq: object
    _pools: list[object]
    pools: list[WorkerPool]
    flags: int
    name: str
    cpu_pwq: int
    dfl_pwq: int
    unbound_attrs: int
    _stats: object
    nr_active: int = 0

    def __init__(self, _wq):
        self._wq = _wq
        self.flags = _wq.flags.value_()
        self.name = _wq.name.string_().decode()
        self.cpu_pwq = _wq.cpu_pwq
        self.dfl_pwq = _wq.dfl_pwq
        self.unbound_attrs = _wq.unbound_attrs.value_()
        self._stats = [0] * PWQ_NR_STATS
        self._pools = []
        self.pools = []
        for pwq in list_for_each_entry('struct pool_workqueue', self._wq.pwqs.address_of_(), 'pwqs_node'):
            for i in range(PWQ_NR_STATS):
                self._stats[i] += int(pwq.stats[i])
            self.pools.append(WorkerPool(drgn.Object(
                prog, 'struct worker_pool', address=pwq.pool.value_()), self.name))
            # self._pools.append(drgn.Object(
            #     prog, 'struct worker_pool', address=pwq.pool.value_()))
            self.nr_active += pwq.nr_active.value_()

    def __str__(self):
        s = f'{self.name:{WQ_NAME_LEN}}\n'
        s += f' ├─flags=0x{self.flags:02x} type={wq_type_str(self._wq):4}\n'
        if verbose:
            for pool in self.pools:
                s += f' ├─{pool}\n'
        s += f' ├─nr_active={self.nr_active:8}\n'
        s += f' ├─nr_activev2={(self._stats[PWQ_STAT_STARTED] - self._stats[PWQ_STAT_COMPLETED]):8}\n'
        s += f' ├─started={self._stats[PWQ_STAT_STARTED]:8} '
        s += f'completed={self._stats[PWQ_STAT_COMPLETED]:8} '
        return s


wq_type_len = 9


def wq_type_str(wq):
    if wq.flags & WQ_BH:
        return f'{"bh":{wq_type_len}}'
    elif wq.flags & WQ_UNBOUND:
        if wq.flags & WQ_ORDERED:
            return f'{"ordered":{wq_type_len}}'
        else:
            if wq.unbound_attrs.affn_strict:
                return f'{"unbound,S":{wq_type_len}}'
            else:
                return f'{"unbound":{wq_type_len}}'
    else:
        return f'{"percpu":{wq_type_len}}'


exit_req = False
verbose = False


def sigint_handler(signr, frame):
    global exit_req
    exit_req = True


# TODO: TO FIX !!!!
def scenario_total_workers_per_cpu(iteration: int, filter_cpu: list[int] = None) -> dict[int, dict[str, int]]:
    """
    For each iteration, returns a dictionnary with CPU as key
    and a dictionary of idle, active, and total workers as values.

    Args:
        iteration (int): The current iteration number.
        filter_name (list[str], optional): A list of CPU IDs to filter the results. If None, all CPUs are included.

    Returns:
        results (dict[int, dict[str, int]]): A dictionary where keys are CPU IDs and values are dictionaries with keys 'idle', 'active', and 'total_workers'.
    Example:
        {
            iteration: 1,
            0: {'idle': 5, 'active': 10, 'total_workers': 15},
            1: {'idle': 3, 'active': 7, 'total_workers': 10},
            ...
        }
    """

    cpu = {}
    cpu['iteration'] = iteration

    # For having workers per CPU we need to access workerpools, each workerpool is bound to a CPU.
    # and have idle/workers counters.
    total_workers = 0
    total_idle = 0
    total_active = 0
    for _, pool in idr_for_each(worker_pool_idr):
        pool = drgn.Object(prog, 'struct worker_pool', address=pool)
        if pool.cpu >= 0:
            total_workers += pool.nr_workers.value_()
            total_idle += pool.nr_idle.value_()
            total_active += (pool.nr_workers.value_() -
                             pool.nr_idle.value_())
            cpu_key = pool.cpu.value_()
            if cpu_key not in cpu:
                cpu[cpu_key] = {'idle': 0,
                                'active': 0, 'total_workers': 0}

            cpu[cpu_key]['idle'] = total_idle
            cpu[cpu_key]['active'] = total_active
            cpu[cpu_key]['total_workers'] = total_workers
    return cpu


def scenario_total_workers_from_workqueue_per_cpu(iteration: int, filter_name: list[str] = None) -> dict[str, dict[str, dict[int, dict[str, int]]]]:
    """
    For each workqueue, returns a dictionary with CPU as key and a dictionary of idle, active, and total workers as values.

    Args:
        iteration (int): The current iteration number.
        filter_name (list[str], optional): A list of workqueue names to filter the results. If None, all workqueues are included.

    Returns:
        dict[str, dict[str, dict[int, dict[str, int]]]]: A dictionary where keys are workqueue names, and values are dictionaries with CPU IDs as keys and dictionaries with keys 'idle', 'active', and 'total_workers' as values.
    Example:
        {
            'my_workqueue_name': {
                'cpu': {
                    0: {
                        'idle': 5,
                        'active': 10,
                        'total_workers': 15
                    },
                    1: {
                        'idle': 3,
                        'active': 7,
                        'total_workers': 10
                    }
                }
            }
        }
    """

    wq = {}
    wq['iteration'] = iteration
    for wq_obj in list_for_each_entry('struct workqueue_struct', workqueues.address_of_(), 'list'):
        wq_name = wq_obj.name.string_().decode()
        if filter_name and wq_name not in filter_name:
            continue
        if wq_name not in wq:
            wq[wq_name] = {}
        for pool in list_for_each_entry('struct pool_workqueue', wq_obj.pwqs.address_of_(), 'pwqs_node'):
            cpu_counter_tmp = {}
            pool = drgn.Object(prog, 'struct worker_pool',
                               address=pool.pool.value_())

            for worker in list_for_each_entry('struct worker', pool.workers.address_of_(), 'node'):
                worker_on_wq = Worker(worker)
                if worker_on_wq.cpu not in cpu_counter_tmp:
                    cpu_counter_tmp[worker_on_wq.cpu] = {
                        'idle': 0,
                        'active': 0,
                        'total_workers': 0
                    }

                cpu_counter_tmp[worker_on_wq.cpu]['total_workers'] += 1
                cpu_counter_tmp[worker_on_wq.cpu]['active'] += 1 if worker_on_wq.is_running() else 0
                cpu_counter_tmp[worker_on_wq.cpu]['idle'] += 1 if not worker_on_wq.is_running() else 0

                if 'cpu' not in wq[wq_name]:
                    wq[wq_name]['cpu'] = {}

                if worker_on_wq.cpu not in wq[wq_name]['cpu']:
                    wq[wq_name]['cpu'][worker_on_wq.cpu] = {
                        'idle': 0,
                        'active': 0,
                        'total_workers': 0
                    }

                wq[wq_name]['cpu'][worker_on_wq.cpu]['idle'] = cpu_counter_tmp[worker_on_wq.cpu]['idle']
                wq[wq_name]['cpu'][worker_on_wq.cpu]['active'] = cpu_counter_tmp[worker_on_wq.cpu]['active']
                wq[wq_name]['cpu'][worker_on_wq.cpu]['total_workers'] = cpu_counter_tmp[worker_on_wq.cpu]['total_workers']
    return wq


def scenario_total_work_items_per_cpu(iteration: int, filter_cpu: list[int] = None) -> dict[int, dict[str, int]]:
    """
    For each workqueue, returns a dictionary with CPU as key and a dictionary of started and completed work items as values.

    Args:
        iteration (int): The current iteration number.
        filter_name (list[str], optional): A list of CPU IDs to filter the results. If None, all CPUs are included.

    Returns:
        dict[int, dict[str, int]]: A dictionary where keys are CPU IDs and values are dictionaries with keys 'started' and 'completed'.
    Example:
        {
            0: {'started': 5, 'completed': 10, 'current': 5},
            1: {'started': 3, 'completed': 7, 'current': 4},
            ...
        }
    """

    cpu = {}
    cpu['iteration'] = iteration

    stats = [0] * PWQ_NR_STATS
    for wq in list_for_each_entry('struct workqueue_struct', workqueues.address_of_(), 'list'):
        for pwq in list_for_each_entry('struct pool_workqueue', wq.pwqs.address_of_(), 'pwqs_node'):
            for i in range(PWQ_NR_STATS):
                stats[i] += int(pwq.stats[i])
            worker_pool = drgn.Object(
                prog, 'struct worker_pool', address=pwq.pool.value_())
            cpu[worker_pool.cpu.value_()] = {
                'started': stats[PWQ_STAT_STARTED],
                'completed': stats[PWQ_STAT_COMPLETED],
                'current': pwq.nr_active.value_() if pwq.nr_active else 0
            }
    return cpu


class Scenario(Enum):
    def __str__(self):
        return self.name

    NORMAL = 0
    TOTAL_WORKERS_PER_CPU = scenario_total_workers_per_cpu, 1
    TOTAL_WORKERS_FROM_WQ_PER_CPU = scenario_total_workers_from_workqueue_per_cpu, 2
    TOTAL_WORK_ITEMS_PER_CPU = scenario_total_work_items_per_cpu, 3
    ALL_POOLWORKQUEUES = 4


if __name__ == "__main__":
    parser = ArgumentParser(
        description='Print workqueue information.')
    parser.add_argument('workqueue_names', metavar='WORKQUEUE', nargs='*', default=[],
                        type=str, action='extend',
                        help='a name of the workqueue, can be multiple, e.g. "wq1 wq2", if not specified, all workqueues will be printed')
    parser.add_argument('-i', '--interval', metavar='SECS', type=float, default=1,
                        help='Monitoring interval (0 to print once and exit)')
    parser.add_argument('-m', '--max-iterations', metavar='N', type=int, default=0,
                        help='Maximum number of iterations (0 for infinite)')
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help='Print verbose information')
    parser.add_argument('-o', '--output', metavar='FILE', type=str, default=None,
                        help='Output file to write the results (default: stdout)')
    parser.add_argument('-a', '--show-active-pool', action='store_true', default=False,
                        help='Show active worker pools, only works with --scenario=all_poolworkqueues')
    parser.add_argument('-s', '--scenario', help='Scenario to run',
                        choices=[s.name for s in Scenario],
                        default=Scenario.NORMAL.name)

    args = parser.parse_args()

    if args.verbose:
        verbose = True

    signal.signal(signal.SIGINT, sigint_handler)
    i = 0
    data = []
    while not exit_req:
        if args.max_iterations > 0 and i >= args.max_iterations:
            break
        match args.scenario:
            case Scenario.NORMAL.name:
                for wq in list_for_each_entry('struct workqueue_struct', workqueues.address_of_(), 'list'):
                    if wq.name.string_().decode() not in args.workqueue_names:
                        continue
                    # delemite with number iteratation
                    sys.stdout.write(
                        f'Iteration: {i} --------------------------------------------------\n'
                    )
                    wq = WorkQueue(_wq=wq)
                    sys.stdout.write(f'{wq}\n')
                    sys.stdout.write(
                        f'--------------------------------------------------\n\n\n'
                    )
                sys.stdout.flush()
            case Scenario.TOTAL_WORKERS_PER_CPU.name:
                data.append(scenario_total_workers_per_cpu(i))
            case Scenario.TOTAL_WORKERS_FROM_WQ_PER_CPU.name:
                data.append(scenario_total_workers_from_workqueue_per_cpu(
                    i, filter_name=args.workqueue_names))
            case Scenario.TOTAL_WORK_ITEMS_PER_CPU.name:
                data.append(scenario_total_work_items_per_cpu(i))
            case Scenario.ALL_POOLWORKQUEUES.name:
                for pi, pool in idr_for_each(worker_pool_idr):
                    pool = drgn.Object(
                        prog, 'struct worker_pool', address=pool)
                    wk = WorkerPool(
                        pool, show_active=args.show_active_pool)
                    # print without \n
                    print(wk, end='', flush=True)
            case _:
                err(f'Unknown scenario: {args.scenario}')

        if not args.output:
            sys.stdout.write(f'Iteration: {i}\n')
            if args.scenario != Scenario.NORMAL.name:
                if data:
                    pp.pprint(data[-1])

        if args.interval == 0:
            break
        time.sleep(args.interval)
        i += 1
    if args.output:
        # output to json file
        import json
        with open(args.output, 'w') as f:
            json.dump(data, f, indent=4)
    sys.exit(0)
