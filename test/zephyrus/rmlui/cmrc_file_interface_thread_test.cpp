// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

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

    auto fs{cmrc::zephyrus::asset::test::get_filesystem()};
    constexpr int kThreadCount{8};

    "concurrent_open_close"_test = [&fs] {
        cmrc_file_interface iface{fs};
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        const char *files[]{"hello.txt", "empty.txt", "dir/nested.txt",
                            "dir/deep/leaf.txt"};

        for (int i = 0; i < kThreadCount; ++i) {
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

        for (int i = 0; i < kThreadCount; ++i) {
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

        for (int i = 0; i < kThreadCount; ++i) {
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

        for (int i = 0; i < kThreadCount; ++i) {
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

        for (int i = 0; i < kThreadCount; ++i) {
            threads.emplace_back([&iface, &total_operations] {
                for (int j = 0; j < 100; ++j) {
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
}
