#include "utils.h"
#include "vbfs-fuse.h"
#include "log.h"

char *pathname_str_sep(char **pathname, const char delim)
{
	char *sbegin = *pathname;
	char *end;
	char *sc;
	int found = 0;

	if (sbegin == NULL)
		return NULL;

	for (sc = sbegin; *sc != '\0'; ++sc) {
		if (*sc == delim) {
			found = 1;
			end = sc;
			break;
		}
	}

	if (! found)
		end = NULL;

	if (end)
		*end++ = '\0';

	*pathname = end;

	return sbegin;
}

int get_lastname(char *pathname, char *last_name, const char delim)
{
	int len = 0;
	char *pos = NULL;

	if (pathname == NULL) {
		return -1;
	}

	len = strlen(pathname);

	while (pathname[--len] == delim) {
		if (len <= 1)
			return 0;

		pathname[len] = '\0';
	}

	for (; len >= 0; len --) {
		if (pathname[len] == delim) {
			pos = &pathname[len + 1];
			strncpy(last_name, pos, NAME_LEN);

			pathname[len + 1] = '\0';
			break;
		}
	}


	return 0;
}
