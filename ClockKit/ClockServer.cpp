#include "ClockKit/ClockServer.h"

#include <cmath>

#include "ClockKit/ClockPacket.h"

using namespace std;
using namespace std::chrono;

namespace dex {

ClockServer::ClockServer(const kissnet::endpoint& addr_port, Clock& clock)
    : addr_port_(addr_port)
    , clock_(clock)
    , ackData_{map<string, Entry>()}
    , log_(false)
    , tRecalculated_(clock_.getValue())
{
    kissnet_init();
}

void ClockServer::run()
{
    kissnet::udp_socket socket(addr_port_);
    socket.bind();
    constexpr auto length = ClockPacket::PACKET_LENGTH;
    ClockPacket::packetbuf buffer;
    if (log_)
        cout << "time                     host    \toffset\tround-trip-time" << endl;

    for (;;) {
        kissnet::addr_collection peer;
        const auto [num_bytes, status] = socket.recv(buffer, 0, &peer);
        const auto now = clock_.getValue();  // Before anything else.
        if (now == tpInvalid) {
            // Very unlikely, for std::chrono::system_clock::now().
            cerr << "ClockServer's clock is corrupt.\n";
            continue;
        }
        if (status != kissnet::socket_status::valid) {
            cerr << "got no packet: status " << status << "\n";
            continue;
        }
        if (num_bytes != length) {
            cerr << "ignored packet with wrong length " << num_bytes << ".\n";
            continue;
        }
        const auto peer2 = socket.get_recv_endpoint();  // Same as peer.
        ClockPacket packet(buffer);
        // cout << "got "; packet.print();
        switch (packet.getType()) {
            case ClockPacket::REQUEST:
                packet.setType(ClockPacket::REPLY);
                packet.setServerReplyTime(now);
                // cout << "srv replying: "; packet.print();
                packet.write(buffer);
                {
                    const auto [num_bytes, status] = socket.send(buffer, length, &peer);
                    if (status != kissnet::socket_status::valid)
                        cerr << "ClockServer had problems sending a packet; status " << status << "\n";
                    if (num_bytes != length)
                        cerr << "ClockServer sent incomplete packet.\n";
                    // cout << "sent to " << peer2.address << ":" << peer2.port << "\n";
                    break;
                }
            case ClockPacket::ACKNOWLEDGE:
                updateEntry(peer2.address, packet.getClockOffset(), packet.rtt(), now);
                break;
            case ClockPacket::KILL:
                return;
            default:
                cerr << "ClockServer ignored packet with unexpected type " << packet.getTypeName() << ".\n";
        }
    }
}

// Don't shrink this to (addr, packet, now), because
// that would more tightly couple ClockServer.h to ClockPacket.h.
void ClockServer::updateEntry(const string& addr, dur offset, dur rtt, tp now)
{
    if (!log_)
        return;

    // This output (from ./ckserver) is _only informational_.  What really matters are the timestamps that
    // clients (./ckphaselock, python/ckphaselock.py, etc.) get by calling `ckTimeAsString` or `ckTimeAsValue`.
    //
    // To get synced timestamps on two (or more) hosts, run a client on each host, and run the server
    // somewhere (possibly on a host that's already running a client).  Get the timestamps from the clients,
    // not directly from the host.
    //
    // Anyhow, this output is:
    // - the time "now", in seconds + microseconds since the Unix epoch;
    // - either a client's IP address followed by the client's clock's offset in microseconds,
    // or "offsetMax" followed by the largest offset of all currently connected clients (updated every few seconds).

    const auto nowStr = StringFromTp(now);
    cout << nowStr << ' ' << addr << '\t' << UsecFromDur(offset) << '\t' << UsecFromDur(rtt) << endl;
    ackData_[addr] = Entry(now, offset, rtt);

    if (now < tRecalculated_ + 500ms)
        return;
    tRecalculated_ = now;

    // Purge entries older than 1.5 s.
    const auto tPurge = now - 1500ms;
    std::erase_if(ackData_, [tPurge](const auto& a) {
        auto const& [str, entry] = a;
        return entry.time < tPurge;
    });

    // Recalculate the maximum offset.
    int64_t offsetMax = 0;
    for (const auto& data : ackData_) {
        const auto& entry = data.second;
        if (entry.offset == durInvalid || entry.rtt == durInvalid) {
            offsetMax = usecInvalid;
            break;
        }
        // RHS isn't invalid.
        offsetMax = max(offsetMax, abs(UsecFromDur(entry.offset)) + UsecFromDur(entry.rtt) / 2);
    }
    cout << nowStr << ' ' << "offsetMax" << '\t' << offsetMax << '\t' << "---" << endl;
}

}  // namespace dex
