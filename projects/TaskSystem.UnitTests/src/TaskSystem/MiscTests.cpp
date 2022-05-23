#include <fmt/format.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;


namespace TaskSystem::Tests
{
    namespace
    {
        std::atomic_flag logMutex;

        void Log(std::string_view message)
        {
            std::cout << message;
        }

    }  // namespace

    TEST(MiscTests, DISABLED_atomicFlagWaitAndNotify)
    {
        std::atomic_flag flag;
        flag.clear(std::memory_order_relaxed);
        //flag.test_and_set();

        Log("[main] Starting worker\n");

        std::array<std::thread, 10u> threads;

        for (auto i = 0u; i < threads.size() / 2u; ++i)
        {
            threads[i] = std::thread([&flag, i = i]() {
                Log(fmt::format("[worker-{}] wait flag\n", i));
                flag.wait(false, std::memory_order_relaxed);

                Log(fmt::format("[worker-{}] terminating\n", i));
            });
        }

        std::jthread flagSetThread([&]() {
            Log("[main] set flag: true\n");
            flag.test_and_set(std::memory_order_relaxed);

            //std::this_thread::sleep_for(1s);
            //Log("[main] notify one flag\n");
            //flag.notify_one();

            //std::this_thread::sleep_for(1s);
            Log("[main] notify all flag\n");
            flag.notify_all();
        });

        for (auto i = threads.size() / 2u; i < threads.size(); ++i)
        {
            threads[i] = std::thread([&flag, i = i]() {
                Log(fmt::format("[worker-{}] wait flag\n", i));
                flag.wait(false, std::memory_order_relaxed);

                Log(fmt::format("[worker-{}] terminating\n", i));
            });
        }

        Log("[main] join threads\n");
        for (auto i = 0u; i < threads.size(); ++i)
        {
            threads[i].join();
        }

        std::this_thread::sleep_for(1ms);
        Log("[main] terminating\n");
    }

    TEST(MiscTests, sizeOfStuff)
    {
        EXPECT_EQ(sizeof(void *), 8u);
        EXPECT_EQ(sizeof(std::atomic_flag), 8u);
    }

}  // namespace TaskSystem::Tests