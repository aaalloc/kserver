# Prérequis

- [drgn](https://drgn.readthedocs.io/en/latest/) : pour `wq_monitor.py`
- [bpf/bcc](https://github.com/iovisor/bcc/blob/master/INSTALL.md) : pour évaluer l'expérience ou on souhaite obtenir le temps de création d'un worker.

# `wq_monitor.py`
Script permettant de monitorer les workqueues du noyau linux.
Note : il est nécessaire d'installer [drgn](https://drgn.readthedocs.io/en/latest/)

```
$ drgn wq_monitor.py --help
usage: wq_monitor.py [-h] [-i SECS] [-m N] [-v] [-o FILE] [-a] [-s {NORMAL,TOTAL_WORKERS_PER_CPU,TOTAL_WORKERS_FROM_WQ_PER_CPU,TOTAL_WORK_ITEMS_PER_CPU,ALL_POOLWORKQUEUES}] [WORKQUEUE ...]

Print workqueue information.

positional arguments:
  WORKQUEUE             a name of the workqueue, can be multiple, e.g. "wq1 wq2", if not specified, all workqueues will be printed

options:
  -h, --help            show this help message and exit
  -i SECS, --interval SECS
                        Monitoring interval (0 to print once and exit)
  -m N, --max-iterations N
                        Maximum number of iterations (0 for infinite)
  -v, --verbose         Print verbose information
  -o FILE, --output FILE
                        Output file to write the results (default: stdout)
  -a, --show-active-pool
                        Show active worker pools, only works with --scenario=all_poolworkqueues
  -s {NORMAL,TOTAL_WORKERS_PER_CPU,TOTAL_WORKERS_FROM_WQ_PER_CPU,TOTAL_WORK_ITEMS_PER_CPU,ALL_POOLWORKQUEUES}, --scenario {NORMAL,TOTAL_WORKERS_PER_CPU,TOTAL_WORKERS_FROM_WQ_PER_CPU,TOTAL_WORK_ITEMS_PER_CPU,ALL_POOLWORKQUEUES}
                        Scenario to run
```

Cas d'usage :
Voir les informations sur la workqueue `events` 
```
$ drgn wq_monitor.py events
Iteration: 0 --------------------------------------------------
events                  
 ├─flags=0x00 type=percpu   
 ├─nr_active=       0
 ├─nr_activev2=       0
 ├─started=  732522 completed=  732522 
--------------------------------------------------
.....
```
et avec de la verbosité :
```
$ drgn wq_monitor.py -v events
Iteration: 0 --------------------------------------------------
events                  
 ├─flags=0x00 type=percpu   
 ├─pool[22] flags=0x00 ref=  1 nice=  0 idle/workers=  4/  4 nr_running=(int)0    cpu= 11
 ├─events[PID=20418, ID=2] cpu= 11 Running=False 
 ├─events[PID=25167, ID=3] cpu= 11 Running=False 

 ├─pool[20] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu= 10
 ├─events[PID=21907, ID=1] cpu= 10 Running=False 
 ├─events[PID=24688, ID=2] cpu= 10 Running=False 

 ├─pool[18] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  9
 ├─events[PID=20516, ID=0] cpu=  9 Running=False 

 ├─pool[16] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  8
 ├─events[PID=24153, ID=2] cpu=  8 Running=False 

 ├─pool[14] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  7

 ├─pool[12] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  6
 ├─events[PID=23719, ID=1] cpu=  6 Running=False 

 ├─pool[10] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  5
 ├─events[PID=19058, ID=2] cpu=  5 Running=False 
 ├─events[PID=21775, ID=0] cpu=  5 Running=False 

 ├─pool[08] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  4

 ├─pool[06] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  3
 ├─events[PID=23006, ID=1] cpu=  3 Running=False 
 ├─events[PID=25322, ID=0] cpu=  3 Running=False 

 ├─pool[04] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  2
 ├─events[PID=25308, ID=2] cpu=  2 Running=False 

 ├─pool[02] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  1
 ├─events[PID=21275, ID=1] cpu=  1 Running=False 

 ├─pool[00] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  0
 ├─events[PID=19357, ID=0] cpu=  0 Running=False 
 ├─events[PID=24689, ID=2] cpu=  0 Running=False 
 ├─events[PID=25325, ID=1] cpu=  0 Running=False 

 ├─nr_active=       0
 ├─nr_activev2=       0
 ├─started=  734340 completed=  734340 
--------------------------------------------
```

Voir tous les worker pools 
```
$ drgn wq_monitor.py -s ALL_POOLWORKQUEUES
pool[00] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  0
pool[01] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  0
pool[02] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  1
pool[03] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  1
pool[04] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  2
pool[05] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  2
pool[06] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  3
pool[07] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  3
pool[08] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  4
pool[09] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  4
pool[10] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  5
pool[11] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  5
pool[12] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  6
pool[13] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  6
pool[14] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  7
pool[15] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  7
pool[16] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu=  8
pool[17] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  8
pool[18] flags=0x00 ref=  1 nice=  0 idle/workers=  2/  2 nr_running=(int)0    cpu=  9
pool[19] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu=  9
pool[20] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu= 10
pool[21] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu= 10
pool[22] flags=0x00 ref=  1 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpu= 11
pool[23] flags=0x00 ref=  1 nice=-20 idle/workers=  2/  2 nr_running=(int)0    cpu= 11
pool[24] flags=0x04 ref=650 nice=  0 idle/workers=  3/  3 nr_running=(int)0    cpus=00000fff
pool[25] flags=0x04 ref= 78 nice=-20 idle/workers=  4/  4 nr_running=(int)0    cpus=00000fff
Iteration: 0
...
```
voir seulement les worker pool actif
```
$ drgn wq_monitor.py -s ALL_POOLWORKQUEUES --show-active-pool
```

# Kserver
Module noyau permettant de simuler un graphe d'exécution de tâches avec des workqueues.

Voir le code dans `src/main.c` pour plus de détails.

# Expériences

Différente expériences ont étais écrite, elle sont majoritairement sous forme de module à exécuter.

Pour lancer une expérience vous pouvez utiliser le script `xp-wq-script/start.sh`.

Par exemple si vous souhaitez lancer l'évaluation pour obtenir le temps pris entre l'insertion et l'exécution d'une tâche : `bash -x xp-wq-script/start.sh wq_insert_exec.ko`

les données seront écrite dans le dossier `/tmp` et peuvent être analysées via le script `analyze_wq_cycles.py` présent dans le dossier `xp-wq-script`.


Après avoir lancé l'expérience, vous pouvez visualiser les données avec le script `analyze_wq_cycles.py` :
```
$ python3 xp-wq-script/analyze_wq_cycles.py --help                                                                   
usage: analyze_wq_cycles.py [-h] [--output-dir OUTPUT_DIR] data_dir

Analyze workqueue cycle measurements (start/end differences)

positional arguments:
  data_dir              Directory containing the measurement files

options:
  -h, --help            show this help message and exit
  --output-dir OUTPUT_DIR
                        Output directory for plots and analysis
```

Par exemple pour analyser les données dans le dossier `/tmp` :
```
$ python3 xp-wq-script/analyze_wq_cycles.py ~/xp-result-wq/xp-wq-insert-exec-wq-1two1000 --output-dir /tmp/something
```
Différent seront créer, le code actuel gére les scénarios suivants :
- `wq_insert_exec` : pour le temps pris entre l'insertion et l'exécution d'une tâche.
- `wq_exec_time_pred` : pour l'évaluation de la stabilité d'une tâche entre plusieurs exécutions.

La detection du scénario est automatique, il suffit de lancer le script avec le dossier contenant les données.


## Temps pris entre l'insertion et l'exécution d'une tâche

Cet expériences vise à mesurer le temps que le noyau à pris pour insérer une tâche et arriver à son execution.

Pour cela, il n'est nécessaire d'appliquer les patchs sur le code source de linux : `insert-exec-measurement-wq.patch` et `rdtsc.patch` et d'obtenir un nouveau noyau.

Lorsque cela est fait, vous pouvez compiler le module noyau via `make wq-insert-exec`.

## Evalution de la stabilité d'une tâche entre plusieurs exécution

`make wq-exec-time-red`

## Temps de création/suppression d'un worker
Les deux expériences suivantes visent à mesurer le temps de création et de suppression d'un worker dans une workqueue.

Pour la création, l'idée utilisé et de bourriner l'insertion de tâche dans une workqueue pour forcer provoquer un débordement de la workqueue et ainsi provoquer la création de worker.

Pour la suppression, il est nécessaire d'attendre un certain temps pour que les workers crées soient supprimés.

Note : Il est surement possible de récuperer le PID des workers qui sont lié à la workqueue où les tâches sont insérées.

### Creation
```
$ sudo python3 bpf_worker_measurement.py --help                                                                         
usage: bpf_worker_measurement.py [-h] [--type-measurement {creation,die}] [--delay DELAY] [--nr-work-max NR_WORK_MAX] [--save-json SAVE_JSON] [--load-json LOAD_JSON] [--save-plot]

Trace create_worker latency and manage kernel module.

options:
  -h, --help            show this help message and exit
  --type-measurement {creation,die}
                        Type of worker measurement: 'creation' (default) or 'die'
  --delay DELAY         Delay value to pass to the kernel module (default: 100)
  --nr-work-max NR_WORK_MAX
                        nr_work_max value to pass to the kernel module (default: 500)
  --save-json SAVE_JSON
                        Save histogram data to a JSON file
  --load-json LOAD_JSON
                        Load histogram data from JSON file instead of tracing
  --save-plot           Save the histogram plot as a PNG file
```

Pour lancer l'expérience de mesure avec la création de worker
```
$ make wq-new-worker
$ sudo python3 bpf_worker_measurement.py --type-measurement=creation
```

Le script python va insérer le module noyau compiler avec `make wq-new-worker` et ensuite du code eBPF va traquer le nombre de création de worker.

Le module noyau à deux paramètres :
- `delay` : le délai en ms entre chaque insertion de travail dans la workqueue
- `nr_work_max` : le nombre de travail maximum à insérer dans la workqueue

Donc il est possible de lancer l'expérience avec un délai différent ou un nombre de travail maximum différent en utilisant les options `--delay` et `--nr-work-max`.

Par exemple pour un délai de 200ms et un nombre de travail maximum de 1000 :
```
$ sudo python3 bpf_worker_measurement.py --type-measurement=creation --delay 200 --nr-work-max 1000
```


### Suppression

Note:
- Il est nécessaire d'appliquer le patch `workqueue_worker_measure_dying_bpf.patch` présent dans le dossier `linux_patch_exp/`.
- Pour simplifier il est recommandé de lancer d'abord le script pour lancer la création de worker, puis de lancer le script pour la suppression de worker.

```
$ sudo python3 bpf_worker_measurement.py --type-measurement=die
```

Actuellement ce script ne fonctionne pas mais il est possible de l'exécuter via bpftrace :
```
$ sudo bpftrace -e '
tracepoint:workqueue:workqueue_worker_dying_start
{
    @start[args->pid] = nsecs;
}

tracepoint:workqueue:workqueue_worker_dying_end
/@start[args->pid]/
{
    $delta = nsecs - @start[args->pid];
    @duration_us = hist($delta / 1000);
    delete(@start[args->pid]);
}
'
```

