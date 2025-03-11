#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <queue>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "Ws2_32.lib")

// 消息头
struct MessageHeader {
    uint32_t length;
};

// 消息类型 A
struct AddOrderNoMPID {
    char type = 'A';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};

// 消息类型 F
struct AddOrderWithMPID {
    char type = 'F';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    char attribution[4];
};

// 消息类型 U
struct OrderReplace {
    char type = 'U';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t originalOrderReference;
    uint64_t newOrderReferenceNumber;
    uint32_t shares;
    uint32_t price;
};

// 消息类型 E
struct OrderExecuted {
    char type = 'E';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
};

// 消息类型 C
struct OrderExecutedWithPrice {
    char type = 'C';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
    char printable;
    uint32_t executionPrice;
};

// 消息类型 X
struct OrderCancel {
    char type = 'X';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
    uint32_t cancelledShares;
};

// 消息类型 D
struct OrderDelete {
    char type = 'D';
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t orderReferenceNumber;
};

// 转换为网络字节序
uint16_t htons_wrapper(uint16_t value) {
    return htons(value);
}

uint32_t htonl_wrapper(uint32_t value) {
    return htonl(value);
}

uint64_t htonll_wrapper(uint64_t value) {
    uint32_t high = static_cast<uint32_t>(value >> 32);
    uint32_t low = static_cast<uint32_t>(value & 0xFFFFFFFF);
    return (static_cast<uint64_t>(htonl_wrapper(high)) << 32) | htonl_wrapper(low);
}

// 生成随机字符串
std::string generateRandomString(size_t length) {
    static const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.size() - 1);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

// 序列化消息
template <typename T>
std::vector<uint8_t> serialize(const T& msg) {
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + sizeof(T));
    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
    header->length = htonl_wrapper(sizeof(MessageHeader) + sizeof(T));

    T* msgPtr = reinterpret_cast<T*>(buffer.data() + sizeof(MessageHeader));
    *msgPtr = msg;

    // 转换整数字段到网络字节序
    msgPtr->stockLocate = htons_wrapper(msgPtr->stockLocate);
    msgPtr->trackingNumber = htons_wrapper(msgPtr->trackingNumber);
    msgPtr->timestamp = htonll_wrapper(msgPtr->timestamp);

    if constexpr (std::is_same_v<T, AddOrderNoMPID> || std::is_same_v<T, AddOrderWithMPID>) {
        msgPtr->orderReferenceNumber = htonll_wrapper(msgPtr->orderReferenceNumber);
        msgPtr->shares = htonl_wrapper(msgPtr->shares);
        msgPtr->price = htonl_wrapper(msgPtr->price);
    }
    else if constexpr (std::is_same_v<T, OrderReplace>) {
        msgPtr->originalOrderReference = htonll_wrapper(msgPtr->originalOrderReference);
        msgPtr->newOrderReferenceNumber = htonll_wrapper(msgPtr->newOrderReferenceNumber);
        msgPtr->shares = htonl_wrapper(msgPtr->shares);
        msgPtr->price = htonl_wrapper(msgPtr->price);
    }
    else if constexpr (std::is_same_v<T, OrderExecuted> || std::is_same_v<T, OrderExecutedWithPrice>) {
        msgPtr->orderReferenceNumber = htonll_wrapper(msgPtr->orderReferenceNumber);
        msgPtr->executedShares = htonl_wrapper(msgPtr->executedShares);
        msgPtr->matchNumber = htonll_wrapper(msgPtr->matchNumber);
        if constexpr (std::is_same_v<T, OrderExecutedWithPrice>) {
            msgPtr->executionPrice = htonl_wrapper(msgPtr->executionPrice);
        }
    }
    else if constexpr (std::is_same_v<T, OrderCancel>) {
        msgPtr->orderReferenceNumber = htonll_wrapper(msgPtr->orderReferenceNumber);
        msgPtr->cancelledShares = htonl_wrapper(msgPtr->cancelledShares);
    }
    else if constexpr (std::is_same_v<T, OrderDelete>) {
        msgPtr->orderReferenceNumber = htonll_wrapper(msgPtr->orderReferenceNumber);
    }

    if constexpr (std::is_same_v<T, AddOrderWithMPID>) {
        std::string mpid = generateRandomString(4);
        std::copy(mpid.begin(), mpid.end(), msgPtr->attribution);
    }

    return buffer;
}

// 线程安全的队列
template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;

public:
    void push(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(value);
        lock.unlock();
        cond_var_.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        return true;
    }
};

// 发送线程函数
void senderThread(SOCKET sock, ThreadSafeQueue<std::vector<uint8_t>>& messageQueue) {
    std::vector<uint8_t> batch;
    const size_t batchSize = 1024 * 1024; // 1MB 批量发送
    batch.reserve(batchSize);

    while (true) {
        std::vector<uint8_t> message;
        if (messageQueue.pop(message)) {
            batch.insert(batch.end(), message.begin(), message.end());
            if (batch.size() >= batchSize) {
                send(sock, reinterpret_cast<const char*>(batch.data()), static_cast<int>(batch.size()), 0);
                batch.clear();
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 禁用 Nagle 算法
    int opt = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt));

    ThreadSafeQueue<std::vector<uint8_t>> messageQueue;
    std::thread sender(senderThread, sock, std::ref(messageQueue));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 6);

    while (true) {
        int msgType = dis(gen);
        std::vector<uint8_t> serializedMessage;

        switch (msgType) {
        case 0: {
            AddOrderNoMPID msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            msg.buySellIndicator = (gen() % 2) ? 'B' : 'S';
            msg.shares = static_cast<uint32_t>(gen());
            std::string stock = generateRandomString(8);
            std::copy(stock.begin(), stock.end(), msg.stock);
            msg.price = static_cast<uint32_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 1: {
            AddOrderWithMPID msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            msg.buySellIndicator = (gen() % 2) ? 'B' : 'S';
            msg.shares = static_cast<uint32_t>(gen());
            std::string stock = generateRandomString(8);
            std::copy(stock.begin(), stock.end(), msg.stock);
            msg.price = static_cast<uint32_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 2: {
            OrderReplace msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.originalOrderReference = static_cast<uint64_t>(gen());
            msg.newOrderReferenceNumber = static_cast<uint64_t>(gen());
            msg.shares = static_cast<uint32_t>(gen());
            msg.price = static_cast<uint32_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 3: {
            OrderExecuted msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            msg.executedShares = static_cast<uint32_t>(gen());
            msg.matchNumber = static_cast<uint64_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 4: {
            OrderExecutedWithPrice msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            msg.executedShares = static_cast<uint32_t>(gen());
            msg.matchNumber = static_cast<uint64_t>(gen());
            msg.printable = (gen() % 2) ? 'Y' : 'N';
            msg.executionPrice = static_cast<uint32_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 5: {
            OrderCancel msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            msg.cancelledShares = static_cast<uint32_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        case 6: {
            OrderDelete msg;
            msg.stockLocate = static_cast<uint16_t>(gen());
            msg.trackingNumber = static_cast<uint16_t>(gen());
            msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.orderReferenceNumber = static_cast<uint64_t>(gen());
            serializedMessage = serialize(msg);
            break;
        }
        }

        messageQueue.push(serializedMessage);
    }

    sender.join();
    closesocket(sock);
    WSACleanup();
    return 0;
}
