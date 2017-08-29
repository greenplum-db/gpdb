import os
import sys
import pickle

from gppylib import gplog
from gppylib.commands.base import OperationWorkerPool, Command, REMOTE
from gppylib.operations import Operation

DEFAULT_NUM_WORKERS = 64
logger = gplog.get_default_logger()

class RemoteOperation(Operation):
    # TODO: The Operation that is run remotely cannot return Exceptions. 
    # This can be resolved easily with a class that wraps the exception: ExceptionCapsule. (Thank you, Pyro.)
    # TODO: Remote traceback is lost. Again, this can be solved by embedding remote traceback in an ExceptionCapsule.
    """ 
    RemoteOperation communicates w/ gpoperation.py on the remote end, with the following assumptions.
    1) gppylib exists
    2) gpoperation.py can see gppylib as a top-level module
    3) obj is defined at the top level of its module
       This requirement actually arises out of an underlying pickle issue, which in turn, appears
       to result from a python class oddity. If class B is defined within class A, it does not consider
       A to be its module. B is merely a class that is an attribute of A. For this reason, once instantiated,
       B cannot be rebuilt from its module name and class name alone. Its outer class A is a missing piece
       of information that gppickle cannot attain from python internals.
    4) Most importantly, the operation provided here must be imported into the gppylib... namespace. Otherwise,
       gpoperation.py will be unable to deserialize and import it on the remote end.

       In the normal gppylib use case, some bin/ level script will use an absolute import to bring in something
       from gppylib. In this manner, any ensuing imports (even if they're relative) will still be imported into the
       gppylib namespace. Thus, pickling such objects over ssh to gpoperation.py will succeed, because such objects
       will be immediately importable on the remote end.

       However, there is exactly one edge case: unit testing. If a unit test is invoked directly through CLI, its objects
       reside in the __main__ module as opposed to gppylib.test_something. Again, this can be circumvented by invoking unit tests
       through PyUnit or python -m unittest, etc. 
    """
    def __init__(self, operation, host):
        super(RemoteOperation, self).__init__()
        self.operation = operation
        self.host = host
    def execute(self):
        execname = os.path.split(sys.argv[0])[-1]
        pickled_execname = pickle.dumps(execname) 
        pickled_operation = pickle.dumps(self.operation)
        cmd = Command('pickling an operation', '$GPHOME/sbin/gpoperation.py',
                      ctxt=REMOTE, remoteHost=self.host, stdin = pickled_execname + pickled_operation)
        cmd.run(validateAfter=True)
        logger.debug(cmd.get_results().stdout)
        ret = self.operation.ret = pickle.loads(cmd.get_results().stdout)
        if isinstance(ret, Exception):
            raise ret
        return ret
    def __str__(self):
        return "Remote(%s)" % str(self.operation)


class ParallelOperation(Operation):
    """ 
    Caveat: execute returns None. It is the caller's responsibility to introspect operations.
    """
    def __init__(self, operations, max_parallelism=DEFAULT_NUM_WORKERS):
        super(ParallelOperation, self).__init__()
        self.operations = operations        
        self.parallelism = min(len(operations), max_parallelism)

    def execute(self):
        if not self.operations or len(self.operations) == 0:
            return
        pool = OperationWorkerPool(numWorkers=self.parallelism, operations=self.operations)
        pool.join()
        pool.haltWork()

    def __str__(self):
        return "Parallel(%d)" % len(self.operations)


class SerialOperation(Operation):
    """ 
    Caveat: All operations must succeed. SerialOperation will raise first exception encountered.
    """
    def __init__(self, operations):
        super(SerialOperation, self).__init__()
        self.operations = operations
    def execute(self):
        return [operation.run() for operation in self.operations]
    def __str__(self):
        return "Serial(%d)" % len(self.operations)

class MasterOperation(Operation):
    def __init__(self, operation):
        super(MasterOperation, self).__init__()
        self.operation = operation
    def execute(self):
        # TODO: check that we're running on master
        pass

if __name__ == "__main__":
    import sys 
    from unix import CheckFile, CheckRemoteFile
    print RemoteOperation(CheckFile(sys.argv[1]), "localhost").run()
    print CheckRemoteFile(sys.argv[1], "localhost").run()
