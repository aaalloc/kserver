#!/usr/bin/env python3
"""
Script pour analyser les données de workqueue avec pandas et générer des graphiques linéaires.
Charge les 3 types de données JSON et crée des visualisations détaillées.
"""

import pandas as pd
import matplotlib.pyplot as plt
import json
import argparse
import sys
from pathlib import Path
import numpy as np


def load_json_data(file_path):
    """Load JSON data from file and return as DataFrame-friendly format."""
    try:
        with open(file_path, 'r') as f:
            data = json.load(f)
        print(f"Loaded {len(data)} entries from {file_path}")
        return data
    except Exception as e:
        print(f"Error loading {file_path}: {e}")
        return []


def parse_total_workers_per_cpu(data):
    """
    Parse total_workers_per_cpu data into pandas DataFrame.
    
    Input format:
    {
        iteration: 1,
        0: {'idle': 5, 'active': 10, 'total_workers': 15},
        1: {'idle': 3, 'active': 7, 'total_workers': 10},
        ...
    }
    """
    rows = []
    for entry in data:
        if 'iteration' not in entry:
            continue
            
        iteration = entry['iteration']
        for cpu_id, cpu_data in entry.items():
            if cpu_id == 'iteration':
                continue
            
            try:
                cpu_id_int = int(cpu_id)
                rows.append({
                    'iteration': iteration,
                    'cpu_id': cpu_id_int,
                    'idle': cpu_data.get('idle', 0),
                    'active': cpu_data.get('active', 0),
                    'total_workers': cpu_data.get('total_workers', 0)
                })
            except (ValueError, TypeError):
                continue
    
    return pd.DataFrame(rows)


def parse_workqueue_workers_per_cpu(data):
    """
    Parse total_workers_from_workqueue_per_cpu data into pandas DataFrame.
    
    Input format:
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
    rows = []
    for entry in data:
        if 'iteration' not in entry:
            continue
            
        iteration = entry['iteration']
        for wq_name, wq_data in entry.items():
            if wq_name == 'iteration':
                continue
                
            if 'cpu' in wq_data:
                for cpu_id, cpu_data in wq_data['cpu'].items():
                    try:
                        cpu_id_int = int(cpu_id)
                        rows.append({
                            'iteration': iteration,
                            'workqueue': wq_name,
                            'cpu_id': cpu_id_int,
                            'idle': cpu_data.get('idle', 0),
                            'active': cpu_data.get('active', 0),
                            'total_workers': cpu_data.get('total_workers', 0)
                        })
                    except (ValueError, TypeError):
                        continue
    
    return pd.DataFrame(rows)


def parse_work_items_per_cpu(data):
    """
    Parse total_work_items_per_cpu data into pandas DataFrame.
    
    Input format:
    {
        0: {'started': 5, 'completed': 10, 'current': 5},
        1: {'started': 3, 'completed': 7, 'current': 4},
        ...
    }
    """
    rows = []
    for entry in data:
        if 'iteration' not in entry:
            continue
            
        iteration = entry['iteration']
        for cpu_id, cpu_data in entry.items():
            if cpu_id == 'iteration':
                continue
            
            try:
                cpu_id_int = int(cpu_id)
                rows.append({
                    'iteration': iteration,
                    'cpu_id': cpu_id_int,
                    'started': cpu_data.get('started', 0),
                    'completed': cpu_data.get('completed', 0),
                    'current': cpu_data.get('current', 0)
                })
            except (ValueError, TypeError):
                continue
    
    return pd.DataFrame(rows)


def plot_total_workers_per_cpu(df, output_dir):
    """Create line charts for total workers per CPU."""
    if df.empty:
        print("No total workers per CPU data to plot")
        return
    
    # Get unique CPUs
    cpus = sorted(df['cpu_id'].unique())
    
    # Create subplots for total, idle, and active workers
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 12))
    
    colors = plt.cm.tab10(np.linspace(0, 1, len(cpus)))
    
    # Plot total workers
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax1.plot(cpu_data['iteration'], cpu_data['total_workers'], 
                 label=f'CPU {cpu}', marker='o', color=colors[i])
    
    ax1.set_title('Total Workers per CPU')
    ax1.set_xlabel('Iteration')
    ax1.set_ylabel('Total Workers')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True)
    
    # Plot idle workers
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax2.plot(cpu_data['iteration'], cpu_data['idle'], 
                 label=f'CPU {cpu}', marker='s', color=colors[i])
    
    ax2.set_title('Idle Workers per CPU')
    ax2.set_xlabel('Iteration')
    ax2.set_ylabel('Idle Workers')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.grid(True)
    
    # Plot active workers
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax3.plot(cpu_data['iteration'], cpu_data['active'], 
                 label=f'CPU {cpu}', marker='^', color=colors[i])
    
    ax3.set_title('Active Workers per CPU')
    ax3.set_xlabel('Iteration')
    ax3.set_ylabel('Active Workers')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax3.grid(True)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'total_workers_per_cpu_pandas.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Total workers per CPU plot saved to {output_dir / 'total_workers_per_cpu_pandas.png'}")


def plot_workqueue_workers_per_cpu(df, output_dir):
    """Create line charts for workqueue workers per CPU."""
    if df.empty:
        print("No workqueue workers per CPU data to plot")
        return
    
    # Get unique workqueues
    workqueues = sorted(df['workqueue'].unique())
    
    # 1. Plot total workers per workqueue (aggregated across all CPUs)
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 18))
    
    colors = plt.cm.tab10(np.linspace(0, 1, len(workqueues)))
    
    # Aggregate by workqueue across all CPUs
    for i, wq in enumerate(workqueues):
        wq_data = df[df['workqueue'] == wq].groupby('iteration').agg({
            'total_workers': 'sum',
            'idle': 'sum',
            'active': 'sum'
        }).reset_index()
        
        ax1.plot(wq_data['iteration'], wq_data['total_workers'], 
                 label=f'{wq}', marker='o', color=colors[i])
        ax2.plot(wq_data['iteration'], wq_data['idle'], 
                 label=f'{wq}', marker='s', color=colors[i])
        ax3.plot(wq_data['iteration'], wq_data['active'], 
                 label=f'{wq}', marker='^', color=colors[i])
    
    ax1.set_title('Total Workers per Workqueue (All CPUs)')
    ax1.set_xlabel('Iteration')
    ax1.set_ylabel('Total Workers')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True)
    
    ax2.set_title('Idle Workers per Workqueue (All CPUs)')
    ax2.set_xlabel('Iteration')
    ax2.set_ylabel('Idle Workers')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.grid(True)
    
    ax3.set_title('Active Workers per Workqueue (All CPUs)')
    ax3.set_xlabel('Iteration')
    ax3.set_ylabel('Active Workers')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax3.grid(True)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'workqueue_workers_aggregated_pandas.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Workqueue workers aggregated plot saved to {output_dir / 'workqueue_workers_aggregated_pandas.png'}")
    
    # 2. Create detailed plots for each workqueue showing per-CPU breakdown
    for wq in workqueues:
        wq_df = df[df['workqueue'] == wq]
        cpus = sorted(wq_df['cpu_id'].unique())
        
        if len(cpus) == 0:
            continue
            
        fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 18))
        colors = plt.cm.tab20(np.linspace(0, 1, len(cpus)))
        
        # Plot per CPU for this workqueue
        for i, cpu in enumerate(cpus):
            cpu_data = wq_df[wq_df['cpu_id'] == cpu]
            ax1.plot(cpu_data['iteration'], cpu_data['total_workers'], 
                     label=f'CPU {cpu}', marker='o', color=colors[i])
            ax2.plot(cpu_data['iteration'], cpu_data['idle'], 
                     label=f'CPU {cpu}', marker='s', color=colors[i])
            ax3.plot(cpu_data['iteration'], cpu_data['active'], 
                     label=f'CPU {cpu}', marker='^', color=colors[i])
        
        ax1.set_title(f'{wq} - Total Workers per CPU')
        ax1.set_xlabel('Iteration')
        ax1.set_ylabel('Total Workers')
        ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax1.grid(True)
        
        ax2.set_title(f'{wq} - Idle Workers per CPU')
        ax2.set_xlabel('Iteration')
        ax2.set_ylabel('Idle Workers')
        ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax2.grid(True)
        
        ax3.set_title(f'{wq} - Active Workers per CPU')
        ax3.set_xlabel('Iteration')
        ax3.set_ylabel('Active Workers')
        ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax3.grid(True)
        
        plt.tight_layout()
        
        # Safe filename
        safe_wq_name = wq.replace('/', '_').replace(' ', '_')
        plt.savefig(output_dir / f'workqueue_{safe_wq_name}_per_cpu_pandas.png', 
                    dpi=300, bbox_inches='tight')
        plt.close()
        print(f"Workqueue {wq} per CPU plot saved to {output_dir / f'workqueue_{safe_wq_name}_per_cpu_pandas.png'}")


def plot_work_items_per_cpu(df, output_dir):
    """Create line charts for work items per CPU."""
    if df.empty:
        print("No work items per CPU data to plot")
        return
    
    # Get unique CPUs
    cpus = sorted(df['cpu_id'].unique())
    
    # Create subplots for started, completed, and current work items
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 12))
    
    colors = plt.cm.tab10(np.linspace(0, 1, len(cpus)))
    
    # Plot started work items
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax1.plot(cpu_data['iteration'], cpu_data['started'], 
                 label=f'CPU {cpu}', marker='o', color=colors[i])
    
    ax1.set_title('Started Work Items per CPU')
    ax1.set_xlabel('Iteration')
    ax1.set_ylabel('Started Work Items')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True)
    
    # Plot completed work items
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax2.plot(cpu_data['iteration'], cpu_data['completed'], 
                 label=f'CPU {cpu}', marker='s', color=colors[i])
    
    ax2.set_title('Completed Work Items per CPU')
    ax2.set_xlabel('Iteration')
    ax2.set_ylabel('Completed Work Items')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.grid(True)
    
    # Plot current work items
    for i, cpu in enumerate(cpus):
        cpu_data = df[df['cpu_id'] == cpu]
        ax3.plot(cpu_data['iteration'], cpu_data['current'], 
                 label=f'CPU {cpu}', marker='^', color=colors[i])
    
    ax3.set_title('Current Work Items per CPU')
    ax3.set_xlabel('Iteration')
    ax3.set_ylabel('Current Work Items')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax3.grid(True)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'work_items_per_cpu_pandas.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Work items per CPU plot saved to {output_dir / 'work_items_per_cpu_pandas.png'}")


def create_summary_dashboard(df_workers, df_workqueue, df_work_items, output_dir):
    """Create a comprehensive dashboard with all data."""
    fig = plt.figure(figsize=(20, 15))
    
    # Create a 3x2 grid
    gs = fig.add_gridspec(3, 2, hspace=0.3, wspace=0.3)
    
    # 1. Total workers summary
    if not df_workers.empty:
        ax1 = fig.add_subplot(gs[0, 0])
        workers_summary = df_workers.groupby('iteration').agg({
            'total_workers': 'sum',
            'idle': 'sum',
            'active': 'sum'
        }).reset_index()
        
        ax1.plot(workers_summary['iteration'], workers_summary['total_workers'], 
                 label='Total', marker='o', linewidth=2)
        ax1.plot(workers_summary['iteration'], workers_summary['idle'], 
                 label='Idle', marker='s', linewidth=2)
        ax1.plot(workers_summary['iteration'], workers_summary['active'], 
                 label='Active', marker='^', linewidth=2)
        ax1.set_title('Total Workers Summary (All CPUs)')
        ax1.set_xlabel('Iteration')
        ax1.set_ylabel('Number of Workers')
        ax1.legend()
        ax1.grid(True)
    
    # 2. Work items summary
    if not df_work_items.empty:
        ax2 = fig.add_subplot(gs[0, 1])
        work_items_summary = df_work_items.groupby('iteration').agg({
            'started': 'sum',
            'completed': 'sum',
            'current': 'sum'
        }).reset_index()
        
        ax2.plot(work_items_summary['iteration'], work_items_summary['started'], 
                 label='Started', marker='o', linewidth=2)
        ax2.plot(work_items_summary['iteration'], work_items_summary['completed'], 
                 label='Completed', marker='s', linewidth=2)
        ax2.plot(work_items_summary['iteration'], work_items_summary['current'], 
                 label='Current', marker='^', linewidth=2)
        ax2.set_title('Work Items Summary (All CPUs)')
        ax2.set_xlabel('Iteration')
        ax2.set_ylabel('Number of Work Items')
        ax2.legend()
        ax2.grid(True)
    
    # 3. Workqueue comparison
    if not df_workqueue.empty:
        ax3 = fig.add_subplot(gs[1, :])
        workqueues = sorted(df_workqueue['workqueue'].unique())
        colors = plt.cm.tab10(np.linspace(0, 1, len(workqueues)))
        
        for i, wq in enumerate(workqueues):
            wq_summary = df_workqueue[df_workqueue['workqueue'] == wq].groupby('iteration').agg({
                'total_workers': 'sum'
            }).reset_index()
            
            ax3.plot(wq_summary['iteration'], wq_summary['total_workers'], 
                     label=f'{wq}', marker='o', color=colors[i], linewidth=2)
        
        ax3.set_title('Total Workers per Workqueue Comparison')
        ax3.set_xlabel('Iteration')
        ax3.set_ylabel('Total Workers')
        ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax3.grid(True)
    
    # 4. CPU utilization heatmap
    if not df_workers.empty:
        ax4 = fig.add_subplot(gs[2, :])
        
        # Create pivot table for heatmap
        pivot_data = df_workers.pivot(index='cpu_id', columns='iteration', values='total_workers')
        
        im = ax4.imshow(pivot_data.values, cmap='viridis', aspect='auto')
        ax4.set_title('CPU Workers Heatmap')
        ax4.set_xlabel('Iteration')
        ax4.set_ylabel('CPU ID')
        ax4.set_yticks(range(len(pivot_data.index)))
        ax4.set_yticklabels(pivot_data.index)
        
        # Add colorbar
        cbar = plt.colorbar(im, ax=ax4)
        cbar.set_label('Number of Workers')
    
    plt.suptitle('Workqueue Monitoring Dashboard', fontsize=16, fontweight='bold')
    plt.savefig(output_dir / 'dashboard_pandas.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Dashboard saved to {output_dir / 'dashboard_pandas.png'}")


def main():
    parser = argparse.ArgumentParser(description='Analyze workqueue data with pandas')
    parser.add_argument('--total-workers', help='JSON file with total_workers_per_cpu data')
    parser.add_argument('--workqueue-workers', help='JSON file with total_workers_from_wq_per_cpu data')
    parser.add_argument('--work-items', help='JSON file with total_work_items_per_cpu data')
    parser.add_argument('--output-dir', default='.', help='Output directory for plots')
    parser.add_argument('--dashboard', action='store_true', help='Create comprehensive dashboard')
    parser.add_argument('--show-data', action='store_true', help='Show DataFrame info and samples')
    
    args = parser.parse_args()
    
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Load and parse data
    df_workers = pd.DataFrame()
    df_workqueue = pd.DataFrame()
    df_work_items = pd.DataFrame()
    
    if args.total_workers:
        print("Loading total workers per CPU data...")
        data = load_json_data(args.total_workers)
        if data:
            df_workers = parse_total_workers_per_cpu(data)
            if args.show_data:
                print("\nTotal Workers DataFrame:")
                print(df_workers.info())
                print(df_workers.head())
            plot_total_workers_per_cpu(df_workers, output_dir)
    
    if args.workqueue_workers:
        print("Loading workqueue workers per CPU data...")
        data = load_json_data(args.workqueue_workers)
        if data:
            df_workqueue = parse_workqueue_workers_per_cpu(data)
            if args.show_data:
                print("\nWorkqueue Workers DataFrame:")
                print(df_workqueue.info())
                print(df_workqueue.head())
            plot_workqueue_workers_per_cpu(df_workqueue, output_dir)
    
    if args.work_items:
        print("Loading work items per CPU data...")
        data = load_json_data(args.work_items)
        if data:
            df_work_items = parse_work_items_per_cpu(data)
            if args.show_data:
                print("\nWork Items DataFrame:")
                print(df_work_items.info())
                print(df_work_items.head())
            plot_work_items_per_cpu(df_work_items, output_dir)
    
    # Create dashboard if requested and we have data
    if args.dashboard and not all(df.empty for df in [df_workers, df_workqueue, df_work_items]):
        print("Creating comprehensive dashboard...")
        create_summary_dashboard(df_workers, df_workqueue, df_work_items, output_dir)
    
    print("Analysis complete!")


if __name__ == "__main__":
    main()
