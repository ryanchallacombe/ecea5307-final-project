#!/bin/sh
##########################################################################
# Startup script aesd-client
#
# On stop it will send SIGTERM to gracefully exit the daemon and perform 
# associated cleanup steps.
##########################################################################

# TODO: Need to add IP addr to application startup calls


case $1 in 
	start)
		echo "Starting client application daemon"
		
		# native system binary location
		#start-stop-daemon --start --name aesd_client --startas /home/ryan/projects/ecea5307-final-project/client/aesd_client -- 192.168.86.30 -d

		# embedded system binay location
		start-stop-daemon --start --name aesd_client --startas /usr/bin/aesd_client -- 192.168.86.30 -d
		;;
	stop)
		echo "Stopping client application daemon"

		# SIGTERM is the dfault signal sent on stop
		start-stop-daemon --stop --name aesd_client		
		;;
	*)
		echo "Usage: $0 {start | stop}"
	exit 1
esac

exit 0