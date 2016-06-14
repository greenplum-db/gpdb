#ifndef GPFDIST_HELPER_H
#define GPFDIST_HELPER_H
#include <stdbool.h>
bool is_valid_timeout(int timeout_val);
bool is_valid_session_timeout(int timeout_val);
bool is_valid_listen_queue_size(int listen_queue_size);
#endif
