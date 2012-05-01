logio
=====

Small Apache mod_logio parser for store realtime values in Memcache for logging. Another process need to read these informations, clear based on TTL (1 day).

Apache combined log format is "%v %I %O" which means virtualhost name, incoming and outgoing communication size in bytes.

Memcache structures are:

logio_<date>
points to a daily record with a list of logged virtual hosts name list

logio_<data>_<vhost>
daily usage by a virtual host format: in:out

memcachelib used for communication see http://people.freebsd.org/~seanc/libmemcache/README