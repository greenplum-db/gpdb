#include "gpfdist_helper.h"

bool is_valid_timeout(int timeout_val)
{
	if (timeout_val < 0)
		return false;
	else if (timeout_val == 1)
		return false;
	else if (timeout_val > 7200)
		return false;
	else
		return true;
	
}

bool is_valid_session_timeout(int timeout_val)
{
	if (timeout_val < 0)
		return false;
	else if (timeout_val > 7200)
		return false;
	else
		return true;
	
}

bool is_valid_listen_queue_size(int listen_queue_size)
{
	if (listen_queue_size < 16)
		return false;
	else if (listen_queue_size > 512)
		return false;
	else
		return true;
}
