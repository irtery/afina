#include "Connection.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

static const int READ_EVENTS = EPOLLIN | EPOLLRDHUP | EPOLLERR;
static const int WRITE_EVENTS = EPOLLOUT | EPOLLRDHUP | EPOLLERR;
static const int READ_WRITE_EVENTS = EPOLLOUT | EPOLLIN | EPOLLRDHUP | EPOLLERR;

// See Connection.h
void Connection::Start() {
    _logger->debug("Start on descriptor {}", _socket);
    _is_alive = true;

    // Prepare for the new command
    command_to_execute.reset();
    argument_for_command.resize(0);
    parser.Reset();
    arg_remains = 0;

    // Prepare for reading
    _written_bytes = 0;
    _read_bytes = 0;
    _buffers_for_write.clear();
    _event.events = READ_EVENTS;
}

// See Connection.h
void Connection::OnError() {
    std::unique_lock<std::mutex> lock(_lock);
    _logger->debug("OnError on descriptor {}", _socket);
    _is_alive = false;
}

// See Connection.h
void Connection::OnClose() {
    std::unique_lock<std::mutex> lock(_lock);
    _logger->debug("OnClose on descriptor {}", _socket);
    _is_alive = false;
}

// See Connection.h
void Connection::DoRead() {
    std::unique_lock<std::mutex> lock(_lock);
    _logger->debug("DoRead on descriptor {}", _socket);
    int client_socket = _socket;
    command_to_execute = nullptr;

    try {
        int new_bytes;
        while ((new_bytes = read(client_socket, client_buffer + _read_bytes, sizeof(client_buffer) - _read_bytes)) >
               0) {
            _logger->debug("Got {} bytes from socket", new_bytes);
            _read_bytes += new_bytes;

            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (_read_bytes > 0) {
                _logger->debug("Process {} bytes", _read_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, _read_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, _read_bytes - parsed);
                        _read_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", _read_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(_read_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, _read_bytes - to_read);
                    arg_remains -= to_read;
                    _read_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    _buffers_for_write.push_back(result);
                    _event.events = READ_WRITE_EVENTS;

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (_read_bytes > 0) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", client_socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
    std::unique_lock<std::mutex> lock(_lock);
    _logger->debug("DoWrite on descriptor {}", _socket);

    auto buffers_size = _buffers_for_write.size();
    auto buffers_it = _buffers_for_write.begin();
    struct iovec buffers_iov[buffers_size];

    for (auto i = 0; i < buffers_size; ++i, ++buffers_it) {
        buffers_iov[i].iov_base = &(*buffers_it)[0];
        buffers_iov[i].iov_len = (*buffers_it).size();
    }
    buffers_iov[0].iov_base += _written_bytes;
    buffers_iov[0].iov_len -= _written_bytes;

    int written = writev(_socket, buffers_iov, buffers_size);
    _written_bytes += written;

    auto del_it = _buffers_for_write.begin();
    for (int del_it_ind = 0; _written_bytes > buffers_iov[del_it_ind].iov_len; ++del_it_ind) {
        if (buffers_size > del_it_ind) {
            break;
        }
        _written_bytes -= buffers_iov[del_it_ind].iov_len;
        ++del_it;
    }

    _buffers_for_write.erase(_buffers_for_write.begin(), del_it);

    if (_buffers_for_write.size() == 0) {
        _event.events = READ_EVENTS;
    } else {
        _event.events = READ_WRITE_EVENTS;
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
