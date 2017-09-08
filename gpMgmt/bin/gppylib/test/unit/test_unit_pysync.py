import os, tempfile, hashlib
from gp_unittest import *
from lib.pysync import LocalPysync

CONTENT = 'To be or not to be.'

class LocalPySyncTestCase(GpTestCase):

    def setUp(self):
        self.subject = LocalPysync(['pysync', '/tmp', 'localhost:/tmp'])

    def tearDown(self):
        super(LocalPySyncTestCase, self).tearDown()

    def test_doCommand_getDigest(self):
        filename = ''
        try:
            filename, filesize = createTempFileWithContent(CONTENT)
            doCommand_digest = self.subject.doCommand(['getDigest', filename, 0, filesize])
            m = hashlib.sha256()
            m.update(CONTENT)
            self.assertTrue(m.digest() == doCommand_digest)
        finally:
            os.unlink(filename)

def createTempFileWithContent(content):
    fd = tempfile.NamedTemporaryFile(delete=False)
    fd.write(content)
    filename = fd.name
    fd.close()
    filesize = os.stat(filename).st_size

    return filename, filesize



if __name__ == '__main__':
    run_tests()

