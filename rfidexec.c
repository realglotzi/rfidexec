/*
    rfidexec -- client that reads rfid's from 125Khz RFID EM410X USB Reader
                and executes a command

	Copyright (C) 2015 Dirk E. Wagner

    This program is free software; you can redistribute it and/or modify it
    under the terms of version 2 of the GNU General Public License as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/


 /* Standard headers */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>
#include <ctype.h>

/* Input subsystem interface */
#include <linux/input.h>

#include <sys/types.h>
#include <sys/un.h>

#include <sys/file.h>

#include "debug.h"
#include "hashmap.h"
#include "mapping.h"

static int rfid_fd = -1;
static map_t mymap;
static char devinput[BUFSIZ] = "/dev/rfid";
static char last_event[KEY_MAX_LENGTH] = "";

static size_t read_rfid(char *const buffer, const size_t length) {
    size_t len = 0;
    struct input_event ev;
    ssize_t n;
    int digit;

	memset(buffer, 0, length);

    while (1) {

        n = read(rfid_fd, &ev, sizeof ev);

//		DBG("rfidexec: time %ld.%06ld  type %d  code %d  value %d\n",
//			ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value);
		
        if (n == (ssize_t)-1) {
            if (errno == EINTR)
                continue;
            break;

        } else if (n == sizeof ev) {

            if (ev.type != EV_KEY || (ev.value != 1 && ev.value != 2))
                continue;

            switch (ev.code) {
            case KEY_0: digit = '0'; break;
            case KEY_1: digit = '1'; break;
            case KEY_2: digit = '2'; break;
            case KEY_3: digit = '3'; break;
            case KEY_4: digit = '4'; break;
            case KEY_5: digit = '5'; break;
            case KEY_6: digit = '6'; break;
            case KEY_7: digit = '7'; break;
            case KEY_8: digit = '8'; break;
            case KEY_9: digit = '9'; break;
            default:    digit = '\0';
            }

			// finish endless loop
            if (digit == '\0') {
                break;
            }

			/* check buffer overflow */
            if (len < length)
                buffer[len] = digit;
            len++;

            continue;

        } else {
            break;
        }
		
    }

    return len;
}

static void print_help() {

	printf ("rfidexec [-w] [-d path] [-f] [-u username] [-t path]\n\n");
	printf ("Options: \n");
	printf ("\t-d <path> to rfid reader. The default is /dev/rfid.\n");
	printf ("\t-f Run in the foreground.\n");
	printf ("\t-u <user> User name.\n");
	printf ("\t-t <path> Path to translation table.\n");
	
}

static void main_loop() {

	char eventbuf[BUFSIZ] = "";
	int bufSize = 0;
	
	do {
		if ((bufSize = read_rfid(eventbuf, BUFSIZ)) > 0) {

			if (strncmp(eventbuf, last_event, KEY_MAX_LENGTH) != 0) {
				strncpy(last_event, eventbuf, KEY_MAX_LENGTH);
				map_entry_t *map_entry;
				if (hashmap_get(mymap, eventbuf, (void**)(&map_entry)) == MAP_OK) {
					DBG ("MAP_OK map_entry->rfid_code=%s map_entry->value=%s\n", map_entry->key, map_entry->value);	
					syslog(LOG_INFO, "executing by rfidexec (%s)", map_entry->value);
					if (system (map_entry->value)  < 0) {
					  syslog(LOG_INFO, "rfidexec: can not execute %s", map_entry->value);
					}
				} else {
					DBG ("MAP_ERROR rfid_code=%s|\n", eventbuf);	
				}
			} else {
				DBG ("Duplicate event - ignored %s\n", eventbuf);
			}
		}
	} while (true);

}

int main(int argc, char *argv[]) {
	
	char *user = "nobody";
	char *translation_path = "/etc/rfidexec.map";
	int opt;
	bool foreground = false;

	while ((opt = getopt(argc, argv, "hd:fu:t:")) != -1) {
        	switch (opt) {
			case 'd':
				strncpy (devinput, optarg, sizeof devinput - 1); 
				break;
			case 'u':
				user = strdup (optarg);
				break;
			case 'f':
				foreground = true;
				break;
			case 't':
				translation_path = strdup (optarg);
				break;
			case 'h':
				print_help ();
				return 0;
            		default:
				print_help ();
                		return EX_USAGE;
        	}
    	}

	mymap = hashmap_new();
	
	if (!parse_translation_table(translation_path, mymap)) {
		hashmap_free(mymap);
		return EX_OSERR;
	}

	if ((rfid_fd = open(devinput, O_RDONLY)) < 0) {
		hashmap_free(mymap);
		if (rfid_fd >= 0) close(rfid_fd);
		return EX_OSERR;
	}

	struct passwd *pwd = getpwnam(user);
	if (!pwd) {
		hashmap_free(mymap);
		if (rfid_fd >= 0) close(rfid_fd);
		fprintf(stderr, "Unable to resolve user %s!\n", user);
		return EX_OSERR;
	}

	if (setgid(pwd->pw_gid) || setuid(pwd->pw_uid)) {
		hashmap_free(mymap);
		if (rfid_fd >= 0) close(rfid_fd);
		fprintf(stderr, "Unable to setuid/setguid to %s!\n", user);
		return EX_OSERR;
	}

	if (!foreground) {
		int rc = daemon(0, 0);
		if (rc < 0) {
			hashmap_free(mymap);
			if (rfid_fd >= 0) close(rfid_fd);
			fprintf(stderr, "Unable to daemonize!\n");
			return EX_OSERR;
		}		
	}

	syslog(LOG_INFO, "Started");

	signal(SIGPIPE, SIG_IGN);

	main_loop();
	
	/* Now, destroy the map */
	hashmap_free(mymap);
	if (rfid_fd >= 0) close(rfid_fd);
	
	return 0;
}
