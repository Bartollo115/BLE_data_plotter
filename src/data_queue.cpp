#include "data_queue.hpp"

void DataQueue::push(std::unique_ptr<Packet> packet)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.size() >= MAX_QUEUE_DEPTH)
        queue_.pop();
    queue_.push(std::move(packet));
}

std::unique_ptr<Packet> DataQueue::try_pop()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.empty())
        return nullptr;
    auto packet = std::move(queue_.front());
    queue_.pop();
    return packet;
}

size_t DataQueue::drainInto(std::vector<std::unique_ptr<Packet>>& out,
                             size_t maxItems)
{
    std::lock_guard<std::mutex> lock(mtx_);
    size_t count = 0;
    while (!queue_.empty() && count < maxItems) {
        out.push_back(std::move(queue_.front()));
        queue_.pop();
        ++count;
    }
    return count;
}

bool DataQueue::empty() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.empty();
}

size_t DataQueue::size() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.size();
}
