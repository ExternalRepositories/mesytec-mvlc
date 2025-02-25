/* mesytec-mvlc - driver library for the Mesytec MVLC VME controller
 *
 * Copyright (c) 2020 mesytec GmbH & Co. KG
 *
 * Author: Florian Lüke <f.lueke@mesytec.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MESYTEC_MVLC_MVLC_BASIC_INTERFACE_H__
#define __MESYTEC_MVLC_MVLC_BASIC_INTERFACE_H__

#include <system_error>
#include "mvlc_constants.h"

namespace mesytec
{
namespace mvlc
{

class MVLCBasicInterface
{
    public:
        virtual ~MVLCBasicInterface() {}

        virtual std::error_code connect() = 0;
        virtual std::error_code disconnect() = 0;
        virtual bool isConnected() const = 0;

        virtual ConnectionType connectionType() const = 0; // Note: must be thread-safe
        virtual std::string connectionInfo() const = 0; // Note: must be thread-safe

        virtual std::error_code write(Pipe pipe, const u8 *buffer, size_t size,
                                      size_t &bytesTransferred) = 0;

        virtual std::error_code read(Pipe pipe, u8 *buffer, size_t size,
                                     size_t &bytesTransferred) = 0;

        // If enabled the implementation must try to disable all trigger
        // processing while (in the case of USB) reading and discarding all
        // buffered readout data.
        virtual void setDisableTriggersOnConnect(bool b) = 0;
        virtual bool disableTriggersOnConnect() const = 0;
};

}
}

#endif /* __MESYTEC_MVLC_MVLC_BASIC_INTERFACE_H__ */
