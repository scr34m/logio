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

char time_buff[11] = "";
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

void update_date_buff()
{
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(time_buff, 11, "%Y-%m-%d", timeinfo);
}

void set_date()
{
	strncpy(&date, &time_buff, sizeof(time_buff));
}

int vhost_bootstrap_record(char* name)
{
	char *key;
	if (asprintf(&key, "%s%s_%s", prefix, date, name) == -1)
	{
		return 1;
	}

	char *ret = mc_aget(mc, key, strlen(key));
	if (ret == NULL)
	{
		free(key);
		return 0;
	}

	char *p;
	char *out = strstr(ret, ":");
	if (out == NULL)
	{
		free(key);
		return 0;
	}
	
	// replace colon to \0
	p = ret + (out - ret);
	*p = '\0';

	// skip colon
	out++;

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

		asprintf(&vhost_actual->name, "%s", name);
	}

	vhost_actual->in = atoll(ret);
	vhost_actual->out = atoll(out);

	free(ret);
	free(key);
	return 0;
}

int vhost_bootstrap()
{
	update_date_buff();
	set_date();

	char *key;
	if (asprintf(&key, "%s%s", prefix, date) == -1)
	{
		return 1;
	}

	char *ret = mc_aget(mc, key, strlen(key));
	if (ret == NULL)
	{
		free(key);
		return 0;
	}

	char *p;
	p = strtok(ret,":");
	while (p != NULL)
	{
		vhost_bootstrap_record(p);
		p = strtok(NULL, ":");
	}

	free(ret);
	free(key);
	return 0;
}

void line_parse(char *line)
{
	char *name;
	char *in;
	char *out;

	update_date_buff();
	if (strcmp(&date, &time_buff) != 0)
	{
		vhost_destroy();
		set_date();
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

	update_daily_value(vhost_actual->name, vhost_actual->in, vhost_actual->out);
}

int main(int argc, char *argv[])
{
	int read_buf_size = 100;
	char read_buf[read_buf_size];
	FILE *is;
	int line_size_def = 100;
	int line_size = line_size_def;
	char *line = malloc(line_size);
	*line = '\0';

	int clear = 0;
	char c;
	while ((c = getopt(argc, argv, "cm:")) != -1)
	{
		switch (c)
		{
			case 'c':
				clear = 1;
				break;
			case 'm':
				mc_instance = optarg;
				break;
			default:
				abort();
				break;
		}
	}

	if (clear == 0)
	{
		is = fopen("/dev/stdin", "r");
		if (is == NULL)
		{
			fprintf(stderr, "Unable to open STDIN\n");
			return 0;
		}
	}

	setupmemcache();

	if (clear == 1)
	{
		update_date_buff();
		set_date();
		purge_daily_vhost_list();
		exit(0);
	}

	vhost_bootstrap();

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
