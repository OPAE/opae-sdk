#include <iostream>
#include <string>
#include <CLI/CLI.hpp>
#include <opae/cxx/core/handle.h>
#include <opae/cxx/core/token.h>
#include <opae/cxx/core/properties.h>
#include "hssi_mbox.h"

using namespace opae::fpga::types;

#define AFU_ID "823c334c-98bf-11ea-bb37-0242ac130002"

#define DEFAULT_PACKETS 1
#define DEFAULT_PACKET_LENGTH 64

#define CSR_NUM_PACKETS   0x3c00
#define CSR_PACKET_LENGTH 0x3c0d

#define MBOX_TIMEOUT 1000

typedef int (*test_fn)(opae::fpga::types::handle::ptr_t h, CLI::App *app);

handle::ptr_t open_accelerator(const char *guid, const char *bdf)
{
  handle::ptr_t accel;

  auto filter = properties::get();
  filter->type = FPGA_ACCELERATOR;
  filter->guid.parse(guid);

  //TODO
  if (bdf) { ; }

  auto tokens = token::enumerate({filter});
  if (tokens.size() < 1) {
    std::cerr << "Accelerator not found." << std::endl;
    return accel;
  }

  if (tokens.size() > 1) {
    std::cerr << "warning: More than one accelerator found." << std::endl;
  }

  return handle::open(tokens[0], 0);
}

#define INVALID_MAC 0xffffffffffffffffULL
uint64_t mac_string_to_bits(const std::string &str)
{
  //           1111111
  // 01234567890123456
  // xx:xx:xx:xx:xx:xx

  if (str.length() < 17 ||
      str[2] != ':' ||
      str[5] != ':' ||
      str[8] != ':' ||
      str[11] != ':' ||
      str[14] != ':') {
    return INVALID_MAC;
  }

  unsigned shift = 0;
  unsigned i;
  uint64_t res = 0;
  for (i = 0 ; i < 17 ; i += 3, shift += 8) {
    std::string sub = str.substr(i, 2);
    unsigned long bits;
    try {
      bits = std::stoul(sub, nullptr, 16);
    } catch (std::invalid_argument &e) {
      std::cerr << "error: " << e.what() << std::endl;
      return INVALID_MAC;
    }
    res |= bits << shift;
  }

  return res;
}

int run_scratchpad(opae::fpga::types::handle::ptr_t h, CLI::App *app)
{
  (void)app;
  std::cout << "scratchpad: 0x" << std::hex <<
    h->read_csr64(0x48) << std::endl;

  h->write_csr64(0x48, 0xc0cac01a);
  std::cout << "scratchpad: 0x" << std::hex <<
    h->read_csr64(0x48) << std::endl;

  return 0;
}

int run_external_lpbk(opae::fpga::types::handle::ptr_t h, CLI::App *app)
{
  std::cout << "external lpbk test" << std::endl;

  auto num_packets_opt = app->get_option("--num-packets");
  unsigned num_packets = num_packets_opt->as<unsigned>();
  std::cout << "  num_packets: " << num_packets << std::endl;

  auto packet_length_opt = app->get_option("--packet-length");
  unsigned packet_length = packet_length_opt->as<unsigned>();
  std::cout << "  packet_length: " << packet_length << std::endl;

  auto src_addr_opt = app->get_option("--src-addr");
  std::string src_addr = src_addr_opt->as<std::string>();
  std::cout << "  src address: " << src_addr << std::endl;
  uint64_t bin_src_addr = mac_string_to_bits(src_addr);
  std::cout << "   (bits): 0x" << std::hex << bin_src_addr << std::endl;

  if (bin_src_addr == INVALID_MAC) {
    std::cerr << "invalid MAC address: " << src_addr << std::endl;
    return 1;
  }

  auto dest_addr_opt = app->get_option("--dest-addr");
  std::string dest_addr = dest_addr_opt->as<std::string>();
  std::cout << "  dest address: " << dest_addr << std::endl;
  uint64_t bin_dest_addr = mac_string_to_bits(dest_addr);
  std::cout << "   (bits): 0x" << std::hex << bin_dest_addr << std::endl;
  
  if (bin_dest_addr == INVALID_MAC) {
    std::cerr << "invalid MAC address: " << dest_addr << std::endl;
    return 1;
  }

  uint8_t *mmio_base = h->mmio_ptr(0);

  // TODO
  // 1. External Loopback Test: In this test, traffic will be generated by AFU and loopback will be done with QSFP loopback connector. 
  //
  // a. Clear MAC IP statistics registers (May be some OPAE API to do it, as these are in FIM space)
  //
  // b. Program number_of_packets  register of Traffic generator CSR space with number of packets to be transmitted
 
  //mbox_write(uint8_t *mmio_base, uint16_t csr, uint32_t data, uint64_t max_ticks) 

  mbox_write(mmio_base, CSR_NUM_PACKETS, num_packets, MBOX_TIMEOUT);

  // c. Program pkt_length register of Traffic generator CSR space with length of each packet to be transmitted

  mbox_write(mmio_base, CSR_PACKET_LENGTH, packet_length, MBOX_TIMEOUT);

  // d. Program source_addr0 and source_addr1 registers of Traffic generator CSR space with Source MAC address

  // e. Program destination_addr0 and destination_addr1 registers of Traffic generator CSR space with Destination MAC address

  // f. Write 1 to start register of Traffic generator CSR space
  //
  // g. Print MAC IP statistics registers (May be some OPAE API to do it, as these are in FIM space)


  return 0;
}

int run_afu_lpbk(opae::fpga::types::handle::ptr_t h, CLI::App *app)
{
  (void)h;
  (void)app;




  return 0;
}

int main(int argc, char *argv[])
{
  CLI::App app("hssi");
  std::string guid, bdf;
  auto guid_opt = app.add_option("-g,--guid", guid, "GUID");
  auto bdf_opt = app.add_option("-b,--bdf", bdf, "<bus>:<device>.<function>");

  std::map<CLI::App *, test_fn> tests;

  auto scratch_cmd = app.add_subcommand("scratch", "run scratchpad test");
  tests[scratch_cmd] = run_scratchpad;

  auto external_cmd = app.add_subcommand("external", "run external lpbk test");
  external_cmd->add_option("--num-packets", "number of packets")->default_val(DEFAULT_PACKETS);
  external_cmd->add_option("--packet-length", "packet length")->default_val(DEFAULT_PACKET_LENGTH);
  external_cmd->add_option("--src-addr", "source MAC address");
  external_cmd->add_option("--dest-addr", "destination MAC address");
  tests[external_cmd] = run_external_lpbk;


  auto afu_cmd = app.add_subcommand("afu", "run afu lpbk test");
  tests[afu_cmd] = run_afu_lpbk;

  CLI11_PARSE(app, argc, argv);

  auto afu_id = *guid_opt ? guid.c_str() : AFU_ID;
  auto addr = *bdf_opt ? bdf.c_str() : nullptr;
  auto h = open_accelerator(afu_id, addr);

  for (auto kv : tests) {
    if (*kv.first)
      return kv.second(h, kv.first);
  }

  return 1;
}
