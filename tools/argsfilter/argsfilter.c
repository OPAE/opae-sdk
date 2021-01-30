// Copyright(c) 2018-2021, Intel Corporation
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include "argsfilter.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

#ifdef _WIN32
#define EX_OK 0
#define EX_USAGE (-1)
#define EX_SOFTWARE (-2)
#else
#include <sysexits.h>
#endif

struct _args_filter_config {
	int segment;
	int bus;
	int device;
	int function;
};

STATIC bool get_pci_address(const char *addr,
			    struct _args_filter_config *c)
{
	regex_t re;
	regmatch_t matches[5];

	//           11
	// 012345678901
	// ssss:bb:dd.f
	char address[32];

	const char *sbdf = "([0-9a-fA-F]{4}):"
			   "([0-9a-fA-F]{2}):"
			   "([0-9a-fA-F]{2})\\."
			   "([0-7])";

	const char *bdf = "([0-9a-fA-F]{2}):"
			  "([0-9a-fA-F]{2})\\."
			  "([0-7])";

	bool is_match = false;

	size_t len = strlen(addr);

	if (len > 12)
		return false;

	memcpy(address, addr, len + 1);
	address[len] = '\0';

	memset(matches, 0, sizeof(matches));
	regcomp(&re, sbdf, REG_EXTENDED);

	if (regexec(&re,
		    address,
		    sizeof(matches) / sizeof(matches[0]),
		    matches,
		    0) == 0) {
		is_match = true;
	}

	regfree(&re);

	if (is_match) {
		address[matches[1].rm_eo] = '\0';
		c->segment = (int) strtoul(&address[matches[1].rm_so],
					   NULL, 16);

		address[matches[2].rm_eo] = '\0';
		c->bus = (int) strtoul(&address[matches[2].rm_so],
				       NULL, 16);

		address[matches[3].rm_eo] = '\0';
		c->device = (int) strtoul(&address[matches[3].rm_so],
					  NULL, 16);

		c->function = (int) strtoul(&address[matches[4].rm_so],
					    NULL, 10);
		return true;
	}

	memset(matches, 0, sizeof(matches));
	regcomp(&re, bdf, REG_EXTENDED);

	if (regexec(&re,
		    address,
		    sizeof(matches) / sizeof(matches[0]),
		    matches,
		    0) == 0) {
		is_match = true;
	}

	regfree(&re);

	if (is_match) {
		c->segment = 0;

		address[matches[1].rm_eo] = '\0';
		c->bus = (int) strtoul(&address[matches[1].rm_so],
				       NULL, 16);

		address[matches[2].rm_eo] = '\0';
		c->device = (int) strtoul(&address[matches[2].rm_so],
					  NULL, 16);

		c->function = (int) strtoul(&address[matches[3].rm_so],
					    NULL, 10);
		return true;
	}

	return false;
}

#define RETURN_ON_ERR(res, desc)                                               \
	do {                                                                   \
		if ((res) != FPGA_OK) {                                        \
			optind = 1;                                            \
			opterr = old_opterr;                                   \
			fprintf(stderr, "Error %s: %s\n", (desc),              \
				fpgaErrStr(res));                              \
			return EX_SOFTWARE;                                    \
		}                                                              \
	} while (0)

int set_properties_from_args(fpga_properties filter, fpga_result *result,
			     int *argc, char *argv[])
{
	// prefix the short options with '-' so that unrecognized options are
	// ignored
	const char *short_opts = "-:B:D:F:S:";
	struct option longopts[] = {
		{ "segment",  required_argument, NULL, 'S'},
		{ "bus",      required_argument, NULL, 'B'},
		{ "device",   required_argument, NULL, 'D'},
		{ "function", required_argument, NULL, 'F'},
		{0, 0, 0, 0},
	};
	int supported_options = sizeof(longopts) / sizeof(longopts[0]) - 1;
	int getopt_ret = -1;
	int option_index = 0;
	char *endptr = NULL;
	int found_opts[] = {0, 0, 0, 0 };
	int next_found = 0;
	int old_opterr;

	struct _args_filter_config args_filter_config = {
		.segment = -1,
		.bus = -1,
		.device = -1,
		.function = -1
	};

	old_opterr = opterr;
	opterr = 0;

	while (-1
	       != (getopt_ret = getopt_long(*argc, argv, short_opts, longopts,
					    &option_index))) {
		const char *tmp_optarg = optarg;

		if ((optarg) && ('=' == *tmp_optarg))
			++tmp_optarg;

		switch (getopt_ret) {
		case 'B': /* bus */
			if (NULL == tmp_optarg)
				break;
			endptr = NULL;
			args_filter_config.bus =
				(int)strtoul(tmp_optarg, &endptr, 0);
			if (endptr != tmp_optarg + strlen(tmp_optarg)) {
				fprintf(stderr, "invalid bus: %s\n",
					tmp_optarg);
				return EX_USAGE;
			}
			found_opts[next_found++] = optind - 2;
			break;

		case 'D': /* device */
			if (NULL == tmp_optarg)
				break;
			endptr = NULL;
			args_filter_config.device =
				(int)strtoul(tmp_optarg, &endptr, 0);
			if (endptr != tmp_optarg + strlen(tmp_optarg)) {
				fprintf(stderr, "invalid device: %s\n",
					tmp_optarg);
				return EX_USAGE;
			}
			found_opts[next_found++] = optind - 2;
			break;

		case 'F': /* function */
			if (NULL == tmp_optarg)
				break;
			endptr = NULL;
			args_filter_config.function =
				(int)strtoul(tmp_optarg, &endptr, 0);
			if (endptr != tmp_optarg + strlen(tmp_optarg)) {
				fprintf(stderr, "invalid function: %s\n",
					tmp_optarg);
				return EX_USAGE;
			}
			found_opts[next_found++] = optind - 2;
			break;

		case 'S': /* segment */
			if (NULL == tmp_optarg)
				break;
			endptr = NULL;
			args_filter_config.segment =
				(int)strtoul(tmp_optarg, &endptr, 0);
			if (endptr != tmp_optarg + strlen(tmp_optarg)) {
				fprintf(stderr, "invalid segment: %s\n",
					tmp_optarg);
				return EX_USAGE;
			}
			found_opts[next_found++] = optind - 2;
			break;
		case ':': /* missing option argument */
			fprintf(stderr, "Missing option argument\n");
			return EX_USAGE;

		case '?':
			break;
		case 1:
			break;
		default: /* invalid option */
			fprintf(stderr, "Invalid cmdline options\n");
			return EX_USAGE;
		}
	}

	// using the list of optind values
	// shorten the argv vector starting with a decrease
	// of 2 and incrementing that amount by two for each option found
	int removed = 0;
	int i, j;
	for (i = 0; i < supported_options; ++i) {
		if (found_opts[i]) {
			for (j = found_opts[i] - removed; j < *argc - 2; j++) {
				argv[j] = argv[j + 2];
			}
			removed += 2;
		} else {
			break;
		}
	}
	*argc -= removed;

	for (i = 0 ; i < *argc ; ++i) {
		if (get_pci_address(argv[i], &args_filter_config)) {
			break;
		}
	}

	if (-1 != args_filter_config.segment) {
		*result = fpgaPropertiesSetSegment(
			filter, args_filter_config.segment);
		RETURN_ON_ERR(*result, "setting segment");
	}

	if (-1 != args_filter_config.bus) {
		*result = fpgaPropertiesSetBus(filter, args_filter_config.bus);
		RETURN_ON_ERR(*result, "setting bus");
	}
	if (-1 != args_filter_config.device) {
		*result = fpgaPropertiesSetDevice(filter,
						  args_filter_config.device);
		RETURN_ON_ERR(*result, "setting device");
	}

	if (-1 != args_filter_config.function) {
		*result = fpgaPropertiesSetFunction(
			filter, args_filter_config.function);
		RETURN_ON_ERR(*result, "setting function");
	}

	// restore getopt variables
	// setting optind to zero will cause getopt to reinitialize for future
	// calls within the program
	optind = 0;
	opterr = old_opterr;
	return EX_OK;
}
