#!/usr/bin/env python3
"""
Script pour générer des graphiques basés sur les données de workqueue du kernel 
obtenu via wq_monito.py

Utilise matplotlib pour créer des visualisations des trois structures de données.
"""

import seaborn as sns
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


def plot_metrics(pivot_df: pd.DataFrame, ax, title: str, ylabel: str, chart_type: str = 'line'):
    """
    Plot a single metric on the provided axis.

    Parameters:
    - pivot_df: DataFrame with pivoted data (index=iteration, columns=cpu or wq)
    - ax: matplotlib Axes object
    - title: Title for the subplot
    - ylabel: Y-axis label
    - chart_type: 'line' or 'stacked'
    """
    if chart_type == 'line':
        pivot_df.plot(ax=ax)
    elif chart_type == 'stacked':
        ax.stackplot(pivot_df.index, pivot_df.T.values,
                     labels=pivot_df.columns)
        ax.legend()
    else:
        raise ValueError("chart_type must be 'line' or 'stacked'")

    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.grid(True)


def plot_workqueue_workers_per_cpu(df: pd.DataFrame, output_dir: Path, wq_names_filter: list[str] = None) -> None:
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

    Notes:
        wq_name_filter: If provided, filter the DataFrame to only include the specified workqueue name.
    """
    wq_df = df
    workqueues = wq_df['workqueue'].unique()
    chart_type = 'line'  # Default chart type

    metrics = ['total_workers', 'idle', 'active']
    titles = ['Total Workers', 'Idle Workers', 'Active Workers']
    for wq_name in workqueues:
        if wq_names_filter and wq_name not in wq_names_filter:
            continue

        wq_data = wq_df[wq_df['workqueue'] == wq_name]

        fig, axes = plt.subplots(3, 1, figsize=(15, 12))

        for i, (metric, title) in enumerate(zip(metrics, titles)):
            pivot_df = wq_data.pivot(
                index='iteration', columns='cpu_id', values=metric)
            plot_metrics(
                pivot_df, axes[i], f'{wq_name} - {title} per CPU', title, chart_type)

        plt.tight_layout()
        safe_wq_name = wq_name.replace(
            '/', '_').replace(' ', '_').replace(':', '_')
        output_file = output_dir / f'workqueue_{safe_wq_name}_per_cpu.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()

    if wq_names_filter:
        filtered_wq_df = wq_df[~wq_df['workqueue'].isin(wq_names_filter)]
    else:
        filtered_wq_df = wq_df.copy()

    # Aggregate remaining workqueues into a single "total"
    aggregated_data = (
        filtered_wq_df
        .groupby(['iteration'])
        .sum(numeric_only=True)
        .reset_index()
    )
    aggregated_data['workqueue'] = 'total'  # Add a "total" label

    # Prepare for plotting
    fig, axes = plt.subplots(3, 1, figsize=(15, 12))

    for i, (metric, title) in enumerate(zip(metrics, titles)):
        pivot_df = aggregated_data.pivot(
            index='iteration', columns='workqueue', values=metric
        )
        plot_metrics(
            pivot_df, axes[i], f'{title} - Total (Excluding Filtered WQs)', title, chart_type
        )

    plt.tight_layout()
    output_file = output_dir / 'workqueue_aggregated_summary_total_only.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()


def load_json_data(file_path) -> pd.DataFrame:
    """Load JSON data from file as a pandas DataFrame."""
    return pd.read_json(file_path)


def main():
    parser = argparse.ArgumentParser(
        description='Generate plots from workqueue monitoring data')
    parser.add_argument('--workqueue-workers',
                        help='JSON file with total_workers_from_wq_per_cpu data')
    parser.add_argument('--wq-names-filter', nargs='*', default=None,
                        help='Filter workqueue names to plot (space-separated list)')
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
    plot_workqueue_workers_per_cpu(data, output_dir, args.wq_names_filter)

    print("Plot generation complete!")


if __name__ == "__main__":
    main()
