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

    "open_empty_string_path_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("")};
        expect(handle == 0);
    };

    "open_path_traversal_parent_ref_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        expect(iface.Open("../hello.txt") == 0);
    };

    "open_path_traversal_deep_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        expect(iface.Open("../../etc/passwd") == 0);
    };

    "open_path_traversal_dotdot_slash_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        expect(iface.Open("dir/../../hello.txt") == 0);
    };

    "close_null_handle_does_not_crash"_test = [&fs] {
        cmrc_file_interface iface{fs};
        iface.Close(0);
    };

    "close_many_handles_for_same_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        constexpr int count{16};
        Rml::FileHandle handles[count]{};
        for (int i{}; i < count; ++i) {
            handles[i] = iface.Open("hello.txt");
            expect(handles[i] != 0);
        }
        for (int i{}; i < count; ++i) {
            iface.Close(handles[i]);
        }
    };

    "read_with_null_buffer_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);
        auto n{iface.Read(nullptr, 16, handle)};
        expect(n == 0_u);
        iface.Close(handle);
    };

    "read_with_zero_size_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(handle != 0);
        char buf[16]{};
        auto n{iface.Read(buf, 0, handle)};
        expect(n == 0_u);
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "read_after_seek_to_beginning"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[16]{};
        iface.Read(buf, sizeof(buf), handle);
        expect(iface.Tell(handle) == 13_u);
        expect(iface.Seek(handle, 0, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 13_u);
        expect(std::string_view{buf, n} == "Hello, world!"sv);
        iface.Close(handle);
    };

    "read_after_seek_to_end_returns_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 0, SEEK_END));
        char buf[16]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 0_u);
        iface.Close(handle);
    };

    "read_binary_file_byte_by_byte"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("binary.bin")};
        expect(handle != 0);
        const unsigned char expected[] = {0x00, 0x01, 0x02, 0xFF};
        char c{};
        for (std::size_t i{}; i < 4; ++i) {
            auto n{iface.Read(&c, 1, handle)};
            expect(n == 1_u);
            expect(static_cast<unsigned char>(c) == expected[i]);
        }
        expect(iface.Read(&c, 1, handle) == 0_u);
        iface.Close(handle);
    };

    "two_reads_split_at_boundary"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[13]{};
        auto n1{iface.Read(buf, 6, handle)};
        expect(n1 == 6_u);
        expect(std::string_view{buf, n1} == "Hello,"sv);
        auto n2{iface.Read(buf + 6, 7, handle)};
        expect(n2 == 7_u);
        expect(std::string_view{buf + 6, n2} == " world!"sv);
        expect(iface.Read(buf, 1, handle) == 0_u);
        iface.Close(handle);
    };

    "read_advances_cursor_correctly_across_multiple_reads"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        char buf[4]{};
        iface.Read(buf, 2, handle);
        expect(iface.Tell(handle) == 2_u);
        iface.Read(buf, 1, handle);
        expect(iface.Tell(handle) == 3_u);
        iface.Read(buf, 3, handle);
        expect(iface.Tell(handle) == 6_u);
        iface.Close(handle);
    };

    "seek_zero_from_beginning_stays_at_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 0, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "multiple_sequential_seeks"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 5, SEEK_SET));
        expect(iface.Tell(handle) == 5_u);
        expect(iface.Seek(handle, 3, SEEK_CUR));
        expect(iface.Tell(handle) == 8_u);
        expect(iface.Seek(handle, -2, SEEK_END));
        expect(iface.Tell(handle) == 11_u);
        expect(iface.Seek(handle, 0, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "seek_on_empty_file"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("empty.txt")};
        expect(handle != 0);
        expect(iface.Seek(handle, 0, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        expect(iface.Seek(handle, 5, SEEK_SET));
        expect(iface.Tell(handle) == 0_u);
        expect(iface.Seek(handle, -5, SEEK_END));
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "negative_seek_cur_backwards"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 10, SEEK_SET));
        expect(iface.Tell(handle) == 10_u);
        expect(iface.Seek(handle, -5, SEEK_CUR));
        expect(iface.Tell(handle) == 5_u);
        iface.Close(handle);
    };

    "seek_then_read_consistent"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        expect(iface.Seek(handle, 7, SEEK_SET));
        char buf[6]{};
        auto n{iface.Read(buf, sizeof(buf), handle)};
        expect(n == 6_u);
        expect(std::string_view{buf, n} == "world!"sv);
        iface.Close(handle);
    };

    "tell_after_seek_and_read"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("hello.txt")};
        iface.Seek(handle, 5, SEEK_SET);
        char buf[3]{};
        iface.Read(buf, 3, handle);
        expect(iface.Tell(handle) == 8_u);
        iface.Close(handle);
    };

    "tell_on_empty_file_is_zero"_test = [&fs] {
        cmrc_file_interface iface{fs};
        auto handle{iface.Open("empty.txt")};
        expect(handle != 0);
        expect(iface.Tell(handle) == 0_u);
        iface.Close(handle);
    };

    "two_interfaces_share_filesystem"_test = [&fs] {
        cmrc_file_interface iface1{fs};
        cmrc_file_interface iface2{fs};
        auto h1{iface1.Open("hello.txt")};
        auto h2{iface2.Open("hello.txt")};
        expect(h1 != 0);
        expect(h2 != 0);
        char buf1[64]{};
        char buf2[64]{};
        auto n1{iface1.Read(buf1, sizeof(buf1), h1)};
        auto n2{iface2.Read(buf2, sizeof(buf2), h2)};
        expect(n1 == 13_u);
        expect(n2 == 13_u);
        expect(std::string_view{buf1, n1} == std::string_view{buf2, n2});
        iface1.Close(h1);
        iface2.Close(h2);
    };

    "open_many_files_simultaneously"_test = [&fs] {
        cmrc_file_interface iface{fs};
        constexpr int count{32};
        Rml::FileHandle handles[count]{};
        for (int i{}; i < count; ++i) {
            handles[i] = iface.Open("hello.txt");
            expect(handles[i] != 0);
        }
        for (int i{}; i < count; ++i) {
            char buf[13]{};
            auto n{iface.Read(buf, sizeof(buf), handles[i])};
            expect(n == 13_u);
            expect(std::string_view{buf, n} == "Hello, world!"sv);
        }
        for (int i{}; i < count; ++i) {
            iface.Close(handles[i]);
        }
    };
}
