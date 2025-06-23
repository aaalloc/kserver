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

import matplotlib.pyplot as plt
import pandas as pd
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import argparse
from collections import defaultdict
import numpy as np


class WQCycleAnalyzer:
    def __init__(self, data_dir: Path):
        self.data_dir = Path(data_dir)
        self.data_cache = {}
        self.parsed_files = []

    def parse_filename(self, filename: str) -> Optional[Dict[str, str]]:
        """
        Parse filename following convention:
        wq-insert-exec-{iteration}-{bounded|unbound}-{low_affinity|high_affinity}-{timestamp}.{start|end}
        """
        pattern = r'wq-insert-exec-(\d+)-(bounded|unbound)-(low_affinity|high_affinity)-([^.]+)\.(start|end)'
        match = re.match(pattern, filename)

        if match:
            return {
                'iteration': int(match.group(1)),
                'workqueue_type': match.group(2),
                'affinity': match.group(3),
                'timestamp': match.group(4),
                'cycle_type': match.group(5)
            }
        return None

    def load_cycles_data(self, file_path: Path) -> List[int]:
        """Load cycles data from text file (one cycle per line)."""
        try:
            with open(file_path, 'r') as f:
                cycles = []
                for line in f:
                    line = line.strip()
                    if line:  # Skip empty lines
                        cycles.append(int(line))
                return cycles
        except (ValueError, IOError) as e:
            print(f"Error loading {file_path}: {e}")
            return []

    def load_data_files(self) -> Dict[str, Dict[str, List[int]]]:
        """Load all data files and group them by test configuration."""
        files = list(self.data_dir.glob('wq-insert-exec-*'))

        grouped_data = defaultdict(lambda: {'start': None, 'end': None})

        for file_path in files:
            parsed = self.parse_filename(file_path.name)
            if not parsed:
                print(f"Skipping file with invalid name: {file_path.name}")
                continue

            # Create a key for grouping (iteration, workqueue_type, affinity, timestamp)
            key = (parsed['iteration'], parsed['workqueue_type'],
                   parsed['affinity'], parsed['timestamp'])

            try:
                # Load cycles data (one cycle per line)
                cycles = self.load_cycles_data(file_path)
                if cycles:
                    grouped_data[key][parsed['cycle_type']] = cycles

                self.parsed_files.append({
                    'file_path': file_path,
                    'config': parsed,
                    'key': key
                })

            except Exception as e:
                print(f"Error loading {file_path}: {e}")
                continue

        return dict(grouped_data)

    def calculate_cycle_differences(self, grouped_data: Dict) -> pd.DataFrame:
        """Calculate differences between start and end cycles."""
        results = []

        for key, data_pair in grouped_data.items():
            iteration, workqueue_type, affinity, timestamp = key

            start_cycles = data_pair['start']
            end_cycles = data_pair['end']

            if start_cycles is None or end_cycles is None:
                print(f"Missing start or end data for {key}")
                continue

            # Ensure both lists have the same length
            if len(start_cycles) != len(end_cycles):
                print(
                    f"Length mismatch for {key}: start {len(start_cycles)} vs end {len(end_cycles)}")
                # Take the minimum length to avoid index errors
                min_len = min(len(start_cycles), len(end_cycles))
                start_cycles = start_cycles[:min_len]
                end_cycles = end_cycles[:min_len]

            # Calculate differences for each measurement
            for idx, (start_cycle, end_cycle) in enumerate(zip(start_cycles, end_cycles)):
                cycle_diff = end_cycle - start_cycle

                results.append({
                    'iteration': iteration,
                    'workqueue_type': workqueue_type,
                    'affinity': affinity,
                    'timestamp': timestamp,
                    'measurement_idx': idx,
                    'start_cycle': start_cycle,
                    'end_cycle': end_cycle,
                    'cycle_diff': cycle_diff
                })

        return pd.DataFrame(results)

    def generate_summary_report(self, diff_df: pd.DataFrame, output_dir: Path):
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


def ploottt(df: pd.DataFrame):
    import math
    # Get all unique (workqueue_type, affinity) combinations
    groups = list(df.groupby(['workqueue_type', 'affinity']).groups.keys())

    # Determine subplot grid size (e.g., 2 columns)
    n = len(groups)
    cols = 2
    rows = math.ceil(n / cols)

    fig, axes = plt.subplots(rows, cols, figsize=(
        cols * 6, rows * 4), squeeze=False)

    # Plot each group in its subplot
    for idx, ((wq_type, affinity), group_df) in enumerate(df.groupby(['workqueue_type', 'affinity'])):
        r, c = divmod(idx, cols)
        ax = axes[r][c]

        # Compute aggregated stats
        agg_df = group_df.groupby('iteration')['cycle_diff'].agg(
            ['min', 'median', 'max']).reset_index()

        # Plot on the subplot axis
        ax.plot(agg_df['iteration'], agg_df['min'],
                label='Min', linestyle='dotted', marker='o')
        ax.plot(agg_df['iteration'], agg_df['median'],
                label='Median', linestyle='solid', marker='o')
        ax.plot(agg_df['iteration'], agg_df['max'],
                label='Max', linestyle='dashed', marker='o')

        print(f'{wq_type} | {affinity}')
        print(agg_df)
        print('' + '-' * 40)

        ax.set_title(f'{wq_type} | {affinity}')
        ax.set_xlabel('Iteration')
        ax.set_ylabel('Cycle Diff')
        ax.grid(True)
        ax.legend()

    # Hide unused subplots (if any)
    for i in range(n, rows * cols):
        r, c = divmod(i, cols)
        fig.delaxes(axes[r][c])

    fig.suptitle(
        'Cycle Diff per Iteration (per Workqueue Type & Affinity)', fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.97])  # leave space for the suptitle
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

    print("Loading data files...")
    grouped_data = analyzer.load_data_files()
    print(f"Found {len(grouped_data)} test configurations")

    if not grouped_data:
        print("No valid data files found!")
        return

    print("Calculating cycle differences...")
    diff_df = analyzer.calculate_cycle_differences(grouped_data)

    # .......................................

    print("Generating summary report...")
    analyzer.generate_summary_report(diff_df, output_dir)

    ploottt(diff_df)

    print(f"Analysis complete! Results saved to {output_dir}")


if __name__ == "__main__":
    main()
