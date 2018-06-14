# TCL File Generated by Component Editor 17.1.1
# Wed Feb 28 16:44:43 CST 2018
# DO NOT MODIFY


# 
# Write_Response_Bridge "Write_Response_Bridge" v1.0
# JCJB 2018.02.28.16:44:43
# A write only bridge that pipelines all the interface signals and forwards write response information from the master port to the slave port.
# 

# 
# request TCL package from ACDS 17.1
# 
package require -exact qsys 17.1


# 
# module Write_Response_Bridge
# 
set_module_property DESCRIPTION "A write only bridge that pipelines all the interface signals and forwards write response information from the master port to the slave port."
set_module_property NAME write_response_bridge
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR JCJB
set_module_property DISPLAY_NAME "Write Response Bridge"
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE false
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false


# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL write_response_bridge
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file write_response_bridge.v VERILOG PATH write_response_bridge.v TOP_LEVEL_FILE

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL write_response_bridge
set_fileset_property SIM_VERILOG ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property SIM_VERILOG ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file write_response_bridge.v VERILOG PATH write_response_bridge.v

add_fileset SIM_VHDL SIM_VHDL "" ""
set_fileset_property SIM_VHDL TOP_LEVEL write_response_bridge
set_fileset_property SIM_VHDL ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property SIM_VHDL ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file write_response_bridge.v VERILOG PATH write_response_bridge.v


set_module_property VALIDATION_CALLBACK     validate_me


# 
# parameters
# 
add_parameter ADDRESS_WIDTH INTEGER 48
set_parameter_property ADDRESS_WIDTH ALLOWED_RANGES 1:64
set_parameter_property ADDRESS_WIDTH DISPLAY_NAME "Address Width"
set_parameter_property ADDRESS_WIDTH UNITS None
set_parameter_property ADDRESS_WIDTH HDL_PARAMETER true

add_parameter DATA_WIDTH INTEGER 512
set_parameter_property DATA_WIDTH ALLOWED_RANGES {8 16 32 64 128 256 512 1024}
set_parameter_property DATA_WIDTH DISPLAY_NAME "Date Width"
set_parameter_property DATA_WIDTH UNITS None
set_parameter_property DATA_WIDTH HDL_PARAMETER true

# GUI only parameter
add_parameter MAX_BURST_COUNT INTEGER 4 "Maximum burst count"
set_parameter_property MAX_BURST_COUNT ALLOWED_RANGES {1 2 4 8 16 32 64 128 256 512 1024}
set_parameter_property MAX_BURST_COUNT DISPLAY_NAME "Maximum Burst Count"
set_parameter_property MAX_BURST_COUNT AFFECTS_GENERATION false
set_parameter_property MAX_BURST_COUNT HDL_PARAMETER false
set_parameter_property MAX_BURST_COUNT DERIVED false
set_parameter_property MAX_BURST_COUNT AFFECTS_ELABORATION false

# GUI only parameter
add_parameter MAX_PENDING_WRITES INTEGER 4 "Maximum pending write transactions"
set_parameter_property MAX_PENDING_WRITES ALLOWED_RANGES 1:128
set_parameter_property MAX_PENDING_WRITES DISPLAY_NAME "Maximum Pending Write Transactions"
set_parameter_property MAX_PENDING_WRITES AFFECTS_GENERATION false
set_parameter_property MAX_PENDING_WRITES HDL_PARAMETER false
set_parameter_property MAX_PENDING_WRITES DERIVED false
set_parameter_property MAX_PENDING_WRITES AFFECTS_ELABORATION false


# BURST_WIDTH = 1 + log2(MAX_BURST_COUNT)
add_parameter BURST_WIDTH INTEGER 3
set_parameter_property BURST_WIDTH DISPLAY_NAME BURST_WIDTH
set_parameter_property BURST_WIDTH UNITS None
set_parameter_property BURST_WIDTH HDL_PARAMETER true
set_parameter_property BURST_WIDTH VISIBLE false
set_parameter_property BURST_WIDTH DERIVED true

# MAX_PENDING_WRITES_WIDTH = 1 + log2(MAX_PENDING_WRITES)
add_parameter MAX_PENDING_WRITES_WIDTH INTEGER 6
set_parameter_property MAX_PENDING_WRITES_WIDTH DISPLAY_NAME MAX_PENDING_WRITES_WIDTH
set_parameter_property MAX_PENDING_WRITES_WIDTH UNITS None
set_parameter_property MAX_PENDING_WRITES_WIDTH HDL_PARAMETER true
set_parameter_property MAX_PENDING_WRITES_WIDTH VISIBLE false
set_parameter_property MAX_PENDING_WRITES_WIDTH DERIVED true

# 
# display items
# 


# 
# connection point clock
# 
add_interface clock clock end
set_interface_property clock clockRate 0
set_interface_property clock ENABLED true
set_interface_property clock EXPORT_OF ""
set_interface_property clock PORT_NAME_MAP ""
set_interface_property clock CMSIS_SVD_VARIABLES ""
set_interface_property clock SVD_ADDRESS_GROUP ""

add_interface_port clock clk clk Input 1


# 
# connection point reset
# 
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges BOTH
set_interface_property reset ENABLED true
set_interface_property reset EXPORT_OF ""
set_interface_property reset PORT_NAME_MAP ""
set_interface_property reset CMSIS_SVD_VARIABLES ""
set_interface_property reset SVD_ADDRESS_GROUP ""

add_interface_port reset reset reset Input 1


# 
# connection point avmm_slave
# 
add_interface avmm_slave avalon end
set_interface_property avmm_slave addressGroup 0
set_interface_property avmm_slave addressUnits SYMBOLS
set_interface_property avmm_slave associatedClock clock
set_interface_property avmm_slave associatedReset reset
set_interface_property avmm_slave bitsPerSymbol 8
set_interface_property avmm_slave bridgedAddressOffset ""
set_interface_property avmm_slave bridgesToMaster "avmm_master"
set_interface_property avmm_slave burstOnBurstBoundariesOnly false
set_interface_property avmm_slave burstcountUnits WORDS
set_interface_property avmm_slave explicitAddressSpan 0
set_interface_property avmm_slave holdTime 0
set_interface_property avmm_slave linewrapBursts false
set_interface_property avmm_slave maximumPendingReadTransactions 0
set_interface_property avmm_slave maximumPendingWriteTransactions 64
set_interface_property avmm_slave minimumResponseLatency 1
set_interface_property avmm_slave readLatency 0
set_interface_property avmm_slave readWaitTime 1
set_interface_property avmm_slave setupTime 0
set_interface_property avmm_slave timingUnits Cycles
set_interface_property avmm_slave transparentBridge false
set_interface_property avmm_slave waitrequestAllowance 0
set_interface_property avmm_slave writeWaitTime 0
set_interface_property avmm_slave ENABLED true
set_interface_property avmm_slave EXPORT_OF ""
set_interface_property avmm_slave PORT_NAME_MAP ""
set_interface_property avmm_slave CMSIS_SVD_VARIABLES ""
set_interface_property avmm_slave SVD_ADDRESS_GROUP ""

add_interface_port avmm_slave s_address address Input "((ADDRESS_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_slave s_writedata writedata Input "((DATA_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_slave s_write write Input 1
add_interface_port avmm_slave s_byteenable byteenable Input "(((DATA_WIDTH / 8) - 1)) - (0) + 1"
add_interface_port avmm_slave s_response response Output 2
add_interface_port avmm_slave s_waitrequest waitrequest Output 1
add_interface_port avmm_slave s_burst burstcount Input "((BURST_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_slave s_write_response_valid writeresponsevalid Output 1
set_interface_assignment avmm_slave embeddedsw.configuration.isFlash 0
set_interface_assignment avmm_slave embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment avmm_slave embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment avmm_slave embeddedsw.configuration.isPrintableDevice 0



# 
# connection point avmm_master
# 
add_interface avmm_master avalon start
set_interface_property avmm_master addressGroup 0
set_interface_property avmm_master addressUnits SYMBOLS
set_interface_property avmm_master associatedClock clock
set_interface_property avmm_master associatedReset reset
set_interface_property avmm_master bitsPerSymbol 8
set_interface_property avmm_master burstOnBurstBoundariesOnly false
set_interface_property avmm_master burstcountUnits WORDS
set_interface_property avmm_master doStreamReads false
set_interface_property avmm_master doStreamWrites false
set_interface_property avmm_master holdTime 0
set_interface_property avmm_master linewrapBursts false
set_interface_property avmm_master maximumPendingReadTransactions 0
set_interface_property avmm_master maximumPendingWriteTransactions 0
set_interface_property avmm_master minimumResponseLatency 1
set_interface_property avmm_master readLatency 0
set_interface_property avmm_master readWaitTime 0
set_interface_property avmm_master setupTime 0
set_interface_property avmm_master timingUnits Cycles
set_interface_property avmm_master waitrequestAllowance 0
set_interface_property avmm_master writeWaitTime 0
set_interface_property avmm_master ENABLED true
set_interface_property avmm_master EXPORT_OF ""
set_interface_property avmm_master PORT_NAME_MAP ""
set_interface_property avmm_master CMSIS_SVD_VARIABLES ""
set_interface_property avmm_master SVD_ADDRESS_GROUP ""

add_interface_port avmm_master m_address address Output "((ADDRESS_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_master m_writedata writedata Output "((DATA_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_master m_write write Output 1
add_interface_port avmm_master m_byteenable byteenable Output "(((DATA_WIDTH / 8) - 1)) - (0) + 1"
add_interface_port avmm_master m_burst burstcount Output "((BURST_WIDTH - 1)) - (0) + 1"
add_interface_port avmm_master m_response response Input 2
add_interface_port avmm_master m_waitrequest waitrequest Input 1
add_interface_port avmm_master m_write_response_valid writeresponsevalid Input 1



proc validate_me {}  {
  set_parameter_value BURST_WIDTH [expr {1 + (log([get_parameter_value MAX_BURST_COUNT]) / log(2))}]
  set_parameter_value MAX_PENDING_WRITES_WIDTH [expr {1 + (log([get_parameter_value MAX_PENDING_WRITES]) / log(2))}]
}