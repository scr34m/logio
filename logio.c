#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <memcache.h>
#include <time.h>

struct memcache *mc;
char *mc_instance = "127.0.0.1:11211";
char prefix[] = "logio_";
char date[11] = "";

#define EXPIRE 86400

typedef struct s_vhost
{
	char *name;
	long int in;
	long int out;
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
		printf("%p %p\n", iter, iter->next);
		tmp = iter->next;
		free(iter);
		iter = tmp;
	}
}

void update_daily_vhost_list(char *name)
{
	int key_size = strlen(prefix) + strlen(date) + 1;
	char *key = malloc(key_size);
	snprintf(key, key_size, "%s%s", prefix, date);

	char *buf;
	int buf_size = strlen(name) + 1 + 1;

	char *ret = mc_aget(mc, key, strlen(key));
	if (ret != NULL)
	{
		buf_size = buf_size + strlen(ret) + 1;
		buf = malloc(buf_size);
		snprintf(buf, buf_size, "%s:%s", ret, name);
		free(ret);
	} else {
		buf = malloc(buf_size);
		snprintf(buf, buf_size, "%s", name);
	}

	// TODO: check success
	mc_set(mc, key, strlen(key), buf, strlen(buf), EXPIRE, 0); // XXX: expire 1 day

	free(buf);
	free(key);
}

void purge_daily_vhost_list()
{
	int key_size = strlen(prefix) + strlen(date) + 1;
	char *key = malloc(key_size);
	snprintf(key, key_size, "%s%s", prefix, date);

	mc_delete(mc, key, strlen(key), 0);
}

void update_daily_value(char *name, long int in, long int out)
{
	int i = strlen(prefix) + strlen(date) + 1 + strlen(name) + 1;
	char *key = malloc(i);
	snprintf(key, i, "%s%s_%s", prefix, date, name);

	char buf[256];
	sprintf(buf, "%d:%d", in, out);

	mc_set(mc, key, strlen(key), buf, strlen(buf), EXPIRE, 0); // XXX: expire 1 day

	free(key);
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

		vhost_actual->name = malloc(strlen(line) + 1);
		snprintf(vhost_actual->name, strlen(line) + 1, "%s", line);

		update_daily_vhost_list(vhost_actual->name);
	}

	vhost_actual->in += atoi(in);
	vhost_actual->out += atoi(out);
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
		printf("Unable to STDIN\n");
		return 0;
	}

	// XXX: setup from memcache to local memory db?
	setupmemcache();

	while (1)
	{
		if (line == NULL)
		{
			printf("Memory allocation error!");
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
