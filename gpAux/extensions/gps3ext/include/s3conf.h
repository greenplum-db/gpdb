#include <string>
using std::string;

// udp socket to send log
extern int s3ext_logsock_udp;

// default log level
extern int s3ext_loglevel;

// thread number for downloading
extern int s3ext_threadnum;

// chunk size for each downloading
extern int s3ext_chunksize;

// segment id
extern int s3ext_segid;

// total segmeng number
extern int s3ext_segnum;

// log type
extern int s3ext_logtype;

// remote server port if use external log server
extern int s3ext_logserverport;

// remote server address if use external log server
extern string s3ext_logserverhost;

// s3 access id
extern string s3ext_accessid;

// s3 secret
extern string s3ext_secret;

// s3 token
extern string s3ext_token;

// http or https
extern bool s3ext_encryption;

// debug curl or not
extern bool s3ext_debug_curl;

// configuration file path
extern string s3ext_config_path;

// server address where log msg is sent to
extern struct sockaddr_in s3ext_logserveraddr;

// low speed timeout
extern int s3ext_low_speed_limit;
extern int s3ext_low_speed_time;

// not thread safe!! call it only once.
bool InitConfig(const string &path, const string section);
