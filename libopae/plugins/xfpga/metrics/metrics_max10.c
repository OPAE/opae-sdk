#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H


#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <glob.h>


#include "common_int.h"
#include "metrics_int.h"
#include "types_int.h"
#include "sysfs_int.h"
#include "opae/metrics.h"
#include "metrics/vector.h"
#include "xfpga.h"
#include "safe_string/safe_string.h"
#include "metrics/metrics_metadata.h"
#include "metrics/max10_metadata.h"


fpga_result read_sysfs_file(const char *sysfs, const char *file,
			void **buf, uint32_t *tot_bytes_ret)
{
	char sysfspath[SYSFS_PATH_MAX];
	struct stat stats;
	int fd = 0;
	fpga_result res = FPGA_OK;


	if (sysfs == NULL ||
		file == NULL ||
		buf == NULL ||
		tot_bytes_ret == NULL) {
		FPGA_ERR("Invlaid Input parameters");
		return FPGA_INVALID_PARAM;
	}
	*buf = NULL;
	*tot_bytes_ret = 0;


	snprintf_s_ss(sysfspath, sizeof(sysfspath), "%s/%s", sysfs, file);

	glob_t pglob;
	int gres = glob(sysfspath, GLOB_NOSORT, NULL, &pglob);
	if ((gres) || (1 != pglob.gl_pathc)) {
		globfree(&pglob);
		return FPGA_NOT_FOUND;
	}

	fd = open(pglob.gl_pathv[0], O_RDONLY);
	globfree(&pglob);
	if (fd < 0) {
		return FPGA_NOT_FOUND;
	}

	if (fstat(fd, &stats) != 0) {
		close(fd);
		return FPGA_NOT_FOUND;
	}

	// fstat for a sysfs file is not accurate for the BMC
	// Read the entire file into a temp buffer to get actual size of file
	*buf = (void *)calloc(stats.st_size, 1);

	int32_t tot_bytes = 0;
	int32_t bytes_read = 0;
	do {
		bytes_read = (int32_t)read(fd, *buf, stats.st_size);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				bytes_read = 1; // Fool the while loop
				continue;
			}
		}
		tot_bytes += bytes_read;
	} while ((tot_bytes < stats.st_size) && (bytes_read > 0));

	close(fd);

	if ((tot_bytes > stats.st_size) || (bytes_read < 0)) {
		res = FPGA_EXCEPTION;
		free(*buf);
		*buf = NULL;
		goto out;
	}

	*tot_bytes_ret = tot_bytes;

out:
	return res;
}


fpga_result  enum_max10_metrics_info(struct _fpga_handle *_handle,
							fpga_metric_vector *vector,
							uint64_t *metric_num,
							enum fpga_hw_type  hw_type)
{
	fpga_result result                             = FPGA_OK;
	struct _fpga_token *_token                     = NULL;
	size_t i                                       = 0;
	char *tmp                                      = NULL;
	uint32_t tot_bytes                             = 0;
	enum fpga_metric_type metric_type              = FPGA_METRIC_TYPE_POWER;
	char sysfspath[SYSFS_PATH_MAX]                 = { 0 };;
	char metrics_sysfs_path[SYSFS_PATH_MAX]        = { 0 };
	char metric_name[SYSFS_PATH_MAX]               = { 0 };
	char group_name[SYSFS_PATH_MAX]                = { 0 };
	char group_sysfs[SYSFS_PATH_MAX]               = { 0 };
	char qualifier_name[SYSFS_PATH_MAX]            = { 0 };

	fpga_metric_metadata metric_data;
	glob_t pglob;

	if (_handle == NULL ||
		vector == NULL ||
		metric_num == NULL) {
		FPGA_ERR("Invlaid Input parameters");
		return FPGA_INVALID_PARAM;
	}

	_token = (struct _fpga_token *)_handle->token;
	if (_token == NULL) {
		FPGA_ERR("Invalid token within handle");
		return FPGA_INVALID_PARAM;
	}

	// metrics group
	snprintf_s_ss(sysfspath, sizeof(sysfspath), "%s/%s", _token->sysfspath, MAX10_SYSFS_PATH);
	int gres = glob(sysfspath, GLOB_NOSORT, NULL, &pglob);
	if ((gres) || (1 != pglob.gl_pathc)) {
		FPGA_ERR("Failed pattern match %s: %s", sysfspath, strerror(errno));
		globfree(&pglob);
		return FPGA_NOT_FOUND;
	}

	snprintf_s_s(group_sysfs, sizeof(group_sysfs), "%s", pglob.gl_pathv[0]);
	globfree(&pglob);

	// Enum sensors
	snprintf_s_ss(sysfspath, sizeof(sysfspath), "%s/%s", _token->sysfspath, MAX10_SENSOR_SYSFS_PATH);
	gres = glob(sysfspath, GLOB_NOSORT, NULL, &pglob);
	if (gres) {
		FPGA_ERR("Failed pattern match %s: %s", sysfspath, strerror(errno));
		globfree(&pglob);
		return FPGA_NOT_FOUND;
	}


	// for loop
	for (i = 0; i < pglob.gl_pathc; i++) {

		// Sensor name
		result = read_sysfs_file(pglob.gl_pathv[i], SENSOR_SYSFS_NAME, (void **)&tmp, &tot_bytes);
		if (FPGA_OK != result) {
			if (tmp) {
				free(tmp);
			}
			continue;
		}

		memset_s(&metric_name, sizeof(metric_name), 0);
		snprintf_s_s(metric_name, sizeof(metric_name), "%s", (char *)tmp);
		if (tmp) {
			free(tmp);
		}
		metric_name[strlen(metric_name)-1] = '\0';

		// Metrics typw
		result = read_sysfs_file(pglob.gl_pathv[i], SENSOR_SYSFS_TYPE, (void **)&tmp, &tot_bytes);
		if (FPGA_OK != result) {
			if (tmp) {
				free(tmp);
				continue;
			}

		}

		// Metrics group name
		if (strstr(tmp, VOLTAGE) || strstr(tmp, CURRENT) || strstr(tmp, POWER)) {
			metric_type = FPGA_METRIC_TYPE_POWER;
			snprintf_s_s(group_name, sizeof(group_name), "%s", PWRMGMT);
			snprintf_s_ss(qualifier_name, sizeof(qualifier_name), "%s:%s", PWRMGMT, metric_name);

		} else if (strstr(tmp, TEMPERATURE)) {
			metric_type = FPGA_METRIC_TYPE_THERMAL;
			snprintf_s_s(group_name, sizeof(group_name), "%s", THERLGMT);
			snprintf_s_ss(qualifier_name, sizeof(qualifier_name), "%s:%s", THERLGMT, metric_name);
		} else {
			printf("FPGA_METRIC_TYPE_UNKNOWN \n");
			metric_type = FPGA_METRIC_TYPE_UNKNOWN;
		}

		result = get_metric_data_info(group_name, metric_name, fpga_max10_metric_metadata, MAX10_MDATA_SIZE, &metric_data);
		if (result != FPGA_OK) {
			FPGA_MSG("Failed to get metric metadata ");
			continue;
		}

		if (tmp) {
			free(tmp);
		}

		// value sysfs path
		snprintf_s_ss(metrics_sysfs_path, sizeof(metrics_sysfs_path), "%s/%s", pglob.gl_pathv[i], SENSOR_SYSFS_VALUE);

		result = add_metric_vector(vector, *metric_num, qualifier_name, group_name, group_sysfs, metric_name, metrics_sysfs_path, metric_data.metric_units,
			FPGA_METRIC_DATATYPE_DOUBLE, metric_type, hw_type, 0);
		if (result != FPGA_OK) {
			FPGA_ERR("Failed to add metrics");
			goto out;
		}

		*metric_num = *metric_num + 1;

	} // end for loop

out:
	globfree(&pglob);
	return result;
}