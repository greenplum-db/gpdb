-- test invalid connection request (random bytes) to ic proxy:
-- it should be dropped due to mismatch magic number

CREATE OR REPLACE LANGUAGE plpythonu;

CREATE or REPLACE FUNCTION send_bytes_to_icproxy()
    RETURNS VOID as $$
import socket, struct

# parse host and port from gp_interconnect_proxy_addresses
icproxy_host = ""
icproxy_port = -1
try:
    res = plpy.execute("show gp_interconnect_type;", 1)
    if res[0]["gp_interconnect_type"] == "proxy":
        res = plpy.execute("show gp_interconnect_proxy_addresses;", 1)
        addrs = res[0]["gp_interconnect_proxy_addresses"]
        addr = addrs.split(",")[1]
        icproxy_host = addr.split(":")[2]
        icproxy_port = int(addr.split(":")[3])
except:
    # not working on icproxy mode, just return
    icproxy_host = ""

if icproxy_host == "":
    plpy.info("no need to send request")
    return

ranstr="fSSAtMbxAzOhkTcpPwvXQq9Y45vfnTiOdo3Mk9V8QKLKWyd8SHBI59FsDRLWFqMZkFpwosqZEIkua3YfR4C8GZ6n6J4SEYCJcdamjhg2e9e1Ns7kNnnUa7OeRyKSEhIANBfau3tsT1SIR4LnXXOAxMBWJkBLtMDKHQyljfa7iQKa1H0bLBP3gOqX8jshoLE7lksNsvmngfpsGtBmKswFNwdLxR1SiMU7kDfU6gCtmLVbCScImf5AVLMosFExTmX3QdiExuuwST6mNZKS7ZdTzsaKbOSy00EWerm39vW8g305H1jrRNSNZvpRkkE2Hv3Abe9ilXZ5hNsWe0DN1i884iPDmxX0B8T8l3SL4P2NInMmcpuQCNVc4ivE7KgLkFafsKrs1igvyRjLraJcjgk88BOkMoVxSE8plK8JoQGDJylg80WFlPBfgm4vjQjRuh8E9MF0M2Ws5GKiDxQ8RcoEEvmHS6ef76kFjai0A10PRkrOPXjNaCJD7jdRPFjkbr3EyhMGNOKMJxXLaUtzuDpz0Ivc0bH9SXffughI61rzChnt71dS15rpTHm6RHodlkwUHfY0H21I3F2uj7qELJR1kb8A5rfYDf0yv8pyDpCeTxT9WjaDlP1DPT74kXZEuIcIfX2H7RvSR89n3W2DJbdgXD9WSz3DknotAzWdY1iNe7qP6hwGJLkxx0ICBD9JaanCwlL4whB1DmtfZqFARsatXPcgVQIYKquJepNoVdVTwSYTb6XBLEGFTn4S6lJXkpyOB9M6egiYIvMYt1HHgDANFmXLslpB3TIYgc3kgGZ2SKI3sjuYghv0TODOLzFZoH9OuLnDkQVXk4a17xpRgl8pal6geryP6sPGA8XHpi0a6gdnqmGiaTQY84EHeykTcgR6K37UVZm9u8Go1xyjqnHjUtcb0BVZg5uY7MUd6ciMAbvXKh837oNpsbkC27qfPawd3W9Bz0aUhxB9st3EksZSmv6b8sRTyEBOT5zwMP7A"

# send the random bytes to icproxy
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    s.connect((icproxy_host, icproxy_port))
    val = struct.pack('!i', 134591) # an invalid magic number
    s.send(val)
    message = ranstr.encode('utf-8')
    val = struct.pack('!i', len(message))
    s.send(val)
    s.sendall(message)
except:
    pass # error may happens in pipeline env, just skip it
finally:
    s.close()

plpy.info("sent request")
$$ LANGUAGE plpythonu EXECUTE ON MASTER;

1: create table PR_14998(i int);
1: SELECT send_bytes_to_icproxy();
-- the query hung here before the commit adding the magic number field in the ICProxyPkt
-- reason: random bytes cause icproxy OOM or hang forever
1: SET statement_timeout = 10000;
1: select count(*) from PR_14998;
1: RESET statement_timeout;
1: drop table PR_14998;
1q:
