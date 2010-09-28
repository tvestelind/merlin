#include "module.h"
#include "sha1.h"
#include <sys/mman.h>

/* does a deep free of a file_list struct */
void file_list_free(struct file_list *list)
{
	int i = 0;

	file_list *p;

	while ((p = list)) {
		i++;
		free(list->name);
		list = list->next;
		free(p);
	}
}


/*
 * Reads a file named *fname into a buffer and zero-separates its
 * lines into a pretty neat array
 */
static char *read_strip_split(char *fname, int *size)
{
	char *buf, *p;
	int fd, foo;
	int i;
	struct stat st;

	if (stat(fname, &st) < 0)
		return NULL;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		return NULL;

	if (!(buf = malloc(st.st_size))) {
		close(fd);
		return NULL;
	}
	*size = st.st_size;

	foo = read(fd, buf, st.st_size);
	close(fd);
	if (foo == -1) {
		free(buf);
		return NULL;
	}

	for (i = 0; i < st.st_size; i++) {
		if (buf[i] == '\n') {
			p = &buf[i];
			while (isspace(*p)) {
				*p = 0;
				p--;
			}
			buf[i] = '\0';
		}
	}

	return buf;
}


/*
 * recursive function to scan for .cfg files.
 * the static variable depth makes sure we don't loop for ever in
 * a nested symlink tree
 */
static struct file_list *recurse_cfg_dir(char *path, struct file_list *list,
					  int max_depth, int depth)
{
	DIR *dp;
	struct dirent *df;
	char *cwd, *wd;				/* current working directory */
	size_t wdl;					/* length of current working directory */
	struct stat st;

	cwd = getcwd(NULL, 0);

	dp = opendir(path);
	if (!dp || chdir(path) < 0) {
		chdir(cwd);
		return NULL;
	}

	depth++;

	wd = getcwd(NULL, 0);
	wdl = strlen(wd);

	while ((df = readdir(dp))) {
		unsigned len;

		if (!df->d_name)
			continue;
		if (!strcmp(df->d_name, ".") || !strcmp(df->d_name, ".."))
			continue;
		stat(df->d_name, &st);

		if (S_ISDIR(st.st_mode)) {
			/* don't recurse for ever */
			if (depth >= max_depth)
				continue;
			list = recurse_cfg_dir(df->d_name, list, max_depth, depth + 1);	/* parse recursively */
			continue;
		}

		/* we only want files and symlinks (stat() will follow symlinks) */
		if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode))
			continue;
		len = strlen(df->d_name);
		if (len < 4 || (strcmp(&df->d_name[len - 4], ".cfg")))
			continue;

		/* found a file matching "*.cfg" pattern */
		if (!list) {
			list = malloc(sizeof(struct file_list));
		}
		else {
			list->next = malloc(sizeof(struct file_list));
			list = list->next;
		}

		list->next = NULL;
		list->name = malloc(wdl + len + 2);
		memcpy(&list->st, &st, sizeof(struct stat));
		sprintf(list->name, "%s/%s", wd, df->d_name);
	}

	closedir(dp);
	chdir(cwd);

	return list;
}

/* fetches object configuration files in a nagios-compatible fashion
 * (reads cfg_file= variables and enters and recurses cfg_dir= statements,
 * through the use of the recursive function recurse_cfg_dir) */
static struct file_list *get_cfg_files(char *str, struct file_list *list)
{
	char *p;
	int size, i;
	struct file_list *base = NULL;

	p = read_strip_split(str, &size);
	if (!p || !size)
		return NULL;

	for (i = 0; i < size; i += strlen(&p[i]) + 1) {
		if (!prefixcmp(&p[i], "cfg_file=")) {
			i += 9;
			if (!list) {
				base = list = malloc(sizeof(struct file_list));
				if (!base)
					return NULL;
			}
			else {
				list->next = malloc(sizeof(struct file_list));
				if (!list->next)
					return base ? base : list;
				list = list->next;
			}
			list->next = NULL;
			list->name = strdup(&p[i]);
			if (!list->name)
				return base ? base : list;
			stat(list->name, &list->st);
		}
		else if (!prefixcmp(&p[i], "cfg_dir=")) {
			i += 8;
			list = recurse_cfg_dir(&p[i], list, 4, 0);
		}
	}

	return base ? base : list;
}

/* returns the last timestamp of a configuration change */
time_t get_last_cfg_change(void)
{
	time_t mt = 0;
	struct file_list *flist, *base;

	base = flist = get_cfg_files(config_file, NULL);

	for (; flist; flist = flist->next) {
		if (flist->st.st_mtime > mt)
			mt = flist->st.st_mtime;
	}

	if (base)
		file_list_free(base);

	/* 0 if we for some reason failed */
	return mt;
}

static int flist_cmp(const void *a_, const void *b_)
{
	const file_list *a = *(const file_list **)a_;
	const file_list *b = *(const file_list **)b_;

	return strcmp(a->name, b->name);
}

/* mmap() the file and hash its contents, adding it to ctx */
static int flist_hash_add(struct file_list *fl, blk_SHA_CTX *ctx)
{
	void *map;
	int fd;

	fd = open(fl->name, O_RDONLY);
	if (fd < 0)
		return -1;
	map = mmap(NULL, fl->st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!map)
		return -1;
	blk_SHA1_Update(ctx, map, fl->st.st_size);
	munmap(map, fl->st.st_size);
	return 0;
}

/*
 * calculate a sha1 hash of the contents of all config files
 * sorted by their full path.
 * *hash must hold at least 20 bytes
 */
int get_config_hash(unsigned char *hash)
{
	struct file_list *flist, *base;
	struct file_list **sorted_flist;
	int num_files = 0, i = 0;
	blk_SHA_CTX ctx;

	blk_SHA1_Init(&ctx);

	base = flist = get_cfg_files(config_file, NULL);

	/*
	 * this is horribly inefficient, but I don't really care.
	 * it works, and it was quick to write up. I'll improve on
	 * it later.
	 */
	for (flist = base; flist; flist = flist->next) {
		num_files++;
	}

	sorted_flist = calloc(num_files, sizeof(file_list *));
	for (flist = base; flist; flist = flist->next) {
		sorted_flist[i++] = flist;
	}
	qsort(sorted_flist, num_files, sizeof(file_list *), flist_cmp);
	for (i = 0; i < num_files; i++) {
		flist_hash_add(sorted_flist[i], &ctx);
	}
	blk_SHA1_Final(hash, &ctx);
	free(sorted_flist);

	if (base)
		file_list_free(base);

	return 0;
}
