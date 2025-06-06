#!/usr/bin/env python3
"""
Script pour générer des graphiques basés sur les données de workqueue du kernel.
Utilise matplotlib pour créer des visualisations des trois structures de données.
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import pandas as pd
from typing import Dict, List, Any
import json
import argparse
from pathlib import Path


def plot_total_workers_per_cpu(df: pd.DataFrame, output_dir: Path):
    """
    Plot the total number of workers per CPU.
        {
            iteration: 1,
            0: {'idle': 5, 'active': 10, 'total_workers': 15},
            1: {'idle': 3, 'active': 7, 'total_workers': 10},
            ...
        }
    """

    # Normalize the nested dictionary data
    df_normalized = pd.concat(
        [df['iteration']] +
        [df[core].apply(pd.Series).add_prefix(
            f'core{core}_') for core in df.columns if core != 'iteration'],
        axis=1
    )

    # Plotting
    plt.figure(figsize=(12, 6))

    # Plot idle and active workers for each core
    for core in range(len([col for col in df.columns if col != 'iteration'])):
        plt.plot(df_normalized['iteration'],
                 df_normalized[f'core{core}_idle'], label=f'Core {core} Idle', marker='o')
        plt.plot(df_normalized['iteration'],
                 df_normalized[f'core{core}_active'], label=f'Core {core} Active', marker='o')

    # Add labels and legend
    plt.xlabel('Iteration')
    plt.ylabel('Number of Workers')
    plt.title('Worker Status Over Iterations Grouped by Cores')
    plt.legend()
    plt.grid(True)

    # Show plot
    plt.show()


def plot_workqueue_workers_per_cpu(df: pd.DataFrame, output_dir: Path):
    """
    Plot the number of workqueue workers per CPU using pandas.
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
    normalized_data = []
    for index, row in df.iterrows():
        iteration = row.get('iteration', index)
        for workqueue_name, wq_data in row.items():
            if workqueue_name == 'iteration':
                continue
            if not isinstance(wq_data, dict) or 'cpu' not in wq_data:
                continue
            for cpu_id, cpu_stats in wq_data['cpu'].items():
                normalized_data.append({
                    'iteration': iteration,
                    'workqueue': workqueue_name,
                    'cpu_id': int(cpu_id),
                    'idle': cpu_stats.get('idle', 0),
                    'active': cpu_stats.get('active', 0),
                    'total_workers': cpu_stats.get('total_workers', 0)
                })

    if not normalized_data:
        print("No workqueue data found")
        return

    # Create pandas DataFrame from normalized data
    wq_df = pd.DataFrame(normalized_data)

    # Get unique workqueues
    workqueues = wq_df['workqueue'].unique()

    # Create plots for each workqueue
    for wq_name in workqueues:
        wq_data = wq_df[wq_df['workqueue'] == wq_name]

        # Plot total workers, idle, and active workers per CPU
        fig, axes = plt.subplots(3, 1, figsize=(15, 12))

        # Plot total workers
        wq_data.pivot(index='iteration', columns='cpu_id',
                      values='total_workers').plot(ax=axes[0])
        axes[0].set_title(f'{wq_name} - Total Workers per CPU')
        axes[0].set_ylabel('Total Workers')
        axes[0].grid(True)

        # Plot idle workers
        wq_data.pivot(index='iteration', columns='cpu_id',
                      values='idle').plot(ax=axes[1])
        axes[1].set_title(f'{wq_name} - Idle Workers per CPU')
        axes[1].set_ylabel('Idle Workers')
        axes[1].grid(True)

        # Plot active workers
        wq_data.pivot(index='iteration', columns='cpu_id',
                      values='active').plot(ax=axes[2])
        axes[2].set_title(f'{wq_name} - Active Workers per CPU')
        axes[2].set_ylabel('Active Workers')
        axes[2].grid(True)

        plt.tight_layout()

        # Save plot
        safe_wq_name = wq_name.replace(
            '/', '_').replace(' ', '_').replace(':', '_')
        output_file = output_dir / f'workqueue_{safe_wq_name}_per_cpu.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()

    # Create aggregated view of all workqueues
    fig, axes = plt.subplots(3, 1, figsize=(15, 12))

    # Aggregate data per workqueue across all CPUs
    aggregated_data = wq_df.groupby(
        ['iteration', 'workqueue']).sum().reset_index()

    # Plot total workers
    aggregated_data.pivot(index='iteration', columns='workqueue',
                          values='total_workers').plot(ax=axes[0])
    axes[0].set_title('Total Workers per Workqueue (All CPUs)')
    axes[0].set_ylabel('Total Workers')
    axes[0].grid(True)

    # Plot idle workers
    aggregated_data.pivot(index='iteration', columns='workqueue',
                          values='idle').plot(ax=axes[1])
    axes[1].set_title('Idle Workers per Workqueue (All CPUs)')
    axes[1].set_ylabel('Idle Workers')
    axes[1].grid(True)

    # Plot active workers
    aggregated_data.pivot(index='iteration', columns='workqueue',
                          values='active').plot(ax=axes[2])
    axes[2].set_title('Active Workers per Workqueue (All CPUs)')
    axes[2].set_ylabel('Active Workers')
    axes[2].grid(True)

    plt.tight_layout()

    # Save aggregated plot
    output_file = output_dir / 'workqueue_aggregated_summary.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    print("Workqueue workers per CPU plots generation complete!")


def plot_work_items_per_cpu(df: pd.DataFrame, output_dir: Path):
    """
        {
            0: {'started': 5, 'completed': 10, 'current': 5},
            1: {'started': 3, 'completed': 7, 'current': 4},
            ...
        }
    """
    pass


def load_json_data(file_path) -> pd.DataFrame:
    """Load JSON data from file as a pandas DataFrame."""
    return pd.read_json(file_path)


def main():
    parser = argparse.ArgumentParser(
        description='Generate plots from workqueue monitoring data')
    parser.add_argument('--total-workers',
                        help='JSON file with total_workers_per_cpu data')
    parser.add_argument('--workqueue-workers',
                        help='JSON file with total_workers_from_wq_per_cpu data')
    parser.add_argument(
        '--work-items', help='JSON file with total_work_items_per_cpu data')
    parser.add_argument('--output-dir', default='.',
                        help='Output directory for plots')

    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)

    # Process each data type
    if args.total_workers:
        print("Processing total workers per CPU data...")
        data: pd.DataFrame = load_json_data(args.total_workers)
        plot_total_workers_per_cpu(data, output_dir)

    if args.workqueue_workers:
        print("Processing workqueue workers per CPU data...")
        data = load_json_data(args.workqueue_workers)
        plot_workqueue_workers_per_cpu(data, output_dir)

    if args.work_items:
        print("Processing work items per CPU data...")
        data = load_json_data(args.work_items)
        plot_work_items_per_cpu(data, output_dir)

    print("Plot generation complete!")


if __name__ == "__main__":
    main()
