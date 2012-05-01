logio
=====

Apache mod_logio parser for realtime values. Infomrations stored in Memcache for gathering and processing. So a background CRON process need to read these informations.

Memcache expire used to clear records older than 1 day. Each memcache record marked with the current date.

Apache combined log
-------------------

Example configuration for format is `LogFormat "%v %I %O" combined_io` variables are: virtualhost name, incoming and outgoing communication size in bytes.

Tell apache to pipe log directily to the application `CustomLog "| /path/to/logio" combined_io` only one instance will be running.

Memcache structures
-------------------

`logio_<date>`

A colon separated list of tracked virtualhosts reported by apache possible output is example.com:example.org:example.net

`logio_<data>_<vhost>`

Virtualhost bandwith usage with a colon separated string first parameter incoming traffic sendond one is the outgoing traffic.

Used library

memcachelib more info at http://people.freebsd.org/~seanc/libmemcache/README