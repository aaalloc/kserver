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


POOL_BH = prog['POOL_BH']
WQ_NAME_LEN = prog['WQ_NAME_LEN'].value_()
WQ_UNBOUND = prog['WQ_UNBOUND']
WQ_BH = prog['WQ_BH']
WQ_ORDERED = prog['__WQ_ORDERED']
# work items started execution
PWQ_STAT_STARTED = prog['PWQ_STAT_STARTED']
# work items completed execution
PWQ_STAT_COMPLETED = prog['PWQ_STAT_COMPLETED']
PWQ_NR_STATS = prog['PWQ_NR_STATS']


max_pool_id_len = 0
max_ref_len = 0
for pi, pool in idr_for_each(worker_pool_idr):
    pool = drgn.Object(prog, 'struct worker_pool', address=pool)
    max_pool_id_len = max(max_pool_id_len, len(f'{pi}'))
    max_ref_len = max(max_ref_len, len(f'{pool.refcnt.value_()}'))


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
    nr_running: int = -1

    def __init__(self, _pool):
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

    def __str__(self):
        s = f'pool[{self.id:0{max_pool_id_len}}] flags=0x{self.flags:02x} ref={self.refcnt:{max_ref_len}} nice={self.nice:3} '
        s += f'idle/workers={self.nr_idle:3}/{self.nr_workers:3} '
        s += f'nr_running={self.nr_running}    '
        if self.cpu >= 0:
            s += f'cpu={self.cpu:3}'
            if self.bh:
                s += ' bh'
        else:
            s += f'cpus={self.cpus}'
            if self.bh:
                s += ' bh'
        return s


@dataclass
class WorkQueue():
    _wq: object
    _pools: list[object]
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
        for pwq in list_for_each_entry('struct pool_workqueue', self._wq.pwqs.address_of_(), 'pwqs_node'):
            for i in range(PWQ_NR_STATS):
                self._stats[i] += int(pwq.stats[i])
            self._pools.append(drgn.Object(
                prog, 'struct worker_pool', address=pwq.pool.value_()))
            self.nr_active += pwq.nr_active.value_()

    def __str__(self):

        s = f'{self.name:{WQ_NAME_LEN}}\n'
        s += f' ├─flags=0x{self.flags:02x} type={wq_type_str(self._wq):4}\n'
        if verbose:
            for pool in self._pools:
                s += f' ├─{WorkerPool(pool)}\n'
        s += f' ├─nr_active={self.nr_active:8}\n'
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


# fill pools list
# for pi, pool in idr_for_each(worker_pool_idr):
#     pool = drgn.Object(prog, 'struct worker_pool', address=pool)
#     pools.append(WorkerPool(
#         _pool=pool,
#         id=pi,
#         flags=pool.flags.value_(),
#         refcnt=pool.refcnt.value_(),
#         nice=pool.attrs.nice.value_(),
#         nr_idle=pool.nr_idle.value_(),
#         nr_workers=pool.nr_workers.value_(),
#         cpu=pool.cpu.value_() if pool.cpu >= 0 else -1,
#         cpus=cpumask_str(pool.attrs.cpumask),
#         bh=pool.flags & POOL_BH,
#         unbound=pool.cpu < 0,
#     ))

# workqueues_dict = {}
# for wq in list_for_each_entry('struct workqueue_struct', workqueues.address_of_(), 'list'):
#     wq = drgn.Object(prog, 'struct workqueue_struct', address=wq)
#     workqueues_dict[wq.name.string_().decode()] = WorkQueue(
#         _wq=wq,
#         flags=wq.flags.value_(),
#         name=wq.name.string_().decode(),
#         cpu_pwq=wq.cpu_pwq,
#         dfl_pwq=wq.dfl_pwq,
#         unbound_attrs=wq.unbound_attrs.value_()
#     )


exit_req = False
verbose = False


def sigint_handler(signr, frame):
    global exit_req
    exit_req = True


if __name__ == "__main__":
    # print(pools[49])
    parser = ArgumentParser(
        description='Print workqueue information.')
    parser.add_argument('workqueue_names', metavar='N', type=str, nargs='+',
                        help='a name of the workqueue')
    parser.add_argument('-i', '--interval', metavar='SECS', type=float, default=1,
                        help='Monitoring interval (0 to print once and exit)')
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help='Print verbose information')

    args = parser.parse_args()

    if args.verbose:
        verbose = True

    signal.signal(signal.SIGINT, sigint_handler)
    i = 0
    while not exit_req:
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
        if args.interval == 0:
            break
        time.sleep(args.interval)
        i += 1
