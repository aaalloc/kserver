# Prérequis

- [drgn](https://drgn.readthedocs.io/en/latest/) : pour `wq_monitor.py`
- [bpf/bcc](https://github.com/iovisor/bcc/blob/master/INSTALL.md) : pour évaluer l'expérience ou on souhaite obtenir le temps de création d'un worker.

# Kserver

# Expériences

Différente expériences ont étais écrite, elle sont majoritairement sous forme de module à exécuter.

Pour lancer une expérience vous pouvez utiliser le script `xp-wq-script/start.sh`.

Par exemple si vous souhaitez lancer l'évaluation pour obtenir le temps pris entre l'insertion et l'exécution d'une tâche : `bash -x xp-wq-script/start.sh wq_insert_exec.ko`

les données seront écrite dans le dossier `/tmp` et peuvent être analysées via le script `analyze_wq_cycles.py` présent dans le dossier `xp-wq-script`.

## Temps pris entre l'insertion et l'exécution d'une tâche

Cet expériences vise à mesurer le temps que le noyau à pris pour insérer une tâche et arriver à son execution.

Pour cela, il n'est nécessaire d'appliquer les patchs sur le code source de linux : `insert-exec-measurement-wq.patch` et `rdtsc.patch` et d'obtenir un nouveau noyau.

Lorsque cela est fait, vous pouvez compiler le module noyau via `make wq-insert-exec`.

## Evalution de la stabilité d'une tâche entre plusieurs exécution

`make wq-exec-time-red`

## Temps de création d'un worker

1. `make wq-new-worker`

2. `sudo python3 bpf_worker_measurement.py`

## Temps de suppression d'un worker

`make wq-del-worker`
