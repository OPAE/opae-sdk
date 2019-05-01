// Copyright(c) 2019, Intel Corporation
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>

#include "safe_string/safe_string.h"

#include <opae/init.h>
#include <opae/utils.h>
#include "pluginmgr.h"
#include "opae_int.h"

#define HOME_CFG_PATHS 3
STATIC const char *_opae_home_cfg_files[HOME_CFG_PATHS] = {
	"/.local/opae_ase.cfg",
	"/.local/opae/opae_ase.cfg",
	"/.config/opae/opae_ase.cfg",
};
#define SYS_CFG_PATHS 2
STATIC const char *_opae_sys_cfg_files[SYS_CFG_PATHS] = {
	"/usr/local/etc/opae/opae_ase.cfg",
	"/etc/opae/opae_ase.cfg",
};

// Find the canonicalized configuration file. If null, the file was not found.
// Otherwise, it's the first configuration file found from a list of possible
// paths. Note: The char * returned is allocated here, caller must free.
STATIC char *find_ase_cfg()
{
	int i = 0;
	char *file_name = NULL;
	char home_cfg[PATH_MAX] = {0};
	char *home_cfg_ptr = &home_cfg[0];
	// get the user's home directory
	struct passwd *user_passwd = getpwuid(getuid());

	// first look in possible paths in the users home directory
	for (i = 0; i < HOME_CFG_PATHS; ++i) {
		if (strcpy_s(home_cfg, PATH_MAX, user_passwd->pw_dir)) {
			OPAE_ERR("error copying pw_dir string");
			return NULL;
		}
		home_cfg_ptr = home_cfg + strlen(home_cfg);
		if (strcpy_s(home_cfg_ptr, PATH_MAX, _opae_home_cfg_files[i])) {
			OPAE_ERR("error copying opae cfg dir string: %s",
				 _opae_home_cfg_files[i]);
			return NULL;
		}
		file_name = canonicalize_file_name(home_cfg);
		if (file_name) {
			return file_name;
		} else {
			home_cfg[0] = '\0';
		}
	}
	// now look in possible system paths
	for (i = 0; i < SYS_CFG_PATHS; ++i) {
		strcpy_s(home_cfg, PATH_MAX, _opae_sys_cfg_files[i]);
		file_name = canonicalize_file_name(home_cfg);
		if (file_name) {
			return file_name;
		}
	}
	return NULL;
}

__attribute__((constructor)) STATIC void opae_ase_init(void)
{
	char *cfg_path = find_ase_cfg();

	setenv("OPAE_EXPLICIT_INITIALIZE", "yes", 0);

	fpgaInitialize(cfg_path);
	free(cfg_path);
}

__attribute__((destructor)) STATIC void opae_ase_release(void)
{
	fpga_result res;

	res = fpgaFinalize();
	if (res != FPGA_OK)
		OPAE_ERR("fpgaFinalize: %s", fpgaErrStr(res));
}