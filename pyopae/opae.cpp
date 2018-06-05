#include <Python.h>

#include <opae/cxx/core/token.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pyproperties.h"

namespace py = pybind11;
using opae::fpga::types::properties;
using opae::fpga::types::token;

PYBIND11_MODULE(_opae, m) {
  m.doc() = "Open Programmable Acceleration Engine - Python bindings";

  // define enumerations
  py::enum_<fpga_result>(m, "fpga_result", py::arithmetic(),
                         "OPAE return codes")
      .value("FPGA_OK", FPGA_OK)
      .value("FPGA_INVALID_PARAM", FPGA_INVALID_PARAM)
      .value("FPGA_BUSY", FPGA_BUSY)
      .value("FPGA_EXCEPTION", FPGA_EXCEPTION)
      .value("FPGA_NOT_FOUND", FPGA_NOT_FOUND)
      .value("FPGA_NO_MEMORY", FPGA_NO_MEMORY)
      .value("FPGA_NOT_SUPPORTED", FPGA_NOT_SUPPORTED)
      .value("FPGA_NO_DRIVER", FPGA_NO_DRIVER)
      .value("FPGA_NO_DAEMON", FPGA_NO_DAEMON)
      .value("FPGA_NO_ACCESS", FPGA_NO_ACCESS)
      .value("FPGA_RECONF_ERROR", FPGA_RECONF_ERROR)
      .export_values();

  py::enum_<fpga_objtype>(m, "fpga_objtype", py::arithmetic(),
                          "OPAE resource objects")
      .value("FPGA_DEVICE", FPGA_DEVICE)
      .value("FPGA_ACCELERATOR", FPGA_ACCELERATOR)
      .export_values();

  py::enum_<fpga_open_flags>(m, "fpga_open_flags", py::arithmetic(),
                             "OPAE flags for opening resources")
      .value("FPGA_OPEN_SHARED", FPGA_OPEN_SHARED)
      .export_values();

  py::enum_<fpga_event_type>(m, "fpga_event_type", py::arithmetic(),
                             "OPAE event type")
      .value("FPGA_EVENT_INTERRUPT", FPGA_EVENT_INTERRUPT)
      .value("FPGA_EVENT_ERROR", FPGA_EVENT_ERROR)
      .value("FPGA_EVENT_POWER_THERMAL", FPGA_EVENT_POWER_THERMAL)
      .export_values();

  py::enum_<fpga_accelerator_state>(m, "fpga_accelerator_state",
                                    py::arithmetic(), "OPAE accelerator_state")
      .value("FPGA_ACCELERATOR_ASSIGNED", FPGA_ACCELERATOR_ASSIGNED)
      .value("FPGA_ACCELERATOR_UNASSIGNED", FPGA_ACCELERATOR_UNASSIGNED)
      .export_values();

  // define properties class
  py::class_<properties, properties::ptr_t> pyproperties(m, "properties");
  pyproperties.def_static("get", properties_get, properties_doc_get())
      .def_static("get", properties_get_token, properties_doc_get_token())
      .def_property("parent", properties_get_parent, properties_get_parent,
                    properties_doc_parent())
      .def_property("guid", properties_get_guid, properties_set_guid,
                    properties_doc_guid())
      .def_property("type", properties_get_type, properties_set_type,
                    properties_doc_type())
      .def_property("bus", properties_get_bus, properties_set_bus,
                    properties_doc_bus())
      .def_property("device", properties_get_device, properties_set_device,
                    properties_doc_device())
      .def_property("function", properties_get_function,
                    properties_set_function, properties_doc_function())
      .def_property("socket_id", properties_get_socket_id,
                    properties_set_socket_id, properties_doc_socket_id())
      .def_property("object_id", properties_get_object_id,
                    properties_set_object_id, properties_doc_object_id())
      .def_property("num_slots", properties_get_num_slots,
                    properties_set_num_slots, properties_doc_num_slots())
      .def_property("bbs_id", properties_get_bbs_id, properties_set_bbs_id,
                    properties_doc_bbs_id())
      .def_property("bbs_version", properties_get_bbs_version,
                    properties_set_bbs_id, properties_doc_bbs_id())
      .def_property("vendor_id", properties_get_vendor_id,
                    properties_set_vendor_id, properties_doc_vendor_id())
      .def_property("model", properties_get_model, properties_set_model,
                    properties_doc_model())
      .def_property("local_memory_size", properties_get_local_memory_size,
                    properties_set_local_memory_size,
                    properties_doc_local_memory_size())
      .def_property("capabilities", properties_get_capabilities,
                    properties_set_capabilities, properties_doc_capabilities())
      .def_property("num_mmio", properties_get_num_mmio,
                    properties_set_num_mmio, properties_doc_num_mmio())
      .def_property("num_interrupts", properties_get_num_interrupts,
                    properties_set_num_interrupts,
                    properties_doc_num_interrupts())
      .def_property("accelerator_state", properties_get_accelerator_state,
                    properties_set_accelerator_state,
                    properties_doc_accelerator_state());

  // define token class
  py::class_<token, token::ptr_t> pytoken(m, "token");
  pytoken.def_static("enumerate", &token::enumerate);
}
