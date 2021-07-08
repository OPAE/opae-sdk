// Copyright(c) 2018, Intel Corporation
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
 * fpgainfo.h
 */
#ifndef FPGAINFO_H
#define FPGAINFO_H

#include <opae/fpga.h>
//#include "sysinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACTORY_BIT (1ULL << 36)

void fpgainfo_print_common(const char *hdr, fpga_properties props);

void fpgainfo_print_err(const char *s, fpga_result res);

// Replace occurrences of character within string
void replace_chars(char *str, char match, char rep);

// Turn all "pcie" into "PCIe"
void upcase_pci(char *str);

// Upper-case the first letter of each word in str
void upcase_first(char *str);

// Find string in list of strings
int str_in_list(const char *key, const char *list[], size_t size);

/*
 * macro to check FPGA return codes, print error message, and goto cleanup label
 * NOTE: this changes the program flow (uses goto)!
 */
#define ON_FPGAINFO_ERR_GOTO(res, label, desc)                                 \
	do {                                                                   \
		if ((res) != FPGA_OK) {                                        \
			fpgainfo_print_err((desc), (res));                     \
			goto label;                                            \
		}                                                              \
	} while (0)


#ifdef __cplusplus
}
#endif
#endif /* !FPGAINFO_H */
