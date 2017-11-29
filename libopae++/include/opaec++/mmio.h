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
#pragma once
#include <memory>
#include "handle.h"

namespace opae
{
namespace fpga
{
namespace io
{

class mmio
{
public:
    /** The types of mmio region that can be requested.
     */
    enum class region_t : int32_t
    {
        AFU, ///< Get the Accelerator Function Unit region.
        STP  ///< Get the Signal Tap region.
    };

    /** The types of MMIO implementation that can be requested.
     */
    enum class impl_t : int32_t
    {
        API,   ///< Use the OPAE API implementation.
        Direct ///< Use the Direct implementation.
    };

    /**
     * mmio smart pointer type.
     */
    typedef std::shared_ptr<mmio> ptr_t;

    /** Factory method used to obtain an mmio_region.
     * @return A smart pointer containing the requested region,
     * or an empty smart pointer if the region was not obtained.
     */
    static ptr_t map(opae::fpga::types::handle::ptr_t h, 
                     region_t region = region_t::AFU,
                     impl_t implementation = impl_t::API);
    /**
     * mmio destructor.
     */
    virtual ~mmio() {}

    /** Write 32-bit value to MMIO.
     * @param[in] offset The byte offset to the location to be written.
     * @param[in] value  The value to write.
     * @return Whether the write was successful.
     */
    virtual bool write_mmio32(uint32_t offset, uint32_t value) = 0;

    /** Write 64-bit value to MMIO.
     * @param[in] offset The byte offset to the location to be written.
     * @param[in] value  The value to write.
     * @return Whether the write was successful.
     */
    virtual bool write_mmio64(uint32_t offset, uint64_t value) = 0;

    /** Read 32-bit value from MMIO.
     * @param[in] offset The byte offset to the location to be written.
     * @param[out] value Contains the read value on return.
     * @return Whether the read was successful.
     */
    virtual bool read_mmio32(uint32_t offset, uint32_t & value) const = 0;

    /** Read 64-bit value from MMIO.
     * @param[in] offset The byte offset to the location to be written.
     * @param[out] value Contains the read value on return.
     * @return Whether the read was successful.
     */
    virtual bool read_mmio64(uint32_t offset, uint64_t & value) const = 0;

    /** Retrieve a pointer to the MMIO, given an offset.
     * @param[in] offset The byte offset to be added to the MMIO base.
     */
    virtual volatile uint8_t * mmio_pointer(uint32_t offset) const = 0;

    /** Retrieve the region type.
     */
    region_t region() const { return region_; }

    /** Retrieve the underlying region implementation type.
     */
    impl_t implementation() const { return implementation_; }

protected:
    region_t region_;
    impl_t  implementation_;

    mmio(region_t region, impl_t implementation);
};

} // end of namespace io
} // end of namespace fpga
} // end of namespace opae
