// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include <boost/ut.hpp>
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(zephyrus::asset::test);

int main() {
    using namespace boost::ut;
    using namespace std::literals;
    using zephyrus::rmlui::cmrc_file_interface;

    zephyrus::log::init_logger();

    auto fs{cmrc::zephyrus::asset::test::get_filesystem()};
    constexpr int kThreadCount{8};

    "concurrent_open_close"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        const char *files[]{"hello.txt", "empty.txt", "dir/nested.txt",
                            "dir/deep/leaf.txt"};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count, i, &files] {
                auto handle{iface.Open(files[i % 4])};
                if (handle != 0) {
                    char buf[64]{};
                    iface.Read(buf, sizeof(buf), handle);
                    iface.Close(handle);
                    ++success_count;
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
    };

    "concurrent_read_same_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count] {
                auto handle{iface.Open("hello.txt")};
                expect(handle != 0);
                char buf[64]{};
                auto n{iface.Read(buf, sizeof(buf), handle)};
                if (n == 13 && std::string_view{buf, n} == "Hello, world!"sv) {
                    ++success_count;
                }
                iface.Close(handle);
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
    };

    "concurrent_read_same_handle"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);

        std::vector<std::thread> threads;
        std::atomic<size_t> total_bytes{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &total_bytes, handle] {
                char buf[4]{};
                for (;;) {
                    auto n{iface.Read(buf, sizeof(buf), handle)};
                    if (n == 0) {
                        break;
                    }
                    total_bytes += n;
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(total_bytes.load() == 13_u);
        iface.Close(handle);
    };

    "concurrent_mixed_operations"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count] {
                auto handle{iface.Open("hello.txt")};
                expect(handle != 0);

                char buf[5]{};
                auto n{iface.Read(buf, sizeof(buf), handle)};
                expect(n == 5_u);
                expect(std::string_view{buf, n} == "Hello"sv);

                expect(iface.Seek(handle, 0, SEEK_SET));
                expect(iface.Tell(handle) == 0_u);

                expect(iface.Seek(handle, -3, SEEK_END));
                expect(iface.Tell(handle) == 10_u);

                iface.Close(handle);
                ++success_count;
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
    };

    "concurrent_open_close_stress"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> total_operations{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &total_operations] {
                for (int j{}; j < 100; ++j) {
                    auto handle{iface.Open("hello.txt")};
                    expect(handle != 0);
                    char buf[13]{};
                    auto n{iface.Read(buf, sizeof(buf), handle)};
                    expect(n == 13_u);
                    iface.Close(handle);
                    ++total_operations;
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(total_operations == kThreadCount * 100);
    };

    // === Thread edge cases ===

    "concurrent_seek_same_handle"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count, handle, i] {
                for (int j{}; j < 50; ++j) {
                    iface.Seek(handle, i * 1, SEEK_SET);
                }
                ++success_count;
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
        expect(iface.Tell(handle) < 14_u);
        iface.Close(handle);
    };

    "concurrent_close_same_handle"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);

        std::vector<std::thread> threads;
        std::atomic<int> close_count{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &close_count, handle] {
                iface.Close(handle);
                ++close_count;
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(close_count == kThreadCount);
    };

    "concurrent_read_seek_same_handle"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);

        std::vector<std::thread> threads;
        std::atomic<int> total_ops{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &total_ops, handle] {
                for (int j{}; j < 50; ++j) {
                    char buf[4]{};
                    iface.Read(buf, sizeof(buf), handle);
                    iface.Seek(handle, 0, SEEK_SET);
                    ++total_ops;
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(total_ops == kThreadCount * 50);
        iface.Close(handle);
    };

    "concurrent_operations_on_empty_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count] {
                auto handle{iface.Open("empty.txt")};
                expect(handle != 0);
                char buf[16]{};
                auto n{iface.Read(buf, sizeof(buf), handle)};
                expect(n == 0_u);
                expect(iface.Seek(handle, 0, SEEK_SET));
                expect(iface.Tell(handle) == 0_u);
                iface.Close(handle);
                ++success_count;
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
    };

    "concurrent_read_binary_byte_by_byte"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<size_t> total_bytes{0};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &total_bytes] {
                auto handle{iface.Open("binary.bin")};
                expect(handle != 0);
                char c{};
                size_t count{};
                while (iface.Read(&c, 1, handle) == 1_u) {
                    ++count;
                }
                expect(count == 4_u);
                total_bytes += count;
                iface.Close(handle);
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(total_bytes.load() == static_cast<size_t>(kThreadCount * 4));
    };

    "high_thread_count_stress"_test = [&fs] {
        cmrc_file_interface iface{fs};
        constexpr int kHighThreadCount{64};
        std::vector<std::thread> threads;
        std::atomic<int> total_operations{0};

        for (int i{}; i < kHighThreadCount; ++i) {
            threads.emplace_back([&iface, &total_operations] {
                for (int j{}; j < 50; ++j) {
                    auto handle{iface.Open("hello.txt")};
                    expect(handle != 0);
                    char buf[13]{};
                    auto n{iface.Read(buf, sizeof(buf), handle)};
                    expect(n == 13_u);
                    expect(iface.Seek(handle, 0, SEEK_SET));
                    n = iface.Read(buf, sizeof(buf), handle);
                    expect(n == 13_u);
                    iface.Close(handle);
                    ++total_operations;
                }
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(total_operations == kHighThreadCount * 50);
    };

    "concurrent_many_files"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        const char *files[]{"hello.txt", "empty.txt", "binary.bin",
                            "dir/nested.txt", "dir/deep/leaf.txt"};

        for (int i{}; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &success_count, i, &files] {
                for (int j{}; j < 20; ++j) {
                    auto handle{iface.Open(files[(i * 20 + j) % 5])};
                    expect(handle != 0);
                    char buf[64]{};
                    iface.Read(buf, sizeof(buf), handle);
                    iface.Seek(handle, 0, SEEK_SET);
                    iface.Read(buf, sizeof(buf), handle);
                    iface.Close(handle);
                }
                ++success_count;
            });
        }

        for (auto &t : threads) {
            t.join();
        }
        expect(success_count == kThreadCount);
    };
}
