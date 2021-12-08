#!/bin/bash

export GPFDIST_WATCHDOG_TIMER=5

NC_IPV6_PID=0
NC_IPV4_PID=0
GPFDIST_PID=0

function clear_subprocess()
{
	if [ $NC_IPV6_PID -ne 0 ]; then
		kill -9 $NC_IPV6_PID
		NC_IPV6_PID=0
	fi

	if [ $NC_IPV4_PID -ne 0 ]; then
		kill -9 $NC_IPV4_PID
		NC_IPV4_PID=0
	fi
	sleep 1
}

function start_nc_ipv4()
{
	nc -l 0.0.0.0 8080 &
	NC_IPV4_PID=$!
}

function start_nc_ipv6()
{
	nc -l :: 8080 &
	NC_IPV6_PID=$!
}

function start_gpfdist()
{
	gpfdist -v &
	GPFDIST_PID=$!
}

which nc
if [ $? != 0 ];then 
	echo -e "\033[32m nc command not supported, passed ipv4v6 test\033[0m"
	exit 0
fi

IPV4_ONLY=0
ip a | grep inet6
if [ $? == 0 ]; then
	IPV4_ONLY=0
else
	IPV4_ONLY=1
fi

# test start gpfdist with nc binded to ipv4 port
start_nc_ipv4
sleep 1
start_gpfdist
wait $GPFDIST_PID
if [ $? -eq 1 ]; then #gpfdist failed to start because of ipv4 port already in use
	echo -e "\033[32m test [gpfdist start with nc binded to ipv4 port] success \033[0m"
else
	echo -e "\033[31m test [gpfdist start with nc binded to ipv4 port] failed \033[0m"
	echo "gpfdist should not start successfully because port ipv4:8080 is used by nc"
	exit 1
fi
clear_subprocess

if [ $IPV4_ONLY == 0 ]; then
	# test start gpfdist with nc binded to ipv6 port
	start_nc_ipv6
	sleep 1
	start_gpfdist
	wait $GPFDIST_PID
	if [ $? -eq 1 ]; then #gpfdist failed to start because of ipv6 port already in use
		echo -e "\033[32m test [gpfdist start with nc binded to ipv6 port] success \033[0m"
	else
		echo -e "\033[31m test [gpfdist start with nc binded to ipv6 port] failed \033[0m"
		echo "gpfdist should not start successfully because port ipv6:8080 is used by nc"
		exit 1
	fi
	clear_subprocess

	# test start gpfdist with nc binded to ipv4 and ipv6 port
	start_nc_ipv4
	start_nc_ipv6
	sleep 1
	start_gpfdist
	wait $GPFDIST_PID
	if [ $? -eq 1 ]; then #gpfdist failed to start because of ipv4 and ipv6 port already in use
		echo -e "\033[32m test [gpfdist start with nc binded to ipv4 and ipv6 port] success \033[0m"
	else
		echo -e "\033[32m test [gpfdist start with nc binded to ipv4 and ipv6 port] failed \033[0m"
		echo "gpfdist should not start successfully because port ipv4::8080 and ipv6:8080 are used by nc"
		exit 1
	fi
	clear_subprocess

	# test start gpfdist alone without nc binded to ipv4 or ipv6 8080 port
	start_gpfdist
	sleep 1
	wait $GPFDIST_PID
	if [ $? -eq 134 ]; then #gpfdist stoped by watchdog
		echo -e "\033[32m test [gpfdist starts without ipv4 or ipv6 nc listening] success \033[0m"
	else
		echo -e "\033[31m test [gpfdist starts without ipv4 or ipv6 nc listening] failed \033[0m"
		echo "gpfdist should start successfully and stoped by watchdog"
		exit 1
	fi
	clear_subprocess
else
	# test start gpfdist alone without nc binded to ipv4 8080 port
	start_gpfdist
	sleep 1
	wait $GPFDIST_PID
	if [ $? -eq 134 ]; then #gpfdist stoped by watchdog
		echo -e "\033[32m test [gpfdist starts without ipv4 nc listening] success \033[0m"
	else
		echo -e "\033[31m test [gpfdist starts without ipv4 nc listening] failed \033[0m"
		echo "gpfdist should start successfully and stoped by watchdog"
		exit 1
	fi
	clear_subprocess

fi


echo -e "\n"
echo -e "\033[32m Test gpfdist binding ipv4/ipv6 ports success\033[0m"
exit 0
