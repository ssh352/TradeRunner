#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#include <boost/thread.hpp>


class semaphore
{
    unsigned int count_;
    boost::mutex mutex_;
    boost::condition_variable condition_;

public:
    explicit semaphore(unsigned int initial);

    void signal();
    void wait();
};

#endif // SEMAPHORE_H
