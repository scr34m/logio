#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <time.h>
#include <ctype.h>
#include <memcache.h>

struct memcache *mc;
char *mc_instance = "127.0.0.1:11211";
char prefix[] = "logio_";
char date[11] = "";

#define EXPIRE 86400

typedef struct s_vhost
{
	char *name;
	unsigned long long in;
	unsigned long long out;
	struct s_vhost *next;
} vhost;
vhost *vhost_first = NULL;
vhost *vhost_last = NULL;

vhost *create_vhost()
{
	vhost *new_vhost = malloc(sizeof(vhost));
	if (new_vhost != NULL)
	{
		new_vhost->in = 0;
		new_vhost->out = 0;
		new_vhost->next = NULL;
	}
	return new_vhost;
}

void setupmemcache()
{
	mc = mc_new();
	mc_server_add4(mc, mc_instance);
}

vhost *vhost_exist(char *name)
{
	vhost *iter;
	for (iter = vhost_first; NULL != iter; iter = iter->next)
	{
		if (strcmp(iter->name, name) == 0)
		{
			return iter;
		}
	}
	return NULL;
}

void vhost_destroy()
{
	vhost *iter = vhost_first;
	vhost *tmp;
	while (iter != NULL)
	{
		fprintf(stderr, "%p %p\n", iter, iter->next);
		tmp = iter->next;
		if (iter->name != NULL)
		{
			free(iter->name);
		}
		free(iter);
		iter = tmp;
	}

	vhost_first = NULL;
	vhost_last = NULL;
}

int update_daily_vhost_list(char *name)
{
	char *key;
	if (asprintf(&key, "%s%s", prefix, date) == -1)
	{
		return 1;
	}

	char *buf;
	char *ret = mc_aget(mc, key, strlen(key));
	if (ret != NULL)
	{
		asprintf(&buf, "%s:%s", ret, name);
	} else {
		asprintf(&buf, "%s", name);
	}

	if (ret != NULL)
	{
		free(ret);
	}

	if (buf == NULL)
	{
		free(key);
		return 1;
	}

	mc_set(mc, key, strlen(key), buf, strlen(buf), EXPIRE, 0); // XXX: expire 1 day

	free(buf);
	free(key);

	return 0;
}

int purge_daily_vhost_list()
{
	char *key;
	if (asprintf(&key, "%s%s", prefix, date) == -1)
	{
		return 1;
	}

	mc_delete(mc, key, strlen(key), 0);

	free(key);

	return 0;
}

int update_daily_value(char *name, unsigned long long in, unsigned long long out)
{
	char *key;
	if (asprintf(&key, "%s%s_%s", prefix, date, name) == -1)
	{
		return 1;
	}

	char *buf;
	if (asprintf(&buf, "%lld:%lld", in, out) == -1)
	{
		free(key);
		return 1;
	}
	
	mc_set(mc, key, strlen(key), buf, strlen(buf), EXPIRE, 0); // XXX: expire 1 day

	free(key);
	free(buf);

	return 0;
}

void line_parse(char *line)
{
	char *name;
	char *in;
	char *out;

	time_t rawtime;
	struct tm *timeinfo;
	char time_buff[11];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(time_buff, 11, "%Y-%m-%d", timeinfo);
	if (strcmp(&date, &time_buff) != 0)
	{
		vhost_destroy();
		strncpy(&date, &time_buff, sizeof(time_buff));

		purge_daily_vhost_list();
	}

	char *p;
	int i = 0;
	p = strtok(line," ");
	while (p != NULL)
	{
		if (i == 0) name = p;
		if (i == 1) in = p;
		if (i == 2) out = p;

		p = strtok(NULL, " ");
		i++;
	}

	vhost *vhost_actual = vhost_exist(name);
	if (vhost_actual == NULL)
	{
		vhost_actual = create_vhost();

		if (vhost_first == NULL)
		{
			vhost_first = vhost_last = vhost_actual;
		} else {
			vhost_last->next = vhost_actual;
			vhost_last = vhost_actual;
		}

		asprintf(&vhost_actual->name, "%s", line); // XXX: really allocated?

		update_daily_vhost_list(vhost_actual->name);
	}

	vhost_actual->in += atoll(in);
	vhost_actual->out += atoll(out);
	printf("%lld\n", vhost_actual->out);
	update_daily_value(vhost_actual->name, vhost_actual->in, vhost_actual->out);
}

int main(int argc, char *argv[])
{
	int read_buf_size = 100;
	char read_buf[read_buf_size];

	int line_size_def = 100;
	int line_size = line_size_def;
	char *line = malloc(line_size);
	*line = '\0';

	char c;
	while ((c = getopt (argc, argv, "m:")) != -1)
	{
		switch (c)
		{
			case 'm':
				mc_instance = optarg;
				break;
			case '?':
				if (optopt == 'c')
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				abort();
		}
	}

	FILE *is = fopen("/dev/stdin", "r");
	if (is == NULL)
	{
		fprintf(stderr, "Unable to STDIN\n");
		return 0;
	}

	// XXX: setup from memcache to local memory db?
	setupmemcache();

	while (1)
	{
		if (line == NULL)
		{
			fprintf(stderr, "Memory allocation error!");
			return 1;
		}

		if (fgets(read_buf, read_buf_size, is) == NULL)
		{
			return 0;
		}

		strcat(line, read_buf);

		if (strlen(read_buf) > 0 && read_buf[strlen(read_buf) - 1] != '\n')
		{
			line_size += line_size_def;
			line = realloc(line, line_size);
		} else {
			// XXX: replace \n to \0
			line_parse(line);

			free(line);
			line_size = line_size_def;
			line = malloc(line_size);
			memset(line, 0, line_size); // XXX: check line is not null
		}
	}

	return 0;
}
