import os

from gp_unittest import *
from gppylib.mainUtils import PIDLockFile, PIDLockHeld


class MainUtilsTestCase(GpTestCase):

    def setUp(self):
        # parent PID
        self.ppid = os.getpid()
        self.lockfile = "/tmp/lock-%s" % (str(self.ppid))
        self.lock = PIDLockFile(self.lockfile)

    def test_release_removes_lock(self):
        self.lock.acquire()
        self.assertEquals(True,os.path.exists(self.lockfile))
        self.lock.release()
        self.assertEquals(False, os.path.exists(self.lockfile))

    def test_with_block_removes_lock(self):
        with self.lock:
            self.assertEquals(True,os.path.exists(self.lockfile))
        self.assertEquals(False, os.path.exists(self.lockfile))

    def test_lock_owned_by_parent(self):
        with self.lock as l:
            self.assertEquals(l.read_pid(), self.ppid)


    def test_exceptionPIDLockHeld_if_same_pid(self):
        with self.lock:
            with self.assertRaises(PIDLockHeld, message="PIDLock already held at %s" % (self.lockfile)):
                self.lock.acquire()

    def test_child_can_read_lock_owner(self):
        with self.lock as l:
            pid = os.fork()
            if pid == 0:
                self.assertEquals(l.read_pid(), self.ppid)
                os._exit(0)
            else:
                os.wait()

    def test_exceptionPIDLockHeld_if_different_pid(self):
        self.lock.acquire()
        pid = os.fork()
        # if child, os.fork() == 0
        if pid == 0:
            with self.assertRaises(PIDLockHeld, message="PIDLock already held at %s" % (self.lockfile)):
                self.lock.acquire()
            os._exit(0)
        else:
            os.wait()
            self.lock.release()

    def test_childPID_can_not_remove_parent_lock(self):
        with self.lock:
            pid = os.fork()
            if pid == 0:
                newlock = PIDLockFile(self.lockfile)
                try:
                    with newlock:
                        # the __exit__ or __del__ of this with
                        # would normally call release - but this
                        # PID does not own the lock
                        pass
                # we expect the the acquire to fail
                except:
                    pass
                self.assertEquals(True, os.path.exists(self.lockfile))
                os._exit(0)
            else:
                os.wait()




if __name__ == '__main__':
    run_tests()
