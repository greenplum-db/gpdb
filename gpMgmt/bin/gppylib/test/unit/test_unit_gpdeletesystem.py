import imp, os
from optparse import Values
import tempfile
import shutil
from .gp_unittest import *
from mock import *

class GpDeleteSystemTestCase(GpTestCase):
    def setUp(self):
        # because gpdeletesystem does not have a .py extension, we have to use imp to import it
        # if we had a gpdeletesystem.py, this is equivalent to:
        #   import gpdeletesystem
        #   self.subject = gpdeletesystem
        gpdeletesystem_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpdeletesystem")
        self.subject = imp.load_source('gpdeletesystem', gpdeletesystem_file)
        self.tmpDir = tempfile.mkdtemp()
        os.chmod(self.tmpDir, 0o777)
        self.options = Values()
        setattr(self.options, 'coordinator_data_dir', self.tmpDir)

    def tearDown(self):
        shutil.rmtree(self.tmpDir)

    def test_check_dump_files_exist(self):
        os.mkdir(os.path.join(self.tmpDir, 'db_dumps'))
        self.assertEqual(self.subject.check_for_dump_files(self.options), True)

    def test_check_no_dump_files(self):
        os.mkdir(os.path.join(self.tmpDir, 'doesntmatch'))
        self.assertEqual(self.subject.check_for_dump_files(self.options), False)

    def test_check_backup_files_exist(self):
        os.mkdir(os.path.join(self.tmpDir, 'backups'))
        self.assertEqual(self.subject.check_for_dump_files(self.options), True)

    def test_delete_cluster_backups_exist_noforce_fails(self):
        setattr(self.options, 'force', '')
        os.mkdir(os.path.join(self.tmpDir, 'backups'))
        with self.assertRaises(self.subject.GpDeleteSystemException) as context:
            self.subject.delete_cluster(self.options)

        self.assertTrue('Backup files exist' in context.exception.message)

    def test_delete_cluster_dumps_exist_noforce_fails(self):
        setattr(self.options, 'force', '')
        os.mkdir(os.path.join(self.tmpDir, 'db_dumps'))
        with self.assertRaises(self.subject.GpDeleteSystemException) as context:
            self.subject.delete_cluster(self.options)

        self.assertTrue('Backup files exist' in context.exception.message)

    def test_delete_cluster_force_failed_to_get_gparray(self):
        setattr(self.options, 'force', True)
        setattr(self.options, 'pgport', 5432)
        os.mkdir(os.path.join(self.tmpDir, 'backups'))
        os.mkdir(os.path.join(self.tmpDir, 'db_dumps'))
        with self.assertRaises(self.subject.GpDeleteSystemException) as context:
            self.subject.delete_cluster(self.options)

        self.assertTrue('Failed to get database configuration' in context.exception.message)
