#include "semaphore.h"

semaphore::semaphore(unsigned int initial) : count_(initial) {}

void semaphore::signal()
{
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        ++count_;
    }
    condition_.notify_one();
}

void semaphore::wait()
{
    boost::unique_lock<boost::mutex> lock(mutex_);
    while (count_ == 0)
    {
        condition_.wait(lock);
    }
    --count_;
}
