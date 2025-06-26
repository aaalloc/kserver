#!/usr/bin/env python3
"""
Script pour analyser les données de cycles de workqueue avec start/end.
Calcule les différences entre les cycles et génère des graphiques comparatifs.

Convention des fichiers:
wq-i        # Plot by workqueue type
        wq_data = diff_df.groupby('workqueue_type')['cycle_diff'].agg([
            'median', 'std']).reset_index()
        ax1.bar(wq_data['workqueue_type'], wq_data['median'],
                yerr=wq_data['std'], capsize=5)
        ax1.set_title('Cycle Difference by Workqueue Type')
        ax1.set_xlabel('Workqueue Type')
        ax1.set_ylabel('Median Cycle Difference')
        ax1.grid(True, alpha=0.3)

        # Plot by affinity
        affinity_data = diff_df.groupby('affinity')['cycle_diff'].agg([
            'median', 'std']).reset_index()
        ax2.bar(affinity_data['affinity'], affinity_data['median'],
                yerr=affinity_data['std'], capsize=5)
        ax2.set_title('Cycle Difference by Affinity')
        ax2.set_xlabel('Affinity')
        ax2.set_ylabel('Median Cycle Difference')ion}-{bounded|unbound}-{low_affinity|high_affinity}-{timestamp}.{start|end}
"""

import matplotlib.colors as mcolors
import math
import matplotlib.pyplot as plt
import pandas as pd
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import argparse
from collections import defaultdict
import numpy as np
from wq_cycle_analyzer import WQCycleAnalyzer, WQXPEnum


def adjust_color(color, factor=0.8):
    """Darkens or lightens a given color by a factor."""
    rgb = np.array(mcolors.to_rgb(color))
    return np.clip(rgb * factor, 0, 1)


base_colors = {
    'min': 'blue',
    'median': 'green',
    'max': 'red',
}


def ploottt(df: pd.DataFrame):
    # Get all unique workqueue_types
    workqueue_types = df['workqueue_type'].unique()

    # Determine subplot grid size
    n = len(workqueue_types)
    cols = 2
    rows = math.ceil(n / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 6, rows * 4), squeeze=False)

    for idx, wq_type in enumerate(workqueue_types):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        # Filter df for this workqueue_type
        sub_df = df[df['workqueue_type'] == wq_type]

        for affinity, aff_df in sub_df.groupby('affinity'):
            agg_df = aff_df.groupby('iteration')['cycle_diff'].agg(
                ['min', 'median', 'max']).reset_index()

            factor = 0.8 if affinity == 'low_affinity' else 1.0
            ax.plot(agg_df['iteration'], agg_df['min'], label=f'{affinity} - Min',
                    linestyle='dotted', marker='o', markersize=5, color=adjust_color(base_colors['min'], factor))
            ax.plot(agg_df['iteration'], agg_df['median'], label=f'{affinity} - Median',
                    linestyle='solid', marker='o', markersize=5, color=adjust_color(base_colors['median'], factor))
            ax.plot(agg_df['iteration'], agg_df['max'], label=f'{affinity} - Max',
                    linestyle='dashed', marker='o', markersize=5, color=adjust_color(base_colors['max'], factor))

            print(f'{wq_type} | {affinity}')
            print(agg_df)
            print('-' * 40)

        ax.set_title(f'{wq_type}')
        ax.set_xlabel('Iteration')
        ax.set_ylabel('Time taken (in cycles)')
        ax.grid(True)
        ax.legend()

    # Hide unused subplots (if any)
    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Time taken (in cycles) per Iteration (per Workqueue Type)', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.97])
    plt.show()


def violin_plot(df: pd.DataFrame):
    import seaborn as sns
    import matplotlib.pyplot as plt
    import math

    # Remove rows with index 0 to avoid division by zero
    df = df[df.index != 0].copy()

    # Normalize cycle_diff by DataFrame index

    # Get all unique (wq_type, affinity) combinations
    groups = list(df.groupby(['workqueue_type', 'affinity']).groups.keys())

    # Determine grid size (e.g., 2 columns)
    n = len(groups)
    cols = 2
    rows = math.ceil(n / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 6, rows * 5), squeeze=False)

    for idx, ((wq_type, affinity), group_df) in enumerate(df.groupby(['workqueue_type', 'affinity'])):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        sns.violinplot(data=group_df, x='iteration',
                       y='normalized_cycle_diff', ax=ax, inner='quartile')

        ax.set_title(f'{wq_type} | {affinity}')
        ax.set_xlabel('Iteration')
        ax.set_ylabel('Normalized time taken (in cycles)')
        ax.grid(True, axis='y')

    # Remove unused axes if total plots < rows * cols
    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Normalized time taken (in cycles) Violin Plot per Iteration (per Workqueue Type & Affinity)', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.show()


def generate_summary_report(diff_df: pd.DataFrame, output_dir: Path):
    """Generate a summary report of the analysis."""
    report_path = output_dir / 'analysis_summary.txt'

    with open(report_path, 'w') as f:
        f.write("Workqueue Cycle Analysis Summary\n")
        f.write("=" * 40 + "\n\n")

        # Basic statistics
        f.write("Dataset Overview:\n")
        f.write(f"Total measurements: {len(diff_df)}\n")
        f.write(
            f"Unique iterations: {sorted(diff_df['iteration'].unique())}\n")
        f.write(
            f"Workqueue types: {list(diff_df['workqueue_type'].unique())}\n")
        f.write(
            f"Affinity types: {list(diff_df['affinity'].unique())}\n\n")

        # Statistical summary
        f.write("Summary for cycle_diff:\n")
        f.write(f"  Median: {diff_df['cycle_diff'].median():.4f}\n")
        f.write(f"  Std:    {diff_df['cycle_diff'].std():.4f}\n")
        f.write(f"  Min:    {diff_df['cycle_diff'].min():.4f}\n")
        f.write(f"  Max:    {diff_df['cycle_diff'].max():.4f}\n\n")

        # Best/worst configurations
        config_medians = diff_df.groupby(['workqueue_type', 'affinity'])[
            'cycle_diff'].median()
        best_config = config_medians.idxmin()
        worst_config = config_medians.idxmax()

        f.write(
            f"  Best configuration: {best_config} ({config_medians[best_config]:.4f})\n")
        f.write(
            f"  Worst configuration: {worst_config} ({config_medians[worst_config]:.4f})\n\n")

    print(f"Summary report saved to {report_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze workqueue cycle measurements (start/end differences)')
    parser.add_argument(
        'data_dir', help='Directory containing the measurement files')
    parser.add_argument('--output-dir', default='wq_cycle_analysis',
                        help='Output directory for plots and analysis')

    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    output_dir = Path(args.output_dir)

    if not data_dir.exists():
        print(f"Error: Data directory {data_dir} does not exist")
        return

    print("Initializing workqueue cycle analyzer...")
    analyzer = WQCycleAnalyzer(data_dir, WQXPEnum.INSERT_EXEC)

    diff_df = analyzer.analyze()

    match analyzer.xp_type:
        case WQXPEnum.INSERT_EXEC:
            generate_summary_report(diff_df, output_dir)
            ploottt(diff_df)
            violin_plot(diff_df)
        case _:
            print(f"no graph for {analyzer.xp_type}")

    print(f"Analysis complete! Results saved to {output_dir}")


if __name__ == "__main__":
    main()
