// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

#include <boost/ut.hpp>
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(zephyrus::asset::test);

int main() {
    using namespace boost::ut;
    using namespace std::literals;
    using zephyrus::rmlui::cmrc_file_interface;

    zephyrus::log::init_logger();

    auto fs{cmrc::zephyrus::asset::test::get_filesystem()};

    "open_existing_file_returns_nonzero_handle"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);
        iface.Close(handle);
    };

    "open_nonexistent_file_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("no_such_file.txt")};
        expect(handle == 0);
    };

    "open_same_file_twice_returns_distinct_handles"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto h1{iface.Open("hello.txt")};
        auto h2{iface.Open("hello.txt")};
        expect(h1 != 0);
        expect(h2 != 0);
        expect(h1 != h2);
        iface.Close(h1);
        iface.Close(h2);
    };

    "close_valid_handle_does_not_crash"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);
        iface.Close(handle);
    };

    "double_close_does_not_crash"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);
        iface.Close(handle);
        iface.Close(handle);
    };

    "read_after_close_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        iface.Close(handle);
        char buf[16]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 0_u);
    };

    "read_full_file_content"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[64]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 13_u);
        expect(std::string_view{buf, n} == "Hello, world!"sv);
        iface.Close(handle);
    };

    "read_with_small_buffer"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[4]{};
        auto n1{iface.Read(buf, sizeof(buf), handle)};
        expect(n1 == 4_u);
        expect(std::string_view(buf, n1) == "Hell"sv);
        auto n2{iface.Read(buf, sizeof(buf), handle)};
        expect(n2 == 4_u);
        expect(std::string_view(buf, n2) == "o, w"sv);
        iface.Close(handle);
    };

    "read_at_eof_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[16]{};
        iface.Read(buf, sizeof(buf), handle);
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 0_u);
        iface.Close(handle);
    };

    "read_with_invalid_handle_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        char buf[16]{};
        auto n =
            iface.Read(buf, sizeof(buf), static_cast<Rml::FileHandle>(0xDEAD));
        expect(n == 0_u);
    };

    "read_empty_file_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("empty.txt")};
        expect(handle != 0);
        char buf[16]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 0_u);
        iface.Close(handle);
    };

    "read_advances_cursor"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[5]{};
        iface.Read(buf, sizeof(buf), handle);
        expect(iface.Tell(handle) == 5_u);
        iface.Close(handle);
    };

    "seek_set"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 5, SEEK_SET));
        expect(iface.Tell(handle) == 5_u);
        iface.Close(handle);
    };

    "seek_cur"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[3]{};
        iface.Read(buf, sizeof(buf), handle);
        expect(iface.Tell(handle) == 3_u);
        expect(iface.Seek(handle, 2, SEEK_CUR));
        expect(iface.Tell(handle) == 5_u);
        iface.Close(handle);
    };

    "seek_end"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, -3, SEEK_END));
        expect(iface.Tell(handle) == 10_u);
        iface.Close(handle);
    };

    "seek_past_beginning_clamps_to_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, -100, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "seek_past_end_clamps_to_file_size"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 100, SEEK_SET));
        expect(iface.Tell(handle) == 13_u);
        iface.Close(handle);
    };

    "seek_with_invalid_origin_returns_false"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(!iface.Seek(handle, 0, 999));
        iface.Close(handle);
    };

    "seek_with_invalid_handle_returns_false"_test = [&fs] {
        cmrc_file_interface iface{fs};
        expect(!iface.Seek(static_cast<Rml::FileHandle>(0xDEAD), 0, SEEK_SET));
    };

    "tell_with_invalid_handle_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        expect(iface.Tell(static_cast<Rml::FileHandle>(0xDEAD)) == 0_u);
    };

    "tell_at_file_start_is_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "read_subdirectory_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("dir/nested.txt")};
        expect(handle != 0);
        char buf[64]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 14_u);
        expect(std::string_view{buf, n} == "nested content"sv);
        iface.Close(handle);
    };

    "read_deeply_nested_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("dir/deep/leaf.txt")};
        expect(handle != 0);
        char buf[64]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 9_u);
        expect(std::string_view{buf, n} == "deep leaf"sv);
        iface.Close(handle);
    };

    "read_binary_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("binary.bin")};
        expect(handle != 0);
        char buf[16]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 4_u);
        expect(buf[0] == '\x00');
        expect(buf[1] == '\x01');
        expect(buf[2] == '\x02');
        expect(buf[3] == '\xFF');
        iface.Close(handle);
    };

    "destructor_cleans_up_open_files"_test = [&fs] {
        auto *iface{new cmrc_file_interface{fs}};
        iface->Open("hello.txt");
        iface->Open("dir/nested.txt");
        delete iface;
    };

    "full_lifecycle"_test = [&fs] {
        cmrc_file_interface iface{fs};

        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);

        char buf[5]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 5_u);
        expect(std::string_view{buf, n} == "Hello"sv);
        expect(iface.Tell(handle) == 5_u);

        expect(iface.Seek(handle, -3, SEEK_END));
        expect(iface.Tell(handle) == 10_u);

        n = iface.Read(buf, sizeof(buf), handle);
        expect(n == 3_u);
        expect(std::string_view{buf, n} == "ld!"sv);
        expect(iface.Tell(handle) == 13_u);

        n = iface.Read(buf, sizeof(buf), handle);
        expect(n == 0_u);

        iface.Close(handle);
    };
}
