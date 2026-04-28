#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include "packet.hpp"
#include "data_queue.hpp"

class DataQueue {
public:
    void push(std::unique_ptr<Packet> packet)
    {
            std::lock_guard<std::mutex> lock(mtx_);

            if(queue_.size()>=MAX_QUEUE_DEPTH) {
                queue_.pop();
            }
            queue_.push(std::move(packet));
    }

    std::unique_ptr<Packet> try_pop()
    {
        std::lock_guard<std::mutex> lock(mtx_);

        if(queue_.empty())
        {
            return nullptr;
        }

        std::unique_ptr<Packet> packet = std::move(queue_.front());
        queue_.pop();
        return packet;
    }

    size_t drainInto(std::vector<std::unique_ptr<Packet>>& out,
                     size_t maxItems = 256)
    {
        std::lock_guard<std::mutex> lock(mtx_);

        size_t count = 0;
        while (!queue_.empty() && count < maxItems)
        {
            out.push_back(std::move(queue_.front()));
            queue_.pop();
            count++;
        }
        return count;
    }

    bool   empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }    
    size_t size()  const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    static constexpr size_t MAX_QUEUE_DEPTH = 1024;

private:
    mutable std::mutex              mtx_;
    std::queue<std::unique_ptr<Packet>> queue_;
};
