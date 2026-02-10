#pragma once

#include "third_party/moodycamel/readerwriterqueue.h"

#include <visage/utils.h>

namespace chowdsp::slides
{
struct Background_Task : visage::Thread
{
    using Task = std::function<void()>;
    moodycamel::ReaderWriterQueue<Task> task_queue { 10 };

    Background_Task() : Thread { "Background Task" }
    {
        start();
    }
    ~Background_Task() override
    {
        stop();
    }

    void run() override
    {
        while (shouldRun())
        {
            Task task {};
            while (task_queue.try_dequeue (task))
                task();

            sleep (100); // @TODO: tune this?
        }
    }
};
}
