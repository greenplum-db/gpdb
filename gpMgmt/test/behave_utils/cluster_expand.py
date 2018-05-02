import glob
from datetime import datetime, timedelta
from subprocess import Popen, PIPE
from utils import run_gpcommand

from gppylib.commands.base import Command

class Gpexpand:
    def __init__(self, context, working_directory=None, database='pivotal'):
        self.database = database
        self.working_directory = working_directory
        self.context = context


    def do_interview(self, hosts=None, num_of_segments=1, directory_pairs=None, has_mirrors=False):
        """
        hosts: list of hosts to expand to
        num_of_segments: number of segments to expand
        directory_pairs: list of tuple directory pairs where first element is the primary directory and the 2nd is the mirror.

        Note: this code is done with the assumption of primary only cluster.
        There is an assumption that the user knows the kind of cluster to expand.

        Returns: string output, int returncode
        """
        if directory_pairs is None:
            directory_pairs = ('/tmp/foo', '')

        if num_of_segments != len(directory_pairs):
            raise Exception("Amount of directory_pairs needs to be the same amount as the segments.")


        # If working_directory is None, then Popen will use the directory where
        # the python code is being ran.
        p1 = Popen(["gpexpand", "-D", self.database], stdout=PIPE, stdin=PIPE,
                   cwd=self.working_directory)

        # Very raw form of doing the interview part of gpexpand.
        # May be pexpect is the way to do this if we really need it to be more complex
        # Cannot guarantee that this is not flaky either.

        # Would you like to initiate a new System Expansion Yy|Nn (default=N):
        p1.stdin.write("y\n")

        # **Enter a blank line to only add segments to existing hosts**[]:
        p1.stdin.write("%s\n" % (",".join(hosts) if hosts else ""))

        if has_mirrors:
            #What type of mirroring strategy would you like? spread|grouped (default=grouped):
            p1.stdin.write("\n")

        #How many new primary segments per host do you want to add? (default=0):
        p1.stdin.write("%s\n" % num_of_segments)

        # Enter new primary data directory #<number primary segment>
        for directory in directory_pairs:
            primary, mirror = directory
            p1.stdin.write("%s\n" % primary)
            if mirror:
                p1.stdin.write("%s\n" % mirror)

        output, err = p1.communicate()

        return output, p1.wait()

    def initialize_segments(self):
        input_files = sorted(glob.glob('%s/gpexpand_inputfile*' % self.working_directory))
        run_gpcommand(self.context, "gpexpand -D %s -i %s" % (self.database, input_files[-1]))

    def redistribute(self, duration=False, endtime=False):
        # Can flake with "[ERROR]:-End time occurs in the past"
        # if duration is set too low.
        if duration:
            flags = " --duration 00:00:02"
        elif endtime:
            newtime = datetime.now() + timedelta(seconds=2)
            flags = " --end \"%s\"" % newtime.strftime("%Y-%m-%d %H:%M:%S")
        else:
            flags = ""

        run_gpcommand(self.context, "gpexpand %s" % flags)


if __name__ == '__main__':
    gpexpand = Gpexpand()
    gpexpand.do_interview(num_of_segments=2, directory_pairs=[('/tmp/foo',''),
                                                              ('/tmp/bar','')])
