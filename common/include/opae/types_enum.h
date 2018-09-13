// Copyright(c) 2017, Intel Corporation
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

/**
 * @file types_enum.h
 * @brief Definitions of enumerated types for the OPAE C API
 *
 * This file defines return and error codes, event and object types, states,
 * and flags as used or reported by OPAE C API functions.
 */

#ifndef __FPGA_TYPES_ENUM_H__
#define __FPGA_TYPES_ENUM_H__

/**
 * OPAE C API function return codes
 *
 * Every public API function exported by the OPAE C library will return one of
 * these codes. Usually, FPGA_OK denotes successful completion of the requested
 * operation, while any return code *other* than FPGA_OK indicates an error or
 * other deviation from the expected behavior. Users of the OPAE C API should
 * always check the return codes of the APIs they call, and not use output
 * parameters of functions that did not execute successfully.

 * The fpgaErrStr() function converts error codes into printable messages.
 *
 * OPAE also has a logging mechanism that allows a developer to get more
 * information about why a particular call failed with a specific message. If
 * enabled, any function that returns an error code different from FPGA_OK will
 * also print out a message with further details. This mechanism can be enabled
 * by setting the environment variable `LIBOPAE_LOG` to 1 before running the
 * respective application.
 */
typedef enum {
	FPGA_OK = 0,         /**< Operation completed successfully */
	FPGA_INVALID_PARAM,  /**< Invalid parameter supplied */
	FPGA_BUSY,           /**< Resource is busy */
	FPGA_EXCEPTION,      /**< An exception occurred */
	FPGA_NOT_FOUND,      /**< A required resource was not found */
	FPGA_NO_MEMORY,      /**< Not enough memory to complete operation */
	FPGA_NOT_SUPPORTED,  /**< Requested operation is not supported */
	FPGA_NO_DRIVER,      /**< Driver is not loaded */
	FPGA_NO_DAEMON,      /**< FPGA Daemon (fpgad) is not running */
	FPGA_NO_ACCESS,      /**< Insufficient privileges or permissions */
	FPGA_RECONF_ERROR    /**< Error while reconfiguring FPGA */
} fpga_result;

/**
 * FPGA events
 *
 * OPAE currently defines the following event types that applications can
 * register for. Note that not all FPGA resources and target platforms may
 * support all event types.
 */
typedef enum {
	FPGA_EVENT_INTERRUPT = 0,   /**< Interrupt generated by an accelerator */
	FPGA_EVENT_ERROR,           /**< Infrastructure error event */
	FPGA_EVENT_POWER_THERMAL    /**< Infrastructure thermal event */
} fpga_event_type;

/* TODO: consider adding lifecycle events in the future
 * to help with orchestration.  Need a complete specification
 * before including them in the API.  Proposed events:
 *	FPGA_EVENT_APPEAR
 *	FPGA_EVENT_DISAPPEAR
 *	FPGA_EVENT_CHANGE
 */

/** accelerator state */
typedef enum {
	/** accelerator is opened exclusively by another process */
	FPGA_ACCELERATOR_ASSIGNED = 0,
	/** accelerator is free to be opened */
	FPGA_ACCELERATOR_UNASSIGNED
} fpga_accelerator_state;

/**
 * OPAE FPGA resources (objects)
 *
 * These are the FPGA resources currently supported by the OPAE object model.
 */
typedef enum {
	/** FPGA_DEVICE objects represent FPGA devices and their management functionality.
	* These objects can be opened (typically requires a certain privilege level or
	* access permissions) and used for management functions like fpgaReconfigreSlot(). */
	FPGA_DEVICE = 0,
	/** FPGA_ACCELERATOR objects represent allocatable units for accessing
	* accelerated functions on the FPGA. They are frequently opened for
	* interacting via control registers (MMIO), shared memory, or other,
	* possibly platform-specific functions. */
	FPGA_ACCELERATOR
} fpga_objtype;

/**
 * Buffer flags
 *
 * These flags can be passed to the fpgaPrepareBuffer() function.
 */
enum fpga_buffer_flags {
	FPGA_BUF_PREALLOCATED = (1u << 0), /**< Use existing buffer */
	FPGA_BUF_QUIET = (1u << 1)         /**< Suppress error messages */
};

/**
 * Open flags
 *
 * These flags can be passed to the fpgaOpen() function.
 */
enum fpga_open_flags {
	/** Open FPGA resource for shared access */
	FPGA_OPEN_SHARED = (1u << 0)
};

/**
 * Reconfiguration flags
 *
 * These flags can be passed to the fpgaReconfigureSlot() function.
 */
enum fpga_reconf_flags {
	/** Reconfigure the slot without checking if it is in use */
	FPGA_RECONF_FORCE = (1u << 0)
};

enum fpga_object_read_flags {
  FPGA_OBJECT_SYNC = (1u << 0), /**< Synchronize data from driver */
  FPGA_OBJECT_GLOB = (1u << 1), /**< Treat names as glob expressions */
  FPGA_OBJECT_TEXT = (1u << 2), /**< Parse or convert numeric data as text */
  FPGA_OBJECT_RECURSE_ONE = (1u << 3), /**< Create subobjects one level down from containers */
  FPGA_OBJECT_RECURSE_ALL = (1u << 4) /**< Create subobjects all levels from from containers */
};

#endif // __FPGA_TYPES_ENUM_H__
