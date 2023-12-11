CREATE OR REPLACE FUNCTION create_virtual_env(
    prefix text, 
    manager text, 
    current_ts int8
) RETURNS text AS $$
import sys
from pathlib import Path
from subprocess import check_output, STDOUT, CalledProcessError

env_name = f'{manager}_{hex(current_ts)}'
env_dir = Path(prefix) / env_name
try:
    env_dir.mkdir(parents=True, exist_ok=False)
    stdout = check_output(
        [sys.executable, '-m', manager, str(env_dir)], stderr=STDOUT, text=True)
    return env_name
except FileExistsError as e:
    return None
except CalledProcessError as e:
    plpy.notice(e.output)
    raise
$$ LANGUAGE plpython3u;

WITH create_virtual_env AS (
    SELECT create_virtual_env('/tmp/plpython3', 'venv', extract(epoch from now())::int8) AS env_name
    UNION ALL
    SELECT create_virtual_env('/tmp/plpython3', 'venv', extract(epoch from now())::int8) AS env_name
    FROM gp_dist_random('gp_id')
)
SELECT * FROM create_virtual_env
WHERE env_name IS NOT NULL \gset

\connect

SET plpython3.virtual_env = :'env_name';

CREATE OR REPLACE FUNCTION test_path_added(virtual_env_name text) 
RETURNS TEXT AS $$
import sys

assert sys.prefix == f"/tmp/plpython3/{virtual_env_name}"
assert sys.exec_prefix == f"/tmp/plpython3/{virtual_env_name}"
assert sys.executable == f"/tmp/plpython3/{virtual_env_name}/bin/python"
assert sys.base_prefix == f"/tmp/plpython3/{virtual_env_name}"
assert sys.base_exec_prefix == f"/tmp/plpython3/{virtual_env_name}"
assert sys._base_executable == f"/tmp/plpython3/{virtual_env_name}/bin/python"
return "SUCCESS"
$$ language plpython3u;

SELECT DISTINCT * FROM (
    SELECT test_path_added(:'env_name')
    UNION ALL
    SELECT test_path_added(:'env_name') FROM gp_dist_random('gp_id')
) t;

SET plpython3.virtual_env = :'env_name';

CREATE OR REPLACE FUNCTION test_import(name TEXT) 
RETURNS text AS $$
import importlib

try:
    return importlib.import_module(name)
except ModuleNotFoundError as e:
    return e.msg
$$ language plpython3u;

SELECT DISTINCT * FROM
(
    SELECT test_import('numpy')
    UNION ALL
    SELECT test_import('numpy') from gp_dist_random('gp_id')
) t;

CREATE OR REPLACE FUNCTION pip_install(name TEXT) 
RETURNS text AS $$
import os
import sys
from pathlib import Path
from subprocess import check_output, STDOUT

lock_path = Path(sys.prefix) / "pip.lock"
try:
    lock_file = open(lock_path, "x")
    stdout = check_output(
        [sys.executable, '-m', 'pip', 'install', name], stderr=STDOUT, text=True)
    lock_file.close()
    os.remove(lock_path)
    return stdout
except FileExistsError as e:
    return None
except CalledProcessError as e:
    plpy.notice(e.output)
    raise
$$ language plpython3u;

WITH pip_install AS (
    SELECT pip_install('numpy') AS stdout
    UNION ALL
    SELECT pip_install('numpy') AS stdout
    FROM gp_dist_random('gp_id')
), install_error AS (
    SELECT * FROM pip_install
    WHERE NOT (
        (stdout IS NULL) OR 
        (stdout LIKE '%Successfully installed numpy%') OR
        (stdout LIKE 'Requirement already satisfied: numpy%')
    )
)
SELECT NOT EXISTS (SELECT * FROM install_error) AS success;

SELECT DISTINCT * FROM
(
    SELECT test_import('numpy')
    UNION ALL
    SELECT test_import('numpy') from gp_dist_random('gp_id')
) t;
