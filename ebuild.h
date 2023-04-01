#ifndef EBUILD_H_
#define EBUILD_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include "windows.h"
#	include <process.h>

struct dirent {
	char d_name[MAX_PATH+1];
};

typedef struct DIR DIR;

DIR *opendir(const char *dirpath);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
#else
#	include <unistd.h>
#	include <sys/stat.h>
#	include <sys/wait.h>
#	include <dirent.h>
#endif // _WIN32

#ifdef _WIN32
#	define PATH_SEP "\\"
#else
#	define PATH_SEP "/"
#endif // _WIN32
#define PATH_SEP_LEN (sizeof(PATH_SEP) - 1)

#define FOREACH_VARGS(param, arg, args, body)				\
	do {								\
		va_start(args, param);					\
		for (const char *arg = va_arg(args, const char *);	\
				arg != NULL;				\
				arg = va_arg(args, const char *))	\
		{							\
			body;						\
		}							\
		va_end(args);						\
	} while(0)


#define FOREACH_ARRAY(type, item, items, body)				\
	do {								\
		for (size_t i = 0; 					\
				i < sizeof(items) / sizeof((items)[0]);	\
				++i) 					\
		{							\
			type item = items[i];				\
			body;						\
		}							\
	} while(0)

#define FOREACH_FILE_IN_DIR(file, dirpath, body)			\
	do {								\
		struct dirent *dp = NULL;				\
		DIR *dir = opendir(dirpath);				\
		while ((dp = readdir(dir))) {				\
			const char *file = dp->d_name;			\
			body;						\
		}							\
		closedir(dir);						\
	} while(0)

#define CMD(...) 							\
	do {								\
		INFO(JOIN(" ", __VA_ARGS__));				\
		cmd_impl(0, __VA_ARGS__, NULL);				\
	} while(0)

const char *concat_impl(int ignore, ...);
const char *concat_sep_impl(const char *sep, ...);
const char *build__join(const char *sep, ...);
int ebuild__ends_with(const char *str, const char *postfix);
int ebuild__is_dir(const char *path);
void mkdirs_impl(int ignore, ...);
void cmd_impl(int ignore, ...);
void ebuild_exec(const char **argv);
const char *ebuild__remove_ext(const char *path);
char *shift(int *argc, char ***argv);
void ebuild__rm(const char *path);

#define CONCAT(...) concat_impl(0, __VA_ARGS__, NULL)
#define CONCAT_SEP(sep, ...) build__deprecated_concat_sep(sep, __VA_ARGS__, NULL)
#define JOIN(sep, ...) build__join(sep, __VA_ARGS__, NULL)
#define PATH(...) JOIN(PATH_SEP, __VA_ARGS__)
#define MKDIRS(...) mkdirs_impl(0, __VA_ARGS__, NULL)
#define NOEXT(path) ebuild__remove_ext(path)
#define ENDS_WITH(str, postfix) ebuild__ends_with(str, postfix)
#define IS_DIR(path) ebuild__is_dir(path)
#define RM(path)                                \
    do {                                        \
        INFO("rm %s", path);                    \
        ebuild__rm(path);                      \
    } while(0)

void ebuild_log(FILE *stream, const char *tag, const char *fmt, ...);
void ebuild_vlog(FILE *stream, const char *tag, const char *fmt, va_list args);

void INFO(const char *fmt, ...);
void WARN(const char *fmt, ...);
void ERRO(const char *fmt, ...);

#endif // EBUILD_H_

#ifdef EBUILD_IMPLEMENTAION

#ifdef _WIN32
struct DIR{
	HANDLE hFind;
    	WIN32_FIND_DATA data;
    	struct dirent *dirent;
};

DIR *opendir(const char *dirpath) {
	assert(dirpath);

	char buffer[MAX_PATH];
	snprintf(buffer, MAX_PATH, "%s\\*", dirpath);

	DIR *dir = (DIR*)calloc(1, sizeof(DIR));

	dir->hFind = FindFirstFile(buffer, &dir->data);
	if (dir->hFind == INVALID_HANDLE_VALUE) goto fail;

	return dir;

fail:
	if (dir) free(dir);

	return NULL;
}

struct dirent *readdir(DIR *dirp) {
    	assert(dirp);

    	if (dirp->dirent == NULL) {
        	dirp->dirent = (struct dirent*)calloc(1, sizeof(struct dirent));
    	} else {
        	if(!FindNextFile(dirp->hFind, &dirp->data)) return NULL;
    	}

    	memset(dirp->dirent->d_name, 0, sizeof(dirp->dirent->d_name));

    	strncpy(
        	dirp->dirent->d_name,
        	dirp->data.cFileName,
        	sizeof(dirp->dirent->d_name) - 1);

	return dirp->dirent;
}

void closedir(DIR *dirp) {
	assert(dirp);

	FindClose(dirp->hFind);
	if (dirp->dirent) free(dirp->dirent);
    	free(dirp);
}
#endif // _WIN32

const char *build__join(const char *sep, ...) {
	const size_t sep_len = strlen(sep);
	size_t length = 0;
    	size_t seps_count = 0;

    	va_list args;

    	FOREACH_VARGS(sep, arg, args, {
        	length += strlen(arg);
        	seps_count += 1;
    	});
    	assert(length > 0);

    	seps_count -= 1;

    	char *result = malloc(length + seps_count * sep_len + 1);

    	length = 0;
    	FOREACH_VARGS(sep, arg, args, {
        	size_t n = strlen(arg);
        	memcpy(result + length, arg, n);
        	length += n;

        	if (seps_count > 0) {
            		memcpy(result + length, sep, sep_len);
            		length += sep_len;
            		seps_count -= 1;
        	}
    	});

    	result[length] = '\0';

    	return result;
}

void mkdirs_impl(int ignore, ...) {
	size_t length = 0;
	size_t seps_count = 0;

	va_list args;
	FOREACH_VARGS(ignore, arg, args, {
		length += strlen(arg);
		seps_count += 1;
	});

	assert(length > 0);
	seps_count -= 1;
	char *result = malloc(length + seps_count * PATH_SEP_LEN + 1);

	length = 0;
	FOREACH_VARGS(ignore, arg, args, {
		size_t n = strlen(arg);
		memcpy(result + length, arg, n);
		length += n;

		if (seps_count > 0) {
			memcpy(result + length, PATH_SEP, PATH_SEP_LEN);
			length += PATH_SEP_LEN;
			seps_count -= 1;
		}

		result[length] = '\0';

		INFO("mkdirs %s", result);
		if (mkdir(result, 0755) < 0) {
			if (errno == EEXIST) {
				WARN("directory %s alredy exist", result);
			} else {
				ERRO("could not create directory %s: %s",
						result, strerror(errno));
				exit(1);
			}
		}
	});
}

const char *concat_impl(int ignore, ...) {
	size_t length = 0;
	va_list args;
	FOREACH_VARGS(ignore, arg, args, {
		length += strlen(arg);
	});

	char *result = malloc(length + 1);
	length = 0;
	FOREACH_VARGS(ignore, arg, args, {
		size_t n = strlen(arg);
		memcpy(result + length, arg, n);
		length += n;
	});

	result[length] = '\0';

	return result;
}

void ebuild_exec(const char **argv) {
#ifdef _WIN32
	intptr_t status = _spawnvp(_P_WAIT, argv[0], (char * const*) argv);
    	if (status < 0) {
        	ERRO("could not start child process: %s", strerror(errno));
        	exit(1);
    	}

    	if (status > 0) {
        	ERRO("command exited with exit code %d", status);
        	exit(1);
    	}
#else
	pid_t cpid = fork();
	if (cpid == -1) {
		ERRO("could not fork a child process: %s", strerror(errno));
		exit(1);
	}

	if (cpid == 0) {
		if (execvp(argv[0], (char * const*)argv) < 0) {
			ERRO("could not execute child process: %s", strerror(errno));
			exit(1);
		}
	} else {
		for (;;) {
            		int wstatus = 0;
            		wait(&wstatus);

            		if (WIFEXITED(wstatus)) {
                		int exit_status = WEXITSTATUS(wstatus);
                		if (exit_status != 0) {
                    			ERRO("command exited with exit code %d", exit_status);
                    			exit(-1);
                		}

                		break;
            		}

            		if (WIFSIGNALED(wstatus)) {
                		ERRO("command process was terminated by signal %d", WTERMSIG(wstatus));
                		exit(-1);
            		}
        	}
	}
#endif // _WIN32
}

void cmd_impl(int ignore, ...) {
	size_t argc = 0;
	va_list args;
	FOREACH_VARGS(ignore, arg, args, {
		argc += 1;
	});

	const char **argv = malloc(sizeof(const char *) * (argc + 1));
	argc = 0;
	FOREACH_VARGS(ignore, arg, args, {
		argv[argc++] = arg;
	});
	argv[argc] = NULL;

	assert(argc >= 1);

	ebuild_exec(argv);
}

const char *ebuild__remove_ext(const char *path) {
	size_t n = strlen(path);
    	while (n > 0 && path[n - 1] != '.') {
        	n -= 1;
    	}

    	if (n > 0) {
        	char *result = malloc(n);
        	memcpy(result, path, n);
        	result[n - 1] = '\0';

        	return result;
    	} else {
        	return path;
    	}
}

char *shift(int *argc, char ***argv) {
	assert(*argc > 0);
	char *result = **argv;
	*argv += 1;
	*argc -= 1;
	return result;
}

void ebuild_log(FILE *stream, const char *tag, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ebuild_vlog(stream, tag, fmt, args);
	va_end(args);
}

void ebuild_vlog(FILE *stream, const char *tag, const char *fmt, va_list args) {
	fprintf(stream, "[%s] ", tag);
	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");
}

void INFO(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ebuild_vlog(stdout, "INFO", fmt, args);
	va_end(args);
}

void WARN(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ebuild_vlog(stderr, "WARN", fmt, args);
	va_end(args);
}

void ERRO(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ebuild_vlog(stderr, "ERRO", fmt, args);
	va_end(args);
}

int ebuild__ends_with(const char *str, const char *postfix) {
    const size_t str_n = strlen(str);
    const size_t postfix_n = strlen(postfix);
    return postfix_n <= str_n && strcmp(str + str_n - postfix_n, postfix) == 0;
}

int ebuild__is_dir(const char *path) {
#ifdef _WIN32
    	DWORD dwAttrib = GetFileAttributes(path);

    	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    	struct stat statbuf = {0};
    	if (stat(path, &statbuf) < 0) {
		if (errno == ENOENT) return 0;

		ERRO("could not retrieve information about file %s: %s",
	     		path, strerror(errno));
		exit(1);
    	}

    	return S_ISDIR(statbuf.st_mode);
#endif // _WIN32
}

void ebuild__rm(const char *path) {
    	if (IS_DIR(path)) {
		FOREACH_FILE_IN_DIR(file, path, {
	    		if (strcmp(file, ".") != 0 && strcmp(file, "..") != 0) {
				ebuild__rm(PATH(path, file));
	   	 	}
		});

		if (rmdir(path) < 0) {
	    		if (errno == ENOENT) {
				WARN("directory %s does not exist");
	    		} else {
				ERRO("could not remove directory %s: %s", path, strerror(errno));
				exit(1);
	    		}
		}
    	} else {
		if (unlink(path) < 0) {
	    		if (errno == ENOENT) {
				WARN("file %s does not exist");
	    		} else {
				ERRO("could not remove file %s: %s", path, strerror(errno));
				exit(1);
	    		}
		}
    	}
}
#endif // EBUILD_IMPLEMENTAION
