import datetime
import tarfile


def _get_timestamp():
    now = datetime.datetime.now()
    return now.strftime("%Y%m%d%H%M%S")


def _get_failed_job_archive_filename(output_dir):
    return '{path}/failed-job-logs-{timestamp}.tar.gz'.format(
        path=output_dir,
        timestamp=_get_timestamp()
    )
   

def collect_failed_job(path, output_dir):
    with tarfile.open(_get_failed_job_archive_filename(output_dir), mode="w:gz") as archive:
        archive.add(path, recursive=True)

