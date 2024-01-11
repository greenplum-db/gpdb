"""
Reset GUC when cluster is down.

This module works as follows:

1. Read the log of the last run of gpconfig with `--verbose` to get the
    hosts and data directories of all the segments of the cluster.
2. Connect to each host with SSH and call `gpconfig_helper.py` to reset
    (a.k.a. remove) the GUC in postgresql.conf

Currently, only `--remove` is available for safety, since we cannot
validate the GUC setting when the cluster is down.

Please refer to `python3 gpconfig_offline.py --help` for usage.
"""
from pathlib import Path
from typing import Dict, List, Optional
import datetime
import os
import subprocess
import argparse


def get_latest_gpconfig_log_file() -> Path:
    gp_admin_logs = Path(os.path.expanduser("~/gpAdminLogs"))
    latest_gpconfig_date = None
    latest_gpconfig_log = None
    for file_path in gp_admin_logs.iterdir():
        filename = os.path.basename(file_path).split(".")[0]
        program_name, date = filename.split("_")
        if program_name != "gpconfig":
            continue
        date = datetime.datetime.strptime(date, r"%Y%m%d").date()
        if latest_gpconfig_date is None or latest_gpconfig_date < date:
            latest_gpconfig_date = date
            latest_gpconfig_log = file_path
    return latest_gpconfig_log


def parse_timestamp(line: str) -> str:
    return line.split(" ")[0]


def gather_logs_by_timestamp(log_path: Path) -> Dict[str, List[str]]:
    with open(log_path) as log:
        gathered_logs: Dict[str, List[str]] = {}
        # Read the log from latest to oldest.
        for line in reversed(log.readlines()):
            timestamp = parse_timestamp(line)
            if timestamp not in gathered_logs:
                gathered_logs[timestamp] = [line]
            else:
                gathered_logs[timestamp].append(line)
        for timestamp in gathered_logs:
            gathered_logs[timestamp] = reversed(gathered_logs[timestamp])
        return gathered_logs


def parse_host_and_data_dir(line: str):
    _, message = line.split(":-")
    host, data_dir = None, None
    for attr in message.split(" "):
        if "=" not in message:
            continue
        k, v = attr.strip().split("=")
        if k == "host":
            host = v
        if k == "dir":
            data_dir = v
    return host, data_dir


def search_for_data_dir(
    gathered_logs: Dict[str, List[str]]
) -> Optional[Dict[str, List[str]]]:
    # Iterate in insertion order, i.e. from latest to oldest.
    for _, log in gathered_logs.items():
        data_dir_config: Dict[str, List[str]] = {}
        for line in log:
            host, data_dir = parse_host_and_data_dir(line)
            if host is None:
                assert data_dir is None
                continue
            if host not in data_dir_config:
                data_dir_config[host] = [data_dir]
            else:
                data_dir_config[host].append(data_dir)
        if len(data_dir_config) > 0:
            return data_dir_config
    return None


def remove_guc(config_name: str, data_dir_config: Dict[str, List[str]]):
    for host, data_dir_list in data_dir_config.items():
        for data_dir in data_dir_list:
            GPHOME = os.environ['GPHOME']
            assert GPHOME != ""
            try:
                subprocess.check_output(
                    [
                        "ssh",
                        host,
                        f"source {GPHOME}/greenplum_path.sh && {GPHOME}/sbin/gpconfig_helper.py --remove-parameter={config_name} --file={data_dir}/postgresql.conf",
                    ],
                    stderr=subprocess.STDOUT,
                    text=True,
                )
            except subprocess.CalledProcessError as exc:
                raise exc from Exception(exc.stdout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-r", "--remove")
    args = parser.parse_args()

    data_dir_config = search_for_data_dir(
        gather_logs_by_timestamp(get_latest_gpconfig_log_file())
    )
    # print("data_dir_config =", data_dir_config)
    assert data_dir_config is not None, "Cannot find data directories in gpAdminLongs."
    remove_guc(args.remove, data_dir_config)


if __name__ == "__main__":
    main()
