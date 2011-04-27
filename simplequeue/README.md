simplequeue
===========

Simplequeue is an in-memory queue that supports get/put operations on 
named queue endpoints. by default data is queued in the "default" namespace.

API Endpoints:

	/put?queue=named_queue&data=....
	/get?queue=named_queue
	/stats

Command Line Options

	--address=<str>        address to listen on
	                       default: 0.0.0.0
	--daemon               daemonize process
	--enable-logging       request logging
	--group=<str>          run as this group
	--help                 list usage
	--max-bytes=<int>      memory limit
	--max-depth=<int>      maximum items in queue
	--overflow-log=<str>   file to write data beyond --max-depth or --max-bytes
	--port=<int>           port to listen on
	                       default: 8080
	--root=<str>           chdir and run from this directory
	--user=<str>           run as this user
	--version
