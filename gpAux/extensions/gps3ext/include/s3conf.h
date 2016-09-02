#include <string>
using std::string;


// segment id
extern int32_t s3ext_segid;

// total segment number
extern int32_t s3ext_segnum;

// UDP socket to send log
extern int32_t s3ext_logsock_udp;

// default log level
extern int32_t s3ext_loglevel;

// log type
extern int32_t s3ext_logtype;

// remote server port if use external log server
extern int32_t s3ext_logserverport;

// remote server address if use external log server
extern string s3ext_logserverhost;

// server address where log message is sent to
extern struct sockaddr_in s3ext_logserveraddr;

#endif
