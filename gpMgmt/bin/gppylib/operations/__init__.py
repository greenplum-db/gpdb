# Make sure Python loads the modules of this package via absolute paths.
from os.path import abspath as _abspath
from gppylib.commands.base import WorkerPool
from gppylib import gplog
__path__[0] = _abspath(__path__[0])

logger = gplog.get_default_logger()
class Operation(object):
    """
    An Operation (abstract class) is one atomic unit of work.
    
    An implementation of Operation is fully defined by:
        __init__(self, opts, args) - opts, args are intended to be validated and marshaled, as appropriate
        execute(self) - the actual Operation logic commences
        cleanup(self) - [optional] any cleanup operations; e.g. stopping a WorkerPool
        undo(self) - ?
        progress(self) - ?
        validate(self) - ?
        cancel(self) - ?

    By decomposing work into these Operation classes whenever possible, 
    we should be able to reap several benefits: 
        1. Unit testing - It's easier to build unit tests around functions 
                          that are not tightly bound to a monolithic class
        2. Composition  - We should be able to compose Operations in a useful manner. 
                          Consider ParallelOperation, RemoteOperation, SerialOperation, etc.
        3. Readability  - We no longer have to create nested, segment-facing scripts 
                          to perform work on segment hosts: e.g. figuring out how 
                          gpstart -> startSegments -> gpsegstart.py propagates information is 
                          unnecessarily confusing.
        4. Reusability  - This makes core functionality more accessible to other clients 
                          of this code; e.g. a Python web backend
        5. Complexity   - It's easier to grasp what an Operation does by examining 
                          the flowchart of Operations it invokes 

    Caveat: Operations cannot return Exceptions. RemoteOperation and ParallelOperation 
            would interpret that as a raised Exception.
    """
    def __init__(self):
        self.ret = None

    def execute(self):
        raise NotImplementedError("Operation.execute(self) must be implemented.")
    def cleanup(self):
        if hasattr(self,'pool') and self.pool and isinstance(self.pool, WorkerPool):
            self.pool.haltWork()

    def run(self):
        # TODO: logging, just like Program.run
        # TODO: async argument to allow caller to monitor or to provide progress updates
        logger.debug('Starting %s' % self.__class__.__name__)
        try:
            self.ret = self.execute()
            return self.ret
        except Exception as e:
            self.ret = e
            raise
        finally:
            self.cleanup()
            logger.debug('Ending %s' % self.__class__.__name__)
    def __str__(self):
        return self.__class__.__name__
    def get_ret(self):
        if isinstance(self.ret, Exception):
            raise self.ret
        return self.ret
