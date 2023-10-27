#ifndef WS_COMMON_H
#define WS_COMMON_H

#include <stdint.h>
#include <stddef.h>

/**
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
*/

#define WEBSOCKET_HANDSHAKE_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_VERSION "13"

#define WS_OPCODE_CONTINUE 0b0000
#define WS_OPCODE_TEXT     0b0001
#define WS_OPCODE_BINARY   0b0010
#define WS_OPCODE_CLOSE    0b1000
#define WS_OPCODE_PING     0b1001
#define WS_OPCODE_PONG     0b1010

/** An enum of errors when parsing a frame. */
enum ws_frame_parsing_errors
{
    /** The `recv` syscall failed. */
    WS_FRAME_PARSE_ERROR_RECV = -1,
    /** The payload length for the frame is invalid. */
    WS_FRAME_PARSE_ERROR_INVALID_FRAME_LENGTH = -2,
    /** The payload length is too big. */
    WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG = -3
};

/** The parsing state of a frame. */
enum ws_frame_parsing_states
{
    /** The first byte is being parsed. */
    WS_FRAME_PARSING_STATE_FIRST_BYTE,
    /** The second byte is being parsed. */
    WS_FRAME_PARSING_STATE_SECOND_BYTE,
    /** The payload length is going through further review. */
    WS_FRAME_PARSING_STATE_PAYLOAD_LENGTH,
    /** The masking key is being parsed. */
    WS_FRAME_PARSING_STATE_MASKING_KEY,
    /** The payload data is being parsed. */
    WS_FRAME_PARSING_STATE_PAYLOAD_DATA
};

/** A structure representing a WebSocket frame. */
struct ws_frame
{
    /** Whether or not this is the last frame for the entire message. */
    uint8_t fin:     1;

    /** Bits specifically dependent on extensions. Should be set to 0 otherwise. */
    uint8_t rsv1:    1;
    uint8_t rsv2:    1;
    uint8_t rsv3:    1;

    /** The operational code informing what this frame is doing. */
    uint8_t opcode:  4;

    /** Whether or not the payload is masked. */
    uint8_t mask:    1;
    /** The masking key to bitwise XOR the payload data. */
    uint8_t masking_key[4];

    /** The length of the payload data. */
    uint64_t payload_length;
};

/** A structure representing a WebSocket message. */
struct ws_message
{
    /** The opcode for the message. */
    uint8_t opcode;
    /** The buffer of data. */
    uint8_t *buffer;
    /** The size of the buffer. */
    size_t payload_length;
};

/** A structure representing the parsing state of one WebSocket frame. */
struct ws_frame_parsing_state
{
    /** The current state of the frame parser. */
    enum ws_frame_parsing_states parsing_state;

    /** The current frame being parsed. */
    struct ws_frame frame;
    /** The current websocket message. */
    struct ws_message message;

    /** The real payload length being filled (if payload length >= 126). */
    uint64_t real_payload_length;
    /** The amount of bytes received. */
    uint64_t received_length;
};

#endif // WS_COMMON_H