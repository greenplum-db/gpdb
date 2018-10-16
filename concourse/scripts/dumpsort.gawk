#
# Sorts the table data in pg_dump output so we can correctly compare old/new
# dumps during upgrade tests.
#
# GNU awk is needed for the asort() function, sorry. You can always rewrite in
# Perl...
#

BEGIN {
    sorting = 0
}

# End of table data. Dump the lines we have and resume unsorted output.
/^\\\.$/ {
    if (sorting) {
        n = asort(lines)
        for (i = 1; i <= n; i++) {
            print lines[i]
        }

        delete lines
        sorting = 0
    }
}

# When sorting, don't print the lines we read; save them for later sorting at
# the end of the table.
sorting {
    lines[linenum++] = $0
}

# Unsorted lines. Just print them as-is.
!sorting {
    print $0
}

# Beginning of sorted table data. Start sorted mode.
/^COPY .* FROM stdin;$/ {
    if (!sorting) {
        sorting = 1
        linenum = 0
    }
}
