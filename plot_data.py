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


def normalize_workqueue_iat_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    Normalize workqueue/cores idle/active/total_workers data from DataFrame.
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

    return pd.DataFrame(normalized_data)


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
    wq_df = df

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
    # generate folder name based on the current date and time
    current_time = pd.Timestamp.now().strftime('%Y%m%d_%H%M%S')
    output_dir = Path(args.output_dir) / f'workqueue_plots_{current_time}'

    output_dir.mkdir(exist_ok=True)

    print("Processing workqueue workers per CPU data...")
    data = load_json_data(args.workqueue_workers)
    data = normalize_workqueue_iat_data(data)
    plot_workqueue_workers_per_cpu(data, output_dir)

    print("Plot generation complete!")


if __name__ == "__main__":
    main()
