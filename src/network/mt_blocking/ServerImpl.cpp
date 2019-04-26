#include "ServerImpl.h"
#include "Worker.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <arpa/inet.h>
#include <chrono>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <spdlog/logger.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>

#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace MTblocking {

void make_socket_non_blocking(int sfd) {
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Failed to call fcntl to get socket flags");
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        throw std::runtime_error("Failed to call fcntl to set socket flags");
    }
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Logging::Service> pl) : Server(ps, pl) {}

// See Server.h
ServerImpl::~ServerImpl() = default;

// See Server.h
void ServerImpl::Start(uint16_t port, uint32_t n_accept, uint32_t n_workers) {

    _max_workers = n_workers;

    _logger = pLogging->select("network");
    _logger->info("Start mt_blocking network service");

    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE); // adds the specified signal signo to the signal set
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_port = htons(port);       // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any address

    _server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    int opts = 1;
    if (setsockopt(_server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(_server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    make_socket_non_blocking(_server_socket);

    if (bind(_server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(_server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    if (listen(_server_socket, 5) == -1) {
        close(_server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    running.store(true);
    _thread = std::thread(&ServerImpl::OnRun, this);
}

// See Server.h
void ServerImpl::Stop() {
    running.store(false);
    shutdown(_server_socket, SHUT_RD);
    {
        std::lock_guard<std::mutex> lock(_workers_mutex);
        for (auto w : _workers) {
            w->Stop();
        }
    }
}

// See Server.h
void ServerImpl::Join() {
    {
        std::lock_guard<std::mutex> lock(_workers_mutex);
        for (auto w : _workers) {
            w->Join();
            delete w;
        }
    }

    assert(_thread.joinable());
    _thread.join();

    close(_server_socket);
}

// See Server.h
void ServerImpl::OnRun() {
    // Here is connection state
    // - parser: parse state of the stream
    // - command_to_execute: last command parsed out of stream
    // - arg_remains: how many bytes to read from stream to get command argument
    // - argument_for_command: buffer stores argument
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    while (running.load()) {
        _logger->debug("Max possible workers {}", _max_workers);
        _logger->debug("waiting for connection...");

        // The call to accept() blocks until the incoming connection arrives
        int client_socket;
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
    try_accept:
        if ((client_socket = accept(_server_socket, &client_addr, &client_addr_len)) == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ClearFinishedWorkers();
                goto try_accept;
            } else {
                _logger->debug("accept failed with error code {}\n", errno);
                break;
            }
        }

        // Got new connection
        if (_logger->should_log(spdlog::level::debug)) {
            std::string host = "unknown", port = "-1";

            char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
            if (getnameinfo(&client_addr, client_addr_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                            NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                host = hbuf;
                port = sbuf;
            }
            _logger->debug("Accepted connection on descriptor {} (host={}, port={})\n", client_socket, host, port);
        }

        // Configure read timeout
        {
            struct timeval tv;
            tv.tv_sec = 5; // TODO: make it configurable
            tv.tv_usec = 0;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        }

        ClearFinishedWorkers();

        // Start new thread and process data from/to connection
        {
            std::lock_guard<std::mutex> lock(_workers_mutex);
            if (_workers.size() <= _max_workers) {
                _logger->debug("Create new worker for client_socket {}\n", client_socket);
                _workers.push_back(new Worker(pStorage, pLogging, this));
                _workers.back()->Start(client_socket);
            } else {
                _logger->debug("Maximum connections reached, closing client_socket {}\n", client_socket);
                close(client_socket);
            }
        }
    }

    // Cleanup on exit...
    _logger->warn("Network stopped");
}

void ServerImpl::ClearFinishedWorkers() {
    std::lock_guard<std::mutex> lock1(_finished_worker_list_mutex);
    std::lock_guard<std::mutex> lock2(_workers_mutex);
    for (auto w : _finished_worker_list) {
        auto it = find(_workers.begin(), _workers.end(), w);
        if (it != _workers.end()) {
            _workers.erase(it);
            w->Join();
            delete w;
        }
    }
    _finished_worker_list.clear();
}

void ServerImpl::workerDidFinish(Worker *w) {
    std::lock_guard<std::mutex> lock(_finished_worker_list_mutex);
    _finished_worker_list.push_back(w);
}

} // namespace MTblocking
} // namespace Network
} // namespace Afina
