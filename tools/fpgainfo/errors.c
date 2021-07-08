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
/*
 * @file errors.c
 *
 * @brief fpga error reporting
 *
 */
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fpgainfo.h"
#include <opae/properties.h>
#include "errors.h"
#include "errors_metadata.h"

#define FPGA_BIT_IS_SET(val, index) (((val) >> (index)) & 1)

const char *supported_verbs[] = {"all", "fme", "port"};
enum verbs_index { VERB_ALL = 0, VERB_FME, VERB_PORT, VERB_MAX };

#define FPGA_FME_ERROR_STR          "Fme Errors"
#define FPGA_PCIE0_ERROR_STR        "PCIe0 Errors"
#define FPGA_INJECT_ERROR_STR       "Inject Error"
#define FPGA_CATFATAL_ERROR_STR     "Catfatal Errors"
#define FPGA_NONFATAL_ERROR_STR     "Nonfatal Errors"
#define FPGA_PCIE1_ERROR_STR        "PCIe1 Errors"
#define FPGA_PORT_ERROR_STR         "Errors"

/*
 * errors command configuration, set during parse_args()
 */
static struct errors_config {
	bool clear;
	int force_count;
	enum verbs_index which;
	bool help_only;
} errors_config = {.clear = false, .which = VERB_ALL, .help_only = false};

/*
 * Print help
 */
void errors_help(void)
{
	unsigned int i;

	printf("\nPrint and clear errors\n"
	       "        fpgainfo errors [-h] [-c] {");
	printf("%s", supported_verbs[0]);
	for (i = 1; i < sizeof(supported_verbs) / sizeof(supported_verbs[0]);
	     i++) {
		printf(",%s", supported_verbs[i]);
	}
	printf("}\n\n"
	       "                -h,--help           Print this help\n"
	       "                -c,--clear          Clear all errors\n"
	       "                --force             Retry clearing errors 64 times\n"
	       "                                    to clear certain error conditions\n"
	       "\n");
	errors_config.help_only = true;
}

#define ERRORS_GETOPT_STRING ":chf"
int parse_error_args(int argc, char *argv[])
{
	optind = 0;
	struct option longopts[] = {
		{"clear", no_argument, NULL, 'c'},
		{"force", no_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0},
	};

	int getopt_ret;
	int option_index;
	errors_config.force_count = 1;

	while (-1
	       != (getopt_ret = getopt_long(argc, argv, ERRORS_GETOPT_STRING,
					    longopts, &option_index))) {
		const char *tmp_optarg = optarg;

		if ((optarg) && ('=' == *tmp_optarg)) {
			++tmp_optarg;
		}

		switch (getopt_ret) {
		case 'c': /* clear */
			errors_config.clear = true;
			break;

		case 'f': /* Force */
			errors_config.clear = true;
			errors_config.force_count = 64;
			break;

		case 'h': /* help */
			errors_help();
			return -1;

		case ':': /* missing option argument */
			OPAE_ERR("Missing option argument\n");
			errors_help();
			return -1;

		case '?':
		default: /* invalid option */
			OPAE_ERR("Invalid cmdline options\n");
			errors_help();
			return -1;
		}
	}

	// The word after 'errors' should be what to operate on ("all", "fme",
	// or "port")
	optind++;
	if (argc < optind + 1) {
		OPAE_ERR("Not enough parameters\n");
		errors_help();
		return -1;
	}

	if ((optind < argc) &&
		!strcmp(argv[optind - 1], "errors")) {
		char *verb = argv[optind];
		size_t idx = str_in_list(verb, supported_verbs, VERB_MAX);
		if (idx < VERB_MAX) {
			errors_config.which = idx;
		} else {
			OPAE_ERR("Not a valid errors resource spec: %s\n", verb);
			errors_help();
			return -1;
		}
	} else {
		OPAE_ERR("Not a valid errors resource spec: %s\n",
			argv[optind - 1]);
		errors_help();
		return -1;
	}

	return 0;
}

fpga_result errors_filter(fpga_properties *filter, int argc, char *argv[])
{
	fpga_result res = FPGA_OK;
	if (0 == parse_error_args(argc, argv)) {
		switch (errors_config.which) {
		case VERB_FME:
			res = fpgaPropertiesSetObjectType(*filter, FPGA_DEVICE);
			ON_FPGAINFO_ERR_GOTO(res, out,
					     "setting type to FPGA_DEVICE");
			break;
		case VERB_PORT:
			res = fpgaPropertiesSetObjectType(*filter,
							  FPGA_ACCELERATOR);
			ON_FPGAINFO_ERR_GOTO(
				res, out, "setting type to FPGA_ACCELERATOR");
			break;
		case VERB_ALL:
		default:
			break;
		}
	}
out:
	return res;
}

static fpga_result get_error_revision(fpga_token token, uint64_t *value)
{
	fpga_result res = FPGA_OK;
	fpga_object fpga_object;

	res = fpgaTokenGetObject(token, "*error*/revision", &fpga_object, FPGA_OBJECT_GLOB);
	if (res != FPGA_OK) {
		OPAE_MSG("Failed to get token Object");
		return res;
	}

	res = fpgaObjectRead64(fpga_object, value, 0);
	if (res != FPGA_OK) {
		OPAE_MSG("Failed to Read object ");
		fpgaDestroyObject(&fpga_object);
		return res;
	}

	res = fpgaDestroyObject(&fpga_object);
	if (res != FPGA_OK) {
		OPAE_MSG("Failed to Destroy Object");
	}
	return res;
}

// print error string format
static void print_errors_str(struct fpga_error_info errinfo,
				uint64_t error_value,
				uint64_t revision)
{
	uint64_t i = 0;
	uint64_t j = 0;
	enum fapg_error_type error_type  = FPGA_ERROR_UNKNOWN;

	if (!strcmp(errinfo.name, FPGA_FME_ERROR_STR)) {
		error_type = FPGA_FME_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_PCIE0_ERROR_STR)) {
		error_type = FPGA_PCIE0_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_INJECT_ERROR_STR)) {
		error_type = FPGA_INJECT_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_CATFATAL_ERROR_STR)) {
		error_type = FPGA_CATFATAL_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_NONFATAL_ERROR_STR)) {
		error_type = FPGA_NONFATAL_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_PCIE1_ERROR_STR)) {
		error_type = FPGA_PCIE1_ERROR;
	} else if (!strcmp(errinfo.name, FPGA_PORT_ERROR_STR)) {
		error_type = FPGA_PORT_ERROR;
	}

	for (i = 0; i < FPGA_ERR_METADATA_COUNT; i++) {
		if ((fpga_errors_metadata[i].error_type == error_type) &&
			(fpga_errors_metadata[i].revision == revision)) {

			for (j = 0; j < fpga_errors_metadata[i].array_size_max; j++) {
				if (FPGA_BIT_IS_SET(error_value, j)) {
					printf("bit %ld error:%s\n",
					j, fpga_errors_metadata[i].str_err[j].err_str);
				}
			} // end for
		}
	} // end for
	return;
}

static void print_errors_info(fpga_token token, fpga_properties props,
			      struct fpga_error_info *errinfos,
			      uint32_t num_errors)
{
	int i;
	fpga_result res = FPGA_OK;
	fpga_objtype objtype;
	uint64_t revision = 0;
	if ((NULL == errinfos) || (0 == num_errors)) {
		return;
	}

	if (errors_config.clear) {
		for (i = 0; i < errors_config.force_count; i++) {
			fpgaClearAllErrors(token);
		}
	}

	res = fpgaPropertiesGetObjectType(props, &objtype);
	fpgainfo_print_err("reading objtype from properties", res);

	if (((VERB_ALL == errors_config.which)
	     || (VERB_FME == errors_config.which))
	    && (FPGA_DEVICE == objtype)) {
		fpgainfo_print_common("//****** FME ERRORS ******//", props);

		for (i = 0; i < (int)num_errors; i++) {
			uint64_t error_value = 0;

			res = fpgaReadError(token, i, &error_value);
			fpgainfo_print_err("reading error for FME", res);

			printf("%-32s : 0x%" PRIX64 "\n", errinfos[i].name,
			       error_value);

			res = get_error_revision(token, &revision);
			if (res == FPGA_NOT_FOUND) {
				//Todo : fpga-upstream-dev branch remove the revision sysfs node.
				//if we check the revision is not present, we use the default value
				revision = 0;
			} else if (res != FPGA_OK) {
				OPAE_ERR("could not find FME error revision - skipping decode\n");
				continue;
			}

			if (error_value > 0)
				print_errors_str(errinfos[i], error_value, revision);

		}
	} else if (((VERB_ALL == errors_config.which)
		    || (VERB_PORT == errors_config.which))
		   && (FPGA_ACCELERATOR == objtype)) {
		fpgainfo_print_common("//****** PORT ERRORS ******//", props);

		for (i = 0; i < (int)num_errors; i++) {
			uint64_t error_value = 0;
			res = fpgaReadError(token, i, &error_value);
			fpgainfo_print_err("reading error for PORT", res);

			printf("%-32s : 0x%" PRIX64 "\n", errinfos[i].name,
			       error_value);

			res = get_error_revision(token, &revision);
			if (res == FPGA_NOT_FOUND) {
				//Todo : fpga-upstream-dev branch remove the revision sysfs node.
				//if we check the revision is not present, we use the default value
				revision = 0;
			} else if (res != FPGA_OK) {
				OPAE_ERR("could not find port error revision - skipping decode\n");
				continue;
			}

			if (error_value > 0)
				print_errors_str(errinfos[i], error_value, revision);

		}
	}
}

fpga_result errors_command(fpga_token *tokens, int num_tokens, int argc,
			   char *argv[])
{
	(void)argc;
	(void)argv;
	fpga_result res = FPGA_OK;
	fpga_properties props;
	struct fpga_error_info *errinfos = NULL;

	if (errors_config.help_only) {
		return res;
	}

	int i = 0;
	for (i = 0; i < num_tokens; ++i) {
		uint32_t num_errors = 0;

		res = fpgaGetProperties(tokens[i], &props);
		if (res == FPGA_OK) {
			res = fpgaPropertiesGetNumErrors(props, &num_errors);

			if ((res == FPGA_OK) && (num_errors != 0)) {
				int j;
				errinfos = (struct fpga_error_info *)calloc(
					num_errors, sizeof(*errinfos));
				if (!errinfos) {
					res = FPGA_NO_MEMORY;
					OPAE_ERR("Error allocating memory");
					goto destroy_and_free;
				}

				for (j = 0; j < (int)num_errors; j++) {
					res = fpgaGetErrorInfo(tokens[i], j,
							       &errinfos[j]);
					fpgainfo_print_err(
						"reading error info structure", res);
					replace_chars(errinfos[j].name, '_', ' ');
					upcase_pci(errinfos[j].name);
					upcase_first(errinfos[j].name);
				}

				print_errors_info(tokens[i], props, errinfos, num_errors);
			}

		destroy_and_free:
			if (errinfos)
				free(errinfos);
			errinfos = NULL;
			fpgaDestroyProperties(&props);
			if (res == FPGA_NO_MEMORY) {
			    break;
			}
		} else {
			fpgainfo_print_err("reading properties from token", res);
		}
	}

	return res;
}
