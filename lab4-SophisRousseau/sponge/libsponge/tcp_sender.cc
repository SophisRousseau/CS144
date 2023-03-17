#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight; 
}

void TCPSender::fill_window() {
    if(_finish_sending) {
        return;
    }

    _remaining_size = (_window_size ? _window_size : 1) - (next_seqno_absolute() - _current_ackno); // remaining space for sending new bytes to the receiver
    // have the sending space and not get to sending ending

    while (_remaining_size > 0 && !_finish_sending) {
        TCPSegment segment = TCPSegment();

        if (next_seqno_absolute() == 0) { // (1) handling SYN 
            segment.header().syn = true;
            _remaining_size--; // syn occupies space in the window
        }

        segment.header().seqno = next_seqno(); // (2) handling sequence number (seqno)
        
        segment.payload() = stream_in().read(min(_remaining_size, TCPConfig::MAX_PAYLOAD_SIZE)); // (3) handling payload // use min(_remaining_size, MSS)
        
        _remaining_size -= segment.payload().size();

        if (stream_in().eof() && _remaining_size > 0) { // (4) handling FIN
            segment.header().fin = true;
            _finish_sending = true;
            _remaining_size--; // fin occupies space in the window
        }

        if (segment.length_in_sequence_space() == 0) { // segment.length_in_sequence_space() == length of the segment 
            return;
        }

        segments_out().push(segment); // send segment
        _outstanding_segments.push(segment); // store outstanding segment
        if (!_retransmission_timer.is_started()) { // Every time a segment containing data (nonzero length in sequence space) is sent (whether it’s the first time or a retransmission), if the timer is not running, start it running so that it will expire after RTO milliseconds (for the current value of RTO).
            _retransmission_timer.start();
        }

        // update members in class TCPSender
        _next_seqno += segment.length_in_sequence_space(); // bytes sent to the receiver increases by segment.length_in_sequence_space()
        _bytes_in_flight += segment.length_in_sequence_space(); // outstanding bytes increase by segment.length_in_sequence_space()
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _current_ackno = unwrap(ackno, _isn, next_seqno_absolute()); // (1) read the ackno
    _window_size = window_size; // (2) read the window size (and update _window_size)

    bool flag = false;
    while (_outstanding_segments.size() > 0) {
        TCPSegment segment = _outstanding_segments.front();
        uint64_t absolute_seqno = unwrap(segment.header().seqno, _isn, next_seqno_absolute()); 
        if (absolute_seqno + segment.length_in_sequence_space() > _current_ackno) { // the segment is not fully acknowledged by the receiver
            _bytes_in_flight -= _current_ackno - absolute_seqno;
            break; // so do the segments following the segment
        }
        _outstanding_segments.pop(); // fully acknowledged segment
        _bytes_in_flight -= segment.length_in_sequence_space(); // outstanding bytes decrease by segment.length_in_sequence_space()
        flag = true; // acknowledges the successful receipt of new data
    }

    if (flag) { // the receiver acknowledges the successful receipt of new data
        _retransmission_timeout = _initial_retransmission_timeout; // Set the RTO back to its “initial value.
        if (_outstanding_segments.size() > 0) { // If the sender has any outstanding data, restart the retransmission timer so that it will expire after RTO milliseconds (for the current value of RTO).
            _retransmission_timer.start();
        } else { // When all outstanding data has been acknowledged, stop the retransmission timer.
            _retransmission_timer.stop();
        }
        _consecutive_retransmissions = 0; // Reset the count of “consecutive retransmissions” back to zero.
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(_retransmission_timer.is_expired(ms_since_last_tick, _retransmission_timeout)) {
        _segments_out.push(_outstanding_segments.front()); // push the oldest outstanding segment
        if (_window_size > 0) {
            _consecutive_retransmissions++;
            _retransmission_timeout *= 2;
        }
        _retransmission_timer.start();
    }  
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retransmissions; 
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segments_out().push(segment);
}