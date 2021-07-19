// Copyright(c) 2021, Silciom Denmark A/S
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// compile: sudo gcc -DSTATIC=static -D_GNU_SOURCE -I /usr/src/opae/argsfilter /usr/src/opae/argsfilter/argsfilter.c n5010-ctl.c -o n5010-ctl -l opae-c -l uuid

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include <opae/fpga.h>
#include <uuid/uuid.h>
#include <endian.h>
#include "argsfilter.h"

#define DFH_EOL(h) ((h >> 40) & 1)
#define DFH_NEXT(h) ((h >> 16) & 0xffffff)
#define DFH_TYPE(h) ((h >> 60) & 0xf)
#define DFH_TYPE_AFU 1

#define MAX_PORT 3
#define INVALID_PORT (MAX_PORT + 1)
#define CVL0_QSFP01_SWITCH 0x28
#define CVL1_QSFP23_SWITCH 0x30

char const *default_guid = "c48d4e2a-9121-497d-adc7-3640729ec6f7";

// Define the function to be called when ctrl-c (SIGINT) signal is sent to
// process
static volatile bool stop = false;
void signal_callback_handler(int signum)
{
	switch (signum) {
	case SIGINT:
		stop = true;
		break;
	default:
		break;
	}
}

struct n5010;

struct n5010_test {
	const char *name;
	fpga_result (*func)(struct n5010 *n5010);
};

struct n5010 {
	fpga_properties filter;
	fpga_token token;
	fpga_handle handle;
	fpga_guid guid;
	uint64_t base;
	const struct n5010_test *test;
	uint64_t port;
	bool debug;
};

static fpga_result fpga_setread_switch(struct n5010 *n5010);
static fpga_result fpga_read_switch(struct n5010 *n5010);
static fpga_result fpga_run(struct n5010 *n5010);

static const struct n5010_test n5010_test[] = {
	{
		.name = "setsw",
		.func = fpga_setread_switch,
	},
	{
		.name = "readsw",
		.func = fpga_read_switch,
	},
	{
		.name = "runsw",
		.func = fpga_run,
	},
};


static fpga_result fpga_open(struct n5010 *n5010)
{
	fpga_result res;
	uint32_t num = 0;
	res = fpgaPropertiesSetObjectType(n5010->filter, FPGA_ACCELERATOR);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to set object type: %s\n",
			fpgaErrStr(res));
		goto error;
	}
	res = fpgaPropertiesSetGUID(n5010->filter, n5010->guid);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to set guid: %s\n", fpgaErrStr(res));
		goto error;
	}

	res = fpgaEnumerate(&n5010->filter, 1, &n5010->token, 1, &num);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to enumerate fpga: %s\n",
			fpgaErrStr(res));
		goto error;
	}

	if (num == 0) {
		res = FPGA_NOT_FOUND;
		fprintf(stderr, "failed to find fpga with guid: %s\n",
			fpgaErrStr(res));
		goto error;
	}

	res = fpgaOpen(n5010->token, &n5010->handle, FPGA_OPEN_SHARED);

	if (res != FPGA_OK) {
		fprintf(stderr, "failed to open fpga: %s\n", fpgaErrStr(res));
		goto error;
	}

	res = fpgaMapMMIO(n5010->handle, 0, NULL);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to map io memory: %s\n",
			fpgaErrStr(res));
		fpgaClose(n5010->handle);
		goto error;
	}

error:
	return res;
}

static void fpga_close(struct n5010 *n5010)
{
	fpga_result res;

	if (n5010->handle != NULL) {
		res = fpgaUnmapMMIO(n5010->handle, 0);
		if (res != FPGA_OK)
			fprintf(stderr, "failed to unmap io memory: %s\n",
				fpgaErrStr(res));

		res = fpgaClose(n5010->handle);
		if (res != FPGA_OK)
			fprintf(stderr, "failed to close fpga: %s\n",
				fpgaErrStr(res));
	}

	if (n5010->token != NULL) {
		res = fpgaDestroyToken(&n5010->token);
		if (res != FPGA_OK)
			fprintf(stderr, "failed to destroy token: %s\n",
				fpgaErrStr(res));
	}
}

static fpga_result fpga_base(struct n5010 *n5010)
{
	fpga_result res;

	while (1) {
		uint64_t header;
		struct {
			union {
				uuid_t u128;
				uint64_t u64[2];
			};
		} uuid;
		int i;


		res = fpgaReadMMIO64(n5010->handle, 0, n5010->base, &header);
		if (res != FPGA_OK) {
			fprintf(stderr, "failed to read dfh: %s\n",
				fpgaErrStr(res));
			break;
		}

		res = fpgaReadMMIO64(n5010->handle, 0, n5010->base + 8,
				     &uuid.u64[1]);
		if (res != FPGA_OK) {
			fprintf(stderr, "failed to read uuid high: %s\n",
				fpgaErrStr(res));
			break;
		}

		res = fpgaReadMMIO64(n5010->handle, 0, n5010->base + 16,
				     &uuid.u64[0]);
		if (res != FPGA_OK) {
			fprintf(stderr, "failed to read uuid low: %s\n",
				fpgaErrStr(res));
			break;
		}

		for (i = 0; i < 2; i++)
			uuid.u64[i] = be64toh(uuid.u64[i]);

		if (DFH_TYPE(header) == DFH_TYPE_AFU
		    && uuid_compare(uuid.u128, n5010->guid) == 0) {
		}
		break;

		if (DFH_EOL(header)) {
			fprintf(stderr, "no matching dfh found: 0x%016jx\n",
				header);
			res = FPGA_NOT_FOUND;
			break;
		}

		if (DFH_NEXT(header) == 0 || DFH_NEXT(header) == 0xffff) {
			fprintf(stderr, "next dfh not found: 0x%016jx\n",
				header);
			res = FPGA_NOT_FOUND;
			break;
		}

		n5010->base += DFH_NEXT(header);
	}

	return res;
}

static void fpga_dump(struct n5010 *n5010, uint64_t offset, size_t count)
{
	uint64_t base = n5010->base + offset;

	if (!n5010->debug)
		return;

	for (size_t i = 0; i < count; i++) {
		uint64_t reg = base + i * 8;
		uint64_t val;

		fpgaReadMMIO64(n5010->handle, 0, reg, &val);
		printf("reg: 0x%04jx, val: 0x%016jx\n", reg, val);
	}
}


static fpga_result fpga_set_switch(struct n5010 *n5010)
{
	fpga_result res;

	uint64_t offset = CVL0_QSFP01_SWITCH;
	uint64_t port = n5010->port;
	if (n5010->port > 1) {
		port -= 2;
		offset = CVL1_QSFP23_SWITCH;
	}


	res = fpgaWriteMMIO64(n5010->handle, 0, n5010->base + offset, port);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to set switch: %s\n", fpgaErrStr(res));
		goto error;
	}

error:
	return res;
}

static fpga_result fpga_read_switch_port(struct n5010 *n5010, uint32_t cvl)
{
	uint64_t port = 0;
	uint64_t offset = CVL0_QSFP01_SWITCH;

	if (cvl == 1)
		offset = CVL1_QSFP23_SWITCH;

	fpgaReadMMIO64(n5010->handle, 0, n5010->base + offset, &port);
	printf("CVL: %u connected to front port: %lu\n", cvl, port + (cvl * 2));
	return 0;
}

static fpga_result fpga_read_switch(struct n5010 *n5010)
{
	fpga_read_switch_port(n5010, 0);
	fpga_read_switch_port(n5010, 1);
	return 0;
}

static fpga_result fpga_setread_switch(struct n5010 *n5010)
{
	fpga_result res;

	if (n5010->port == INVALID_PORT) {
		fprintf(stderr, "no port given\n");
		return EXIT_FAILURE;
	}

	res = fpga_set_switch(n5010);
	fpga_read_switch(n5010);
	return res;
}

static fpga_result fpga_run(__attribute__((unused)) struct n5010 *n5010)
{
	printf("Press ctrl-c to stop\n");
	while (!stop) {
		sleep(1);
	}
	return 0;
}

static void print_usage(FILE *f)
{
	fprintf(f,
		"usage: %s [<options>]\n"
		"\n"
		"options:\n"
		"  --help (-h)  print this help\n"
		"  --guid (-g)  uuid of accelerator to open, default: %s\n"
		"  --mode (-m)  test mode to execute. Known modes:\n"
		"               readsw, setsw -p <port>, runsw\n"
		"  --debug (-d) enable debug print of register values\n"
		" [-S <segment>] [-B <bus>] [-D <device>] [-F <function>] [PCI_ADDR]\n"
		"\n",
		program_invocation_short_name, default_guid);
}

static bool parse_port(struct n5010 *n5010, const char *port_str)
{
	char *endptr;
	unsigned long int port = strtoul(port_str, &endptr, 0);
	if (port_str == endptr) {
		fprintf(stderr, "missing port number: '%s'\n", port_str);
		return false;
	}

	if (port > MAX_PORT) {
		fprintf(stderr, "port number too big: '%lu'\n", port);
		return false;
	}

	n5010->port = port;
	return true;
}

static bool parse_mode(struct n5010 *n5010, const char *mode)
{
	const struct n5010_test *t;
	size_t count = sizeof(n5010_test) / sizeof(*n5010_test);
	size_t i;

	for (i = 0; i < count; i++) {
		t = &n5010_test[i];

		if (strcmp(t->name, mode) == 0)
			break;
	}

	if (i == count) {
		fprintf(stderr, "invalid mode setting: '%s'\n", mode);
		return false;
	}

	n5010->test = t;

	return true;
}

static int parse_args(int argc, char **argv, struct n5010 *n5010)
{
	struct option options[] = {
		{"help", no_argument, NULL, 'h'},
		{"guid", required_argument, NULL, 'g'},
		{"mode", required_argument, NULL, 'm'},
		{"port", required_argument, NULL, 'p'},
		{"debug", no_argument, NULL, 'd'},
		{NULL, 0, NULL, 0},
	};

	int c;
	n5010->port = INVALID_PORT;

	while (1) {
		c = getopt_long_only(argc, argv, "hg:m:p:d", options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage(stdout);
			exit(EXIT_SUCCESS);
		case 'g':
			if (uuid_parse(optarg, n5010->guid) < 0) {
				fprintf(stderr, "unparsable uuid: '%s'\n",
					optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'm':
			if (!parse_mode(n5010, optarg))
				return EXIT_FAILURE;
			break;
		case 'p':
			if (!parse_port(n5010, optarg))
				return EXIT_FAILURE;
			break;
		case 'd':
			n5010->debug = true;
			break;
		case '?':
		default:
			return EXIT_FAILURE;
		}
	}

	if (uuid_is_null(n5010->guid)) {
		fprintf(stderr, "Using default guid %s\n", default_guid);
		uuid_parse(default_guid, n5010->guid);
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	struct n5010 n5010 = {
		.filter = NULL, .test = n5010_test, .debug = false};
	fpga_result res;
	int err;

	res = fpgaGetProperties(NULL, &n5010.filter);
	if (res != FPGA_OK) {
		fprintf(stderr, "failed to get properties: %s\n",
			fpgaErrStr(res));
		return res;
	}

	if (opae_set_properties_from_args(n5010.filter, &res, &argc, argv)) {
		fprintf(stderr, "failed arg parse.\n");
		res = FPGA_EXCEPTION;
		goto error2;
	} else if (res) {
		fprintf(stderr, "failed to set properties.\n");
		goto error2;
	}

	err = parse_args(argc, argv, &n5010);
	if (err != 0)
		goto error2;

	// Install Control-C handler
	signal(SIGINT, signal_callback_handler);

	res = fpga_open(&n5010);
	if (res != FPGA_OK)
		goto error;

	res = fpga_base(&n5010);
	if (res != FPGA_OK)
		goto error;

	fpga_dump(&n5010, 0, 8);

	res = n5010.test->func(&n5010);
	if (res != FPGA_OK)
		goto error;
	fpga_dump(&n5010, 0, 8);

error:
	fpga_close(&n5010);


error2:
	fpgaDestroyProperties(&n5010.filter);


	return res == FPGA_OK ? EXIT_SUCCESS : res;
}
