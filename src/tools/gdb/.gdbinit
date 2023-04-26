# USAGE:
# 1. Save this script as .gdbinit in your home directory or GPDB project directory.
# 2. Launch GDB with the appropriate binary.
# 3. GDB will automatically load this script during startup.
# 4. Use the custom GDB commands to examine the relevant data structures during debugging.
#
# NOTE: Modify this script according to your specific debugging needs.
# This is just a starting point and can be customized to better suit your workflow.

define dump_procs
    set $i=0
    print procArray->numProcs
    while ($i < procArray->numProcs)
        set $pgprocno = procArray->pgprocnos[$i]
        set $proc = &allProcs[$pgprocno]
        set $pgxact = &allPgXact[$pgprocno]
        printf "pgprocnos:%d\tprocArray index:%d\t", $pgprocno, $i
        printf "pid:%d\tbackendID:%d\tlxid:%d\tdelayChkpt:%d", $proc->pid, $proc->backendId, $proc->lxid, $pgxact->delayChkpt
        printf "\n"
        set $i = $i + 1
    end
end

define dump_delayChkpt_procs
    set $i=0
    print procArray->numProcs
    while ($i < procArray->numProcs)
        set $pgprocno = procArray->pgprocnos[$i]
        set $proc = &allProcs[$pgprocno]
        set $pgxact = &allPgXact[$pgprocno]
        if ($pgxact->delayChkpt)
           printf "pgprocnos:%d\tprocArray index:%d\t", $pgprocno, $i
           printf "pid:%d\tbackendID:%d\tlxid:%d", $proc->pid, $proc->backendId, $proc->lxid
           printf "\n"
        end
        set $i = $i + 1
    end
end
