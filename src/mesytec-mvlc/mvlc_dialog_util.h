/* mvme - Mesytec VME Data Acquisition
 *
 * Copyright (C) 2016-2020 mesytec GmbH & Co. KG <info@mesytec.com>
 *
 * Author: Florian Lüke <f.lueke@mesytec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __MESYTEC_MVLC_MVLC_DIALOG_UTIL_H__
#define __MESYTEC_MVLC_MVLC_DIALOG_UTIL_H__

#include <array>
#include <iostream>
#include <iomanip>

#include "mvlc_command_builders.h"
#include "mvlc_constants.h"
#include "mvlc_error.h"
#include "mvlc_util.h"

namespace mesytec
{
namespace mvlc
{

struct StackInfo
{
    u32 triggers;
    u32 offset;
    u16 startAddress;
    std::vector<u32> contents;
};

struct StackTrigger
{
    stacks::TriggerType triggerType;
    u8 irqLevel = 0;
};

template<typename DIALOG_API>
std::pair<std::vector<u32>, std::error_code>
read_stack_contents(DIALOG_API &mvlc, u16 startAddress)
{
    using namespace stack_commands;

    u32 stackHeader = 0u;

    if (auto ec = mvlc.readRegister(startAddress, stackHeader))
        return std::make_pair(std::vector<u32>{}, ec);

    std::vector<u32> contents;
    contents.reserve(64);
    contents.push_back(stackHeader);

    u8 headerType = (stackHeader >> CmdShift) & CmdMask; // 0xF3

    if (headerType != static_cast<u8>(StackCommandType::StackStart))
        return { contents, make_error_code(MVLCErrorCode::InvalidStackHeader) };

    u32 addr  = startAddress + AddressIncrement;
    u32 value = 0u;

    do
    {
        if (addr >= stacks::StackMemoryEnd)
            return { contents, make_error_code(MVLCErrorCode::StackMemoryExceeded) };

        if (auto ec = mvlc.readRegister(addr, value))
            return { contents, ec };

        contents.push_back(value);
        addr += AddressIncrement;

    } while (((value >> CmdShift) & CmdMask) != static_cast<u8>(StackCommandType::StackEnd)); // 0xF4

    return { contents, {} };
}

template<typename DIALOG_API>
std::pair<StackInfo, std::error_code>
read_stack_info(DIALOG_API &mvlc, u8 id)
{
    StackInfo result = {};

    if (id >= stacks::StackCount)
        return { result, make_error_code(MVLCErrorCode::StackCountExceeded) };

    if (auto ec = mvlc.readRegister(stacks::get_trigger_register(id), result.triggers))
        return { result, ec };

    if (auto ec = mvlc.readRegister(stacks::get_offset_register(id), result.offset))
        return { result, ec };

    result.startAddress = stacks::StackMemoryBegin + result.offset;

    auto sc = read_stack_contents(mvlc, result.startAddress);

    result.contents = sc.first;

    return { result, sc.second };
}

template<typename DIALOG_API>
std::error_code enable_daq_mode(DIALOG_API &mvlc)
{
    return mvlc.writeRegister(DAQModeEnableRegister, 1);
}

template<typename DIALOG_API>
std::error_code disable_daq_mode(DIALOG_API &mvlc)
{
    return mvlc.writeRegister(DAQModeEnableRegister, 0);
}

template<typename DIALOG_API>
std::error_code read_daq_mode(DIALOG_API &mvlc, u32 &daqMode)
{
    return mvlc.readRegister(DAQModeEnableRegister, daqMode);
}

template<typename DIALOG_API>
std::error_code disable_all_triggers_and_daq_mode(DIALOG_API &mvlc)
{
    SuperCommandBuilder sb;
    sb.addReferenceWord(0x1338);

    sb.addWriteLocal(DAQModeEnableRegister, 0);

    for (u8 stackId = 0; stackId < stacks::StackCount; stackId++)
    {
        u16 addr = stacks::get_trigger_register(stackId);
        sb.addWriteLocal(addr, stacks::NoTrigger);
    }

    std::vector<u32> responseBuffer;
    return mvlc.superTransaction(sb, responseBuffer);
}

template<typename DIALOG_API>
std::error_code reset_stack_offsets(DIALOG_API &mvlc)
{
    for (u8 stackId = 0; stackId < stacks::StackCount; stackId++)
    {
        u16 addr = stacks::get_offset_register(stackId);

        if (auto ec = mvlc.writeRegister(addr, 0))
            return ec;
    }

    return {};
}

// Builds, uploads and sets up the readout stack for each event in the vme
// config.
template<typename DIALOG_API>
std::error_code setup_readout_stacks(
    DIALOG_API &mvlc,
    const std::vector<StackCommandBuilder> &readoutStacks)
{
    // Stack0 is reserved for immediate exec
    u8 stackId = stacks::ImmediateStackID + 1;

    // 1 word gap between immediate stack and first readout stack
    u16 uploadWordOffset = stacks::ImmediateStackStartOffsetWords + stacks::ImmediateStackReservedWords + 1;

    for (const auto &stackBuilder: readoutStacks)
    {
        if (stackId >= stacks::StackCount)
            return make_error_code(MVLCErrorCode::StackCountExceeded);

        // need to convert to a buffer to determine the size
        auto stackBuffer = make_stack_buffer(stackBuilder);

        u16 uploadAddress = uploadWordOffset * AddressIncrement;
        u16 endAddress    = uploadAddress + stackBuffer.size() * AddressIncrement;

        if (endAddress >= stacks::StackMemoryEnd)
            return make_error_code(MVLCErrorCode::StackMemoryExceeded);

        u8 stackOutputPipe = stackBuilder.suppressPipeOutput() ? SuppressPipeOutput : DataPipe;

        if (auto ec = mvlc.uploadStack(stackOutputPipe, uploadAddress, stackBuilder))
            return ec;

        u16 offsetRegister = stacks::get_offset_register(stackId);

        if (auto ec = mvlc.writeRegister(offsetRegister, uploadAddress & stacks::StackOffsetBitMaskBytes))
            return ec;

        stackId++;
        // again leave a 1 word gap between stacks
        uploadWordOffset += stackBuffer.size() + 1;
    }

    return {};
}

template<typename DIALOG_API>
std::error_code write_stack_trigger_value(
    DIALOG_API &mvlc, u8 stackId, u32 triggerVal)
{
    u16 triggerReg = stacks::get_trigger_register(stackId);
    return mvlc.writeRegister(triggerReg, triggerVal);
}

inline u32 trigger_value(const StackTrigger &st)
{
    return trigger_value(st.triggerType, st.irqLevel); // in mvlc_util.h
}

template<typename DIALOG_API>
std::error_code setup_stack_trigger(
    DIALOG_API &mvlc, u8 stackId, const StackTrigger &st)
{
    u32 triggerVal = trigger_value(st);
    return write_stack_trigger_value(mvlc, stackId, triggerVal);
}

// Writes the stack trigger values using a single stack transaction.
template<typename DIALOG_API>
std::error_code setup_readout_triggers(
    DIALOG_API &mvlc,
    const std::array<u32, stacks::ReadoutStackCount> &triggerValues)
{
    SuperCommandBuilder sb;
    sb.addReferenceWord(0x1337);
    u8 stackId = stacks::ImmediateStackID + 1;

    for (u32 triggerVal: triggerValues)
    {
        //std::cerr << "stackId=" << static_cast<unsigned>(stackId)
        //    << ", triggerVal=0x" << std::hex << triggerVal << std::dec
        //    << std::endl;

        u16 triggerReg = stacks::get_trigger_register(stackId);
        sb.addWriteLocal(triggerReg, triggerVal);
        ++stackId;
    }

    std::vector<u32> responseBuffer;

    return mvlc.superTransaction(sb, responseBuffer);
}

template<typename DIALOG_API>
std::error_code setup_readout_triggers(
    DIALOG_API &mvlc,
    const std::array<StackTrigger, stacks::ReadoutStackCount> &triggers)
{
    std::array<u32, stacks::ReadoutStackCount> triggerValues;

    std::transform(std::begin(triggers), std::end(triggers), std::begin(triggerValues),
                   [] (const StackTrigger &st) {
                       return trigger_value(st);
                   });

    return setup_readout_triggers(mvlc, triggerValues);
}

} // end namespace mvlc
} // end namespace mesytec

#endif /* __MESYTEC_MVLC_MVLC_DIALOG_UTIL_H__ */
