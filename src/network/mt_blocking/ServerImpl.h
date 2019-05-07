#ifndef AFINA_NETWORK_MT_BLOCKING_SERVER_H
#define AFINA_NETWORK_MT_BLOCKING_SERVER_H

#include <atomic>
#include <list>
#include <mutex>
#include <thread>

#include <afina/network/Server.h>

namespace spdlog {
class logger;
}

namespace Afina {
namespace Network {
namespace MTblocking {

class Worker;

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Logging::Service> pl);
    ~ServerImpl() override;

    // See Server.h
    void Start(uint16_t port, uint32_t, uint32_t) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void OnRun();

private:
    void RunWorker(int client_socket);

private:
    // Logger instance
    std::shared_ptr<spdlog::logger> _logger;

    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publish changes cross thread
    // bounds
    std::atomic<bool> running;

    // Server socket to accept connections on
    int _server_socket;

    // Thread to run network on
    std::thread _thread;

    uint32_t _max_workers;
    std::list<int> _workers_sockets;
    std::mutex _workers_mutex;
};

} // namespace MTblocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_BLOCKING_SERVER_H
