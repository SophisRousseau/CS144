#include "tcp_connection.hh"

#include <iostream>
#include <limits>

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment& seg) {
    // Receiving segments

    const TCPHeader& header = seg.header();
    // <1> if rst flag is set
    if (header.rst) {
        shutdown(false);  // unclean shutdown
        return;
    }

    // <2> give the segment to the receiver (seqno, syn , payload, and fin)
    _receiver.segment_received(seg);

    // <3> if ack flag is set (ackno and window size)
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }

    // <4> if the incoming segment occupied any sequence numbers, the
    // TCPConnection makes sure that at least one segment is sent in reply
    // (i.e., segments_out.size() > 0), to reflect an update in the ackno and
    // window size
    if (seg.length_in_sequence_space() > 0 &&
        _sender.segments_out().size() == 0) {
        _sender.fill_window();
        // if nothing can be sent, then send an empty segment
        if (_sender.segments_out().size() == 0) {
            _sender.send_empty_segment();
        }
    }

    // <5> responding to a "keep-alive" segment
    if (_receiver.ackno().has_value() &&
        (seg.length_in_sequence_space() == 0) &&
        (header.seqno == _receiver.ackno().value() - 1)) {
        _sender.send_empty_segment();
    }

    // Sending segments

    // <1> The TCPConnection will send TCPSegments over the Internet: any time
    // the TCPSender has pushed a segment onto its outgoing queue, having set
    // the fields it's responsible for on outgoing segments: (seqno, syn ,
    // payload, and fin ).
    send_segments();
    //  The variable starts out true. If the inbound stream ends before the
    //  TCPConnection has reached EOF on its outbound stream, this variable
    //  needs to be set to false.

    _time_since_last_segment_received =
        0;  // elapsed time since last segment received
    if (_receiver.stream_out().eof() &&
        !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::send_segments() {
    while (_sender.segments_out().size() > 0) {
        TCPSegment& segment = _sender.segments_out().front();
        TCPHeader& header = segment.header();
        // header.win is of type uint16_t, while receiver's window size is
        // uint32_t
        //  <2> Before sending the segment, the TCPConnection will ask the
        //  TCPReceiver for the fields it's responsible for on outgoing
        //  segments: ackno and window size. If there is an ackno, it will set
        //  the ack flag and the fields in the TCPSegment.
        header.win = min(size_t(numeric_limits<uint16_t>::max()),
                         _receiver.window_size());
        if (_receiver.ackno().has_value()) {
            header.ack = true;
            header.ackno = _receiver.ackno().value();
        }
        _segments_out.push(segment);
        _sender.segments_out().pop();
    }
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string& data) {
    size_t size = _sender.stream_in().write(data);  // data from application
    _sender.fill_window();
    send_segments();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to
//! this method

void TCPConnection::tick(const size_t ms_since_last_tick) {
    // When time passes
    _time_since_last_segment_received += ms_since_last_tick;

    // <1> abort the connection, and send a reset segment to the peer (an empty
    // segment with the rst flag set), if the number of consecutive
    // retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS
    if (_sender.consecutive_retransmissions() >= _cfg.MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        shutdown(false);
        return;
    }

    // <2> tell the TCPSender about the passage of time
    _sender.tick(ms_since_last_tick);
    send_segments();  // because some segment may need to be retransmitted
    // <3>  end the connection cleanly if necessary
    if (_receiver.stream_out().eof() &&  // Prereq #1
        _sender.stream_in().input_ended() &&     // Prereq #2
        (_sender.bytes_in_flight() ==
         0)) {  // Prereq # 3, i.e., all the bytes are received by the remote peer
        // At any point where prerequisites #1 through #3 are satisfied, the
        // connection is “done” (and active() should return false) if linger
        // after streams finish is false. Otherwise you need to linger: the
        // connection is only done after enough time (10 × cfg.rt timeout)
        // has elapsed since the last segment was received
        if (!_linger_after_streams_finish ||
            _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            shutdown(true);
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();  // send fin if necessary
    send_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();  // send syn
    send_segments();
}

void TCPConnection::send_rst_segment() {
    _sender.send_rst_segment();
    send_segments();
}

void TCPConnection::shutdown(bool is_clean_shutdown) {
    // In an unclean shutdown, the TCPConnection either sends or receives a
    // segment with the rst flag set. In this case, the outbound and inbound
    // ByteStreams should both be in the error state, and active() can return
    // false immediately.
    if (!is_clean_shutdown) {  // unclean shutdown
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    }
    _active = false;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst_segment();
            shutdown(false);
        }
    } catch (const exception& e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}