#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <xosd.h>

enum {
	OSD_DELAY = 25,
	OSD_SLEEP = 40000,
	TEXT_SIZE = 128
};

struct osd_data {
	int  percent;
	char text[TEXT_SIZE];
};

struct osd_msg {
	long            mtype;
	struct osd_data data;
};

void remove_queue(int foo, void* p) {
	int queue = *((int*)p);
	msgctl(queue, IPC_RMID, 0);
}

void usage(FILE* out, const char* name) {
	fprintf(out, "Usage: %s -p percent [text...]\n", name);
}

int worker(key_t key, int percent, char* text) {
	daemon(1, 1);

	int queue;
	if ((queue = msgget(key, IPC_CREAT | 0644)) < 0) {
		perror("Could not create message queue");
		return 1;
	}

	on_exit(remove_queue, &queue);

	xosd* osd = xosd_create(2);
	if (!osd) {
		if (!getenv("DISPLAY"))
			fputs("Could not create OSD: DISPLAY is not set\n", stderr);
		else
			perror("Could not create OSD");
		return 1;
	}

	xosd_set_colour(osd, "#0000FF");
	xosd_set_font(osd, "-*-lucidatypewriter-*-*-*-*-*-240-*-*-*-*-*-*");
	xosd_set_shadow_offset(osd, 1);
	xosd_set_pos(osd, XOSD_middle);
	xosd_set_align(osd, XOSD_center);
	xosd_display(osd, 0, XOSD_string, text);
	if (percent < 0)
		xosd_display(osd, 1, XOSD_string, "");
	else
		xosd_display(osd, 1, XOSD_percentage, percent);

	int i;
	for (i = 0; i < OSD_DELAY; ++i) {
		usleep(OSD_SLEEP);

		struct osd_msg msg;
		struct osd_data data;
		ssize_t size = -1;
		while ((size = msgrcv(queue, &msg, sizeof (data), 0, IPC_NOWAIT)) >= 0) {
			if (size == sizeof (data)) {
				memcpy(&data, &msg.data, sizeof (data));
				i = -1;
			}
		}

		if (i < 0) {
			if (memcmp(text, data.text, TEXT_SIZE - 1)) {
				memcpy(text, data.text, TEXT_SIZE - 1);
				text[TEXT_SIZE - 1] = 0;
				xosd_display(osd, 0, XOSD_string, text);
			}
			if (data.percent != percent) {
				percent = data.percent;
				if (percent < 0)
					xosd_display(osd, 1, XOSD_string, "");
				else
					xosd_display(osd, 1, XOSD_percentage, percent);
			}
		}
	}
	xosd_destroy(osd);
	return 0;
}

int main(int argc, char *argv[]) {
	char ch;
	int percent = -1;
	while ((ch = getopt(argc, argv, "p:h")) >= 0) {
		switch (ch) {
		case 'p':
			percent = atoi(optarg);
			break;
		case 'h':
			usage(stdout, argv[0]);
			return 0;
		case '?':
			if (optopt == 'c')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint (optopt))
				fprintf(stderr, "Unknown option '-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
			usage(stderr, argv[0]);
			return 1;
		}
	}

	if (optind == argc && percent < 0) {
		usage(stderr, argv[0]);
		return 1;
	}

	char text[TEXT_SIZE] = "";
	size_t size = 0;
	int i;
	for (i = optind; i < argc; ++i) {
		if (size < TEXT_SIZE - 1) {
			if (size > 0)
				text[size++] = ' ';
			strncpy(text + size, argv[i], TEXT_SIZE - size - 1);
			size += strlen(argv[i]);
		}
	}
	text[TEXT_SIZE - 1] = 0;

	key_t key;
	if ((key = ftok("osdbar.c", 'B')) < 0) {
		perror("Could not generate message queue key");
		return 1;
	}

	int queue;
	if ((queue = msgget(key, 0644)) < 0)
		return worker(key, percent, text);

	struct osd_msg msg = { 1, { percent } };
	memcpy(msg.data.text, text, TEXT_SIZE);
        if (msgsnd(queue, &msg, sizeof (msg.data), 0) < 0)
		perror("Could not send message");

	return 0;
}
