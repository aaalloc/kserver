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
import seaborn as sns
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import argparse
from collections import defaultdict
import numpy as np
from wq_cycle_analyzer import WQCycleAnalyzer, WQXPEnum
import plotly.express as px


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


def merged_violinplots_by_matrix_op(df: pd.DataFrame):
    # Unique matrix operations
    matrix_ops = sorted(df['matrix_op'].unique())
    n = len(matrix_ops)

    # Subplot layout
    cols = 2
    rows = math.ceil(n / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 7, rows * 5), squeeze=False)

    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    for idx, op in enumerate(matrix_ops):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        sub_df = df[df['matrix_op'] == op]

        sns.violinplot(
            data=sub_df,
            x='workqueue_type',
            y='time_taken',
            hue='affinity',
            split=True,
            inner='quartile',
            ax=ax
        )

        ax.set_title(f'Matrix Op: {op}')
        ax.set_xlabel('Workqueue Type')
        ax.set_ylabel('Time Taken (s)')
        ax.grid(True)
        ax.legend(title='Affinity', loc='upper right')

    # Remove unused axes
    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Time Taken per Workqueue Type (Grouped by Matrix Operation)', fontsize=16)
    plt.tight_layout(h_pad=2.5)
    plt.show()


def plootttv2(df: pd.DataFrame):
    # Get all unique workqueue_types
    workqueue_types = df['workqueue_type'].unique()

    # Determine subplot grid size
    n = len(workqueue_types)
    cols = 2
    rows = math.ceil(n / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 6, rows * 4), squeeze=False)

    agg_df_list = []
    for idx, wq_type in enumerate(workqueue_types):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        # Filter df for this workqueue_type
        sub_df = df[df['workqueue_type'] == wq_type]

        for affinity, aff_df in sub_df.groupby('affinity'):
            agg_df = aff_df.groupby('matrix_op')['time_taken'].agg(
                ['min', 'median', 'max']).reset_index()

            agg_df_list.append(agg_df)
            factor = 0.8 if affinity == 'low_affinity' else 1.0
            ax.plot(agg_df['min'], agg_df['matrix_op'], label=f'{affinity} - Min',
                    linestyle='dotted', marker='o', markersize=5, color=adjust_color(base_colors['min'], factor))
            ax.plot(agg_df['median'], agg_df['matrix_op'], label=f'{affinity} - Median',
                    linestyle='solid', marker='o', markersize=5, color=adjust_color(base_colors['median'], factor))
            ax.plot(agg_df['max'], agg_df['matrix_op'], label=f'{affinity} - Max',
                    linestyle='dashed', marker='o', markersize=5, color=adjust_color(base_colors['max'], factor))
            print(f'{wq_type} | {affinity}')
            print(agg_df)
            print('-' * 40)

        ax.set_title(f'{wq_type}')
        ax.set_xlabel('Time taken (in seconds)')
        ax.set_ylabel('Matrix Operation')
        ax.grid(True)
        ax.legend()

    # Hide unused subplots (if any)
    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Matrix Operation per Time taken (in seconds) (per Workqueue Type)', fontsize=16)
    plt.show()


def min_median_max_errorbar_plot(df: pd.DataFrame):
    workqueue_types = df['workqueue_type'].unique()
    n = len(workqueue_types)
    cols = 2
    rows = math.ceil(n / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 7, rows * 5), squeeze=False)

    for idx, wq_type in enumerate(workqueue_types):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        sub_df = df[df['workqueue_type'] == wq_type]

        plot_data = []
        for (affinity, matrix_op), group in sub_df.groupby(['affinity', 'matrix_op']):
            min_val = group['time_taken'].min()
            median_val = group['time_taken'].median()
            max_val = group['time_taken'].max()
            plot_data.append((affinity, matrix_op, median_val,
                             median_val - min_val, max_val - median_val))

        plot_df = pd.DataFrame(plot_data, columns=[
                               'affinity', 'matrix_op', 'median', 'err_min', 'err_max'])

        for affinity in plot_df['affinity'].unique():
            subset = plot_df[plot_df['affinity'] == affinity]
            ax.errorbar(
                x=subset['median'],
                y=subset['matrix_op'],
                xerr=[subset['err_min'], subset['err_max']],
                fmt='o',
                label=affinity,
                capsize=5
            )

        ax.set_title(f'{wq_type}')
        ax.set_xlabel('Time taken (in seconds)')
        ax.set_ylabel('Iteration (matrix_op)')
        ax.grid(True)
        ax.legend(title='Affinity')

    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Median Time with Min/Max Error Bars (per Workqueue Type)', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.97])
    plt.show()


def errorbar_per_matrix_op_grouped_by_workqueue(df: pd.DataFrame):
    matrix_ops = sorted(df['matrix_op'].unique())
    workqueue_types = df['workqueue_type'].unique()

    for wq_type in workqueue_types:
        sub_df_wq = df[df['workqueue_type'] == wq_type]

        n = len(matrix_ops)
        cols = 3
        rows = math.ceil(n / cols)

        fig, axes = plt.subplots(rows, cols, figsize=(
            cols * 6, rows * 5), squeeze=False)

        for idx, matrix_op in enumerate(matrix_ops):
            r, c = divmod(idx, cols)
            ax = axes[r][c]

            sub_df = sub_df_wq[sub_df_wq['matrix_op'] == matrix_op]

            plot_data = []
            for affinity, group in sub_df.groupby('affinity'):
                min_val = group['time_taken'].min()
                median_val = group['time_taken'].median()
                max_val = group['time_taken'].max()
                err_low = median_val - min_val
                err_high = max_val - median_val
                plot_data.append((affinity, median_val, err_low, err_high))

            plot_df = pd.DataFrame(plot_data, columns=[
                                   'affinity', 'median', 'err_low', 'err_high'])

            x = range(len(plot_df))
            ax.errorbar(
                x=x,
                y=plot_df['median'],
                yerr=[plot_df['err_low'], plot_df['err_high']],
                fmt='o',
                capsize=5,
                ecolor='black',
                marker='o',
                linestyle='None'
            )

            ax.set_title(f'matrix_op = {matrix_op}')
            ax.set_ylabel('Time taken (s)')
            ax.set_xticks(x)
            ax.set_xticklabels(plot_df['affinity'], rotation=30, ha='right')
            ax.grid(True)

        # Remove unused axes
        for i in range(n, rows * cols):
            r, c = divmod(i, cols)
            fig.delaxes(axes[r][c])

        fig.suptitle(
            f'Median ± Min/Max per Matrix Op ({wq_type})', fontsize=16)
        plt.tight_layout(rect=[0, 0, 1, 0.97])
        plt.show()


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
    analyzer = WQCycleAnalyzer(data_dir)

    diff_df = analyzer.analyze()

    match analyzer.xp_type:
        case WQXPEnum.INSERT_EXEC:
            generate_summary_report(diff_df, output_dir)
            ploottt(diff_df)
            violin_plot(diff_df)
        case WQXPEnum.EXEC_TIME_PRED:
            # kde_time_taken_distribution(diff_df)
            merged_violinplots_by_matrix_op(diff_df)
            plootttv2(diff_df)
            # errorbar_per_matrix_op_grouped_by_workqueue(diff_df)
        case _:
            print(f"no graph for {analyzer.xp_type}")

    print(f"Analysis complete! Results saved to {output_dir}")


if __name__ == "__main__":
    main()
