#!/bin/bash


# export WQ_EXP="mom_first_step mom_second_step_cpu mom_second_step_disk mom_third_step_net_notify_sub mom_third_step_net_ack kserver_clients_read"
export WQ_EXP_FILTER="only_cpu_wq"
export ITERATION=300
export POLLING_INTERVAL=1

declare -a scenarios=(
    # "TOTAL_WORKERS_PER_CPU"
    "TOTAL_WORKERS_FROM_WQ_PER_CPU"
    # "TOTAL_WORK_ITEMS_PER_CPU"
)
pids=()
output_paths=()

# before starting we need to ask sudo permission to access the work queue
if ! sudo -n true 2>/dev/null; then
    echo "This script requires sudo permissions to access the work queue."
    echo "Please enter your password to continue."
    sudo -v
    if [ $? -ne 0 ]; then
        echo "Failed to obtain sudo permissions. Exiting."
        exit 1
    fi
fi


for scenario in "${scenarios[@]}"; do
    tmp_filename=$(echo "$scenario" | tr '[:upper:]' '[:lower:]')
    filename="wq_exp_${scenario_lower}_$(date +%Y%m%d_%H%M%S)"
    
    drgn wq_monitor.py $WQ_EXP --scenario=$scenario -i $POLLING_INTERVAL -m $ITERATION -o "${filename}.json" &
    echo "Running scenario: $scenario"
    echo "Output will be saved to ${scenario_lower}.json"
    output_paths+=("${filename}.json")
    pids+=($!)
done

# Wait for all background processes to finish
for pid in "${pids[@]}"; do
    if wait $pid; then
        echo "Process $pid completed successfully."
    else
        echo "Process $pid failed."
    fi
done

# generate graph
for output_path in "${output_paths[@]}"; do
    if [ -f "$output_path" ]; then
        python3 plot_data.py --workqueue-workers "$output_path" --wq-names-filter $WQ_EXP_FILTER
    else
        echo "Output file $output_path not found."
    fi
done




