\! gpsys1 &> /dev/null
\! gpsys1 -a &> /dev/null
\! gpsys1 -p &> /dev/null
\! gpsys1 -m &> /dev/null
\! gpsys1 -pm &> /dev/null
\! gpsys1 -am &> /dev/null
\! python mpp8987.py &> /dev/null
\! gpcheckperf -h localhost -d /tmp  -r d -S 10kb &> /dev/null
\! gpcheckperf -h localhost -d /tmp/  -r s  &>  /dev/null
\! gpstate -v &> /dev/null
\! gpstate -b &> /dev/null
\! gpstate -e &> /dev/null
