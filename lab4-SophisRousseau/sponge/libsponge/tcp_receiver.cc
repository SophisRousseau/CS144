#include "tcp_receiver.hh"
using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    if (!_syn) {
         if (!header.syn) {
             return;
         }
        _syn = true;
        _isn = header.seqno; // 得到 _isn，即约定的 initial sequence number
    }

    _fin |= header.fin;

    _ackno = stream_out().bytes_written() + 1;
    uint64_t abs_seqno = unwrap(header.seqno, _isn, _ackno);
    uint64_t index = abs_seqno - 1 + header.syn;
    _reassembler.push_substring(seg.payload().copy(), index, header.fin);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!_syn)return {};
    if(_fin && !unassembled_bytes())
        return wrap(stream_out().bytes_written() + 2, _isn);
    return wrap(stream_out().bytes_written() + 1, _isn);
}

size_t TCPReceiver::window_size() const { 
    return _capacity - stream_out().buffer_size();
}