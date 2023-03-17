#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

template <typename... Targs>
void DUMMY_CODE(Targs&&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of
//! the interface \param[in] ip_address IP (what ARP calls "protocol") address
//! of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address,
                                   const Address& ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address "
         << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically
//! a router or default gateway, but may also be another host if directly
//! connected to the same network as the destination) (Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) with the
//! Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram& dgram,
                                     const Address& next_hop) {
    // convert an IP address that comes in the form of an Address object, into a
    // raw 32-bit integer
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // map from IP addresses to Ethernet addresses
    const auto& iter = _arp_table.find(next_hop_ip);
    // If the destination Ethernet address is unknown, broadcast an ARP request
    // for the next hop’s Ethernet address, and queue the IP datagram so it can
    // be sent after the ARP reply is received
    if (iter == _arp_table.end()) {
        // if there was no arp request corresponding to the ip
        if (_waiting_arp_requests.find(next_hop_ip) ==
            _waiting_arp_requests.end()) {
            // send a arp request
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.target_ip_address = next_hop_ip;
            arp_request.target_ethernet_address = {};
            // send a ethernet frame packaging the arp request
            EthernetFrame ethernet_frame;
            ethernet_frame.header().dst = ETHERNET_BROADCAST;
            ethernet_frame.header().src = _ethernet_address;
            ethernet_frame.header().type = EthernetHeader::TYPE_ARP;
            ethernet_frame.payload() = arp_request.serialize();
            _frames_out.push(ethernet_frame);

            _waiting_arp_requests[next_hop_ip].ttl = _arp_request_ttl;
        }
        // one ip may correspond to many datagrams
        _waiting_arp_requests[next_hop_ip].pending_datagrams.push_back(
            make_pair(dgram, next_hop));
    } else {
        // send a ethernet frame packaging the datagram
        EthernetFrame ethernet_frame;
        ethernet_frame.header().dst = iter->second.ethernet_address;
        ethernet_frame.header().src = _ethernet_address;
        ethernet_frame.header().type = EthernetHeader::TYPE_IPv4;
        ethernet_frame.payload() = dgram.serialize();
        _frames_out.push(ethernet_frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(
    const EthernetFrame& frame) {
    // The code should ignore any frames not destined for the network interface
    // (meaning, the Ethernet destination is either the broadcast address or the
    // interface’s own Ethernet address stored in the _ethernet_address member
    // variable)
    if (!(frame.header().dst == _ethernet_address ||
          frame.header().dst == ETHERNET_BROADCAST)) {
        return {};
    }
    // If the inbound frame is IPv4, parse the payload as an InternetDatagram
    // and, if successful (meaning the parse() method returned
    // ParseResult::NoError), return the resulting InternetDatagram to the
    // caller
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError) {
            return datagram;
        }
    }
    //  If the inbound frame is ARP
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;

        // parse the payload as an ARPMessage
        if (arp_message.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }
        //  A valid ARP message remember the mapping between the sender’s IP
        //  address and Ethernet address for 30 seconds. (Learn mappings from
        //  both requests and replies.)
        if (arp_message.target_ip_address == _ip_address.ipv4_numeric() &&
            (arp_message.opcode == ARPMessage::OPCODE_REQUEST ||
             (arp_message.opcode == ARPMessage::OPCODE_REPLY &&
              arp_message.target_ethernet_address == _ethernet_address))) {
            _arp_table[arp_message.sender_ip_address] = {
                arp_message.sender_ethernet_address, _arp_entry_ttl};
            // send datagrams waiting for the ethernet address
            const auto& iter = _waiting_arp_requests.find(
                arp_message.sender_ip_address);
            if (iter != _waiting_arp_requests.end()) {
                for (auto& each : iter->second.pending_datagrams) {
                    send_datagram(each.first, each.second);
                }
                _waiting_arp_requests.erase(
                    arp_message.sender_ip_address);
            }

            // In addition, if it’s an ARP request asking for our IP
            // address, send an appropriate ARP reply
            if (arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
                // send a arp reply
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.target_ip_address = arp_message.sender_ip_address;
                arp_reply.target_ethernet_address =
                    arp_message.sender_ethernet_address;
                // send a ethernet frame packaging the arp reply
                EthernetFrame ethernet_frame;
                ethernet_frame.header().dst =
                    arp_message.sender_ethernet_address;
                ethernet_frame.header().src = _ethernet_address;
                ethernet_frame.header().type = EthernetHeader::TYPE_ARP;
                ethernet_frame.payload() = arp_reply.serialize();
                _frames_out.push(ethernet_frame);
            }
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last
//! call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // check _arp_table
    for (auto iter = _arp_table.begin(); iter != _arp_table.end();) {
        // if the arp entry times out
        if (iter->second.ttl <= ms_since_last_tick)
            iter = _arp_table.erase(iter);
        else {
            iter->second.ttl -= ms_since_last_tick;
            ++iter;
        }
    }
    // check _waiting_arp_requests
    for (auto iter = _waiting_arp_requests.begin();
         iter != _waiting_arp_requests.end(); ++iter) {
        // if the arp request times out
        if (iter->second.ttl <= ms_since_last_tick) {
            // resend a arp request
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = iter->first;
            // resend a ethernet frame packaging the arp request
            EthernetFrame ethernet_frame;
            ethernet_frame.header().dst = ETHERNET_BROADCAST;
            ethernet_frame.header().src = _ethernet_address;
            ethernet_frame.header().type = EthernetHeader::TYPE_ARP;
            ethernet_frame.payload() = arp_request.serialize();
            _frames_out.push(ethernet_frame);

            iter->second.ttl = _arp_request_ttl;
        } else {
            iter->second.ttl -= ms_since_last_tick;
        }
    }
}