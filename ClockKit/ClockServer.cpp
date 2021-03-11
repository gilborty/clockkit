#include "ClockServer.h"

#include <cmath>
#include <iostream>

#include "ClockPacket.h"

using namespace std;

namespace dex {

const timestamp_t ClockServer::SYSTEM_STATE_PURGE_TIME = 1500000;  // usec

ClockServer::ClockServer(ost::InetAddress addr, ost::tpport_t port, Clock& clock)
    : addr_(addr)
    , port_(port)
    , clock_(clock)
    , ackData_{std::map<std::string, Entry>()}
    , log_(false)
    , tRecalculated_(clock_.getValue())
{
}

void ClockServer::run()
{
    ost::UDPSocket socket(addr_, port_);
    if (log_)
        cout << "time\thost\toffset\trtt" << endl;
    constexpr auto length = ClockPacket::PACKET_LENGTH;
    uint8_t buffer[length];

    while (socket.isPending(ost::Socket::pendingInput, TIMEOUT_INF)) {
        const timestamp_t serverReplyTime = clock_.getValue();
        const ost::InetAddress peer =
            socket.getPeer();  // also sets up the socket to send back to the sender
        if (socket.receive(buffer, length) != length) {
            cerr << "ERR: packet had wrong length.\n";
        }
        else {
            ClockPacket packet(buffer);
            switch (packet.getType()) {
                case ClockPacket::REQUEST:
                    packet.setServerReplyTime(serverReplyTime);
                    packet.setType(ClockPacket::REPLY);
                    packet.write(buffer);
                    if (socket.send(buffer, length) != length)
                        cerr << "ERR: sent incomplete packet.\n";
                    break;
                case ClockPacket::ACKNOWLEDGE:
                    updateEntry(peer.getHostname(), packet.getClockOffset(), packet.rtt());
                    break;
                case ClockPacket::KILL:
                    return;  // Exit this thread.
                default:
                    cerr << "ERR: packet had wrong type.\n";
            }
        }
    }
}

void ClockServer::updateEntry(string addr, int offset, int rtt)
{
    const auto now = clock_.getValue();
    ackData_[addr] = {now, offset, rtt};
    if (!log_)
        return;

    const auto nowStr = Timestamp::timestampToString(now);
    cout << nowStr << '\t' << addr << '\t' << offset << '\t' << rtt << endl;
    // 1.0 seconds sets only how often to recalculate MAX_OFFSET.
    if (now < tRecalculated_ + 1000000)
        return;
    tRecalculated_ = now;

    // Purge old entries.
    // Don't use erase+remove_if+lambda, because that fails with map;
    // wait for C++20's std::erase_if.
    const auto tPurge = now - SYSTEM_STATE_PURGE_TIME;
    for (auto it = ackData_.begin(); it != ackData_.end();) {
        if (it->second.time < tPurge) {
            it = ackData_.erase(it);
        }
        else {
            ++it;
        }
    }

    // Calculate maximum offset.
    auto offsetMax = 0;
    for (const auto& data : ackData_) {
        const auto& entry = data.second;
        offsetMax = max(offsetMax, abs(entry.offset) + entry.rtt / 2);
    }
    cout << nowStr << '\t' << "MAX_OFFSET" << '\t' << offsetMax << '\t' << "---" << endl;
}

}  // namespace dex
