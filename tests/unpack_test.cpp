/**
 * @file unpack_test.cpp
 * @brief Tests for tools/unpack.sh, the archive dispatcher referenced by every
 *        generated install plan.
 *
 * The script picks an extraction strategy from the archive's *name*, so these
 * tests exercise it end to end: each one builds a real archive, runs the script
 * through bash, and inspects the extracted payload.  Running the script matters
 * because a dispatch mistake still yields a perfectly well-formed install plan
 * (the plan only records ``bash unpack.sh <archive> source``) and surfaces only
 * at install time.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

    /**
     * @brief Fixture that extracts archives with the real tools/unpack.sh.
     *
     * Each test gets a private temporary directory, and the script is invoked
     * with that directory as its working directory so the relative paths in its
     * arguments resolve inside the sandbox.
     */
    class UnpackScript : public ::testing::Test {
       protected:
        void SetUp() override {
            path_ = std::filesystem::temp_directory_path() /
                    ("kez-unpack-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
            script_ = std::filesystem::path(KEZ_SOURCE_DIR) / "tools" / "unpack.sh";
            ASSERT_TRUE(std::filesystem::is_regular_file(script_)) << script_;
        }

        void TearDown() override { std::filesystem::remove_all(path_); }

        /** @brief Run a shell command; returns the exit status. */
        static int shell(const std::string& command) { return std::system(command.c_str()); }

        /** @brief Build an archive inside the sandbox. */
        int pack(const std::string& command) const {
            return shell("cd " + path_.string() + " && " + command);
        }

        /** @brief Run tools/unpack.sh on @p archive, extracting into @p dest. */
        int unpack(const std::string& archive, const std::string& dest) const {
            return shell("cd " + path_.string() + " && bash " + script_.string() + " " + archive +
                         " " + dest);
        }

        void write(const std::filesystem::path& file, const std::string& contents) const {
            std::ofstream output(file);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::string read(const std::filesystem::path& file) const {
            std::ifstream input(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>());
        }

        std::filesystem::path path_;
        std::filesystem::path script_;
    };

    TEST_F(UnpackScript, ExtractsGzipPayloadIntoDestinationDirectory) {
        // Kez downloads gzip sources as "source.gz", so the payload must land as
        // "<dest>/source" rather than being handed to tar, which cannot read it.
        write(path_ / "source", "tree-sitter payload\n");
        ASSERT_EQ(pack("gzip source"), 0);

        ASSERT_EQ(unpack("source.gz", "dest"), 0);

        EXPECT_EQ(read(path_ / "dest" / "source"), "tree-sitter payload\n");
    }

    TEST_F(UnpackScript, RoutesCompressedTarballThroughTarExtraction) {
        // A *.tar.gz archive also ends in .gz, so it has to be recognised as a
        // tarball before the gzip rule claims it and gunzips the tar stream.
        std::filesystem::create_directories(path_ / "payload");
        write(path_ / "payload" / "file.txt", "tar payload\n");
        ASSERT_EQ(pack("tar -czf source.tar.gz payload"), 0);

        ASSERT_EQ(unpack("source.tar.gz", "dest"), 0);

        EXPECT_EQ(read(path_ / "dest" / "file.txt"), "tar payload\n");
    }

    TEST_F(UnpackScript, RoutesShortTarballSuffixThroughTarExtraction) {
        // .tgz (like .tbz and .tbz2) carries no "tar" substring, so matching
        // tarballs by substring would reject it as an unknown format.
        std::filesystem::create_directories(path_ / "payload");
        write(path_ / "payload" / "file.txt", "short suffix payload\n");
        ASSERT_EQ(pack("tar -czf source.tgz payload"), 0);

        ASSERT_EQ(unpack("source.tgz", "dest"), 0);

        EXPECT_EQ(read(path_ / "dest" / "file.txt"), "short suffix payload\n");
    }

}  // namespace
