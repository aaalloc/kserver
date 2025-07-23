import re
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Optional
import pandas as pd
from enum import Enum


class WQXPEnum(Enum):
    INSERT_EXEC = 'insert-exec'
    EXEC_TIME_PRED = 'exec-time-pred'


class WQCycleAnalyzer:

    def __init__(self, data_dir: Path):
        self.data_dir = Path(data_dir)
        self.data_cache = {}
        self.parsed_files = []
        # glob wq-* and first value is type, so if first file is wq-exec-time-pred then xp_type is EXEC_TIME_PRED
        first_file = next(iter(self.data_dir.glob('wq-*')), None)
        if first_file:
            if WQXPEnum.EXEC_TIME_PRED.value in first_file.name:
                self.xp_type = WQXPEnum.EXEC_TIME_PRED
            elif WQXPEnum.INSERT_EXEC.value in first_file.name:
                self.xp_type = WQXPEnum.INSERT_EXEC
            else:
                raise ValueError(
                    f"Unknown workqueue type in file name: {first_file.name}")
        else:
            raise ValueError(
                "No workqueue files found in the specified directory.")

    def parse_filename(self, filename: str) -> Optional[Dict[str, str]]:
        """
        Parse filename following convention:
        wq-...-{iteration}-{bounded|unbound}-{low_affinity|high_affinity}-{timestamp}.{start|end}
        """
        pattern = r"wq-{type}-(\d+)-(bounded|unbound)-(low_affinity|high_affinity)-(.+)\.(start|end)".format(
            type=self.xp_type.value
        )
        match = re.match(pattern, filename)
        if match:
            match self.xp_type.value:
                case WQXPEnum.INSERT_EXEC.value:
                    return {
                        'iteration': int(match.group(1)),
                        'workqueue_type': match.group(2),
                        'affinity': match.group(3),
                        'timestamp': match.group(4),
                        'cycle_type': match.group(5)
                    }
                case WQXPEnum.EXEC_TIME_PRED.value:
                    return {
                        'matrix_op': int(match.group(1)),
                        'workqueue_type': match.group(2),
                        'affinity': match.group(3),
                        'timestamp': match.group(4),
                        'cycle_type': match.group(5),
                    }
                case _:
                    print(f"Unknown workqueue type: {self.xp_type.value}")
                    return None
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
        # file start with wq and end with .start or .end
        files = list(self.data_dir.glob('wq-*'))

        grouped_data = defaultdict(lambda: {'start': None, 'end': None})

        for file_path in files:
            parsed = self.parse_filename(file_path.name)
            if not parsed:
                print(f"Skipping file with invalid name: {file_path.name}")
                continue
            # Create a key for grouping (iteration, workqueue_type, affinity, timestamp)
            match self.xp_type.value:
                case WQXPEnum.INSERT_EXEC.value:
                    key = (parsed['iteration'], parsed['workqueue_type'],
                           parsed['affinity'], parsed['timestamp'])
                case WQXPEnum.EXEC_TIME_PRED.value:
                    key = (parsed['matrix_op'], parsed['workqueue_type'],
                           parsed['affinity'], parsed['timestamp'])
                case _:
                    print(f"Unknown workqueue type: {self.xp_type.value}")
                    continue

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
            match self.xp_type.value:
                case WQXPEnum.INSERT_EXEC.value:
                    iteration, workqueue_type, affinity, timestamp = key
                case WQXPEnum.EXEC_TIME_PRED.value:
                    matrix_op, workqueue_type, affinity, timestamp = key
                case _:
                    print(f"Unknown workqueue type: {self.xp_type.value}")
                    continue

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
                    'workqueue_type': workqueue_type,
                    'affinity': affinity,
                    'timestamp': timestamp,
                    'measurement_idx': idx,
                    'start_cycle': start_cycle,
                    'end_cycle': end_cycle,
                    # Convert cycles to seconds, hardcoded value: 2.3 GHz from troll node / grid5000,
                    'time_taken': cycle_diff / 23e8,
                    'cycle_diff': cycle_diff,
                    'idx': idx,
                    'normalized_cycle_diff': cycle_diff / (idx + 1)
                })
                match self.xp_type.value:
                    case WQXPEnum.INSERT_EXEC.value:
                        results[-1]['iteration'] = iteration
                    case WQXPEnum.EXEC_TIME_PRED.value:
                        results[-1]['matrix_op'] = matrix_op

        return pd.DataFrame(results)

    def analyze(self) -> Optional[pd.DataFrame]:
        grouped_data = self.load_data_files()
        print(
            f"Loaded {len(grouped_data)} configurations from {len(self.parsed_files)} files.")
        if not grouped_data:
            print("No valid data found. Exiting analysis.")
            return
        diff_df = self.calculate_cycle_differences(grouped_data)
        if diff_df.empty:
            print("No cycle differences calculated. Exiting analysis.")
            return
        print(f"Calculated cycle differences for {len(diff_df)} measurements.")
        return diff_df
