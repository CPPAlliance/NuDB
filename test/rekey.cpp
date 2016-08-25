//
// Copyright (c) 2015-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained
#include <nudb/rekey.hpp>

#include "test_util.hpp"
#include <nudb/progress.hpp>
#include <nudb/verify.hpp>
#include <beast/unit_test/suite.hpp>

namespace nudb {
namespace test {

// Simple test to check that rekey works, and
// also to exercise all its failure paths.
//
class rekey_test : public beast::unit_test::suite
{
public:
    void
    do_recover(std::size_t N, nsize_t blockSize, float loadFactor)
    {
        auto const keys = static_cast<std::size_t>(
            loadFactor * detail::bucket_capacity(blockSize));
        std::size_t const bufferSize =
            blockSize * (1 + ((N + keys - 1) / keys));

        temp_dir td;
        auto const dp  = td.file ("nudb.dat");
        auto const kp  = td.file ("nudb.key");
        auto const kp2 = td.file ("nudb.key2");
        auto const lp  = td.file ("nudb.log");
        finisher f(
            [&]
            {
                erase_file(dp);
                erase_file(kp);
                erase_file(lp);
            });
        {
            error_code ec;
            Sequence seq;
            store db;
            create<xxhasher>(dp, kp, lp, appnumValue,
                saltValue, sizeof(key_type), blockSize,
                    loadFactor, ec);
            if(! expect(! ec, ec.message()))
                return;
            db.open(dp, kp, lp, arenaAllocSize, ec);
            if(! expect(! ec, ec.message()))
                return;
            Storage s;
            // Insert
            for(std::size_t i = 0; i < N; ++i)
            {
                auto const v = seq[i];
                auto const success =
                    db.insert(&v.key, v.data, v.size, ec);
                if(! expect(! ec, ec.message()))
                    return;
                expect(success);
            }
        }
        // Verify
        {
            error_code ec;
            verify_info info;
            verify<xxhasher>(
                info, dp, kp, bufferSize, no_progress{}, ec);
            if(! expect(! ec, ec.message()))
                return;
            if(! expect(info.value_count == N))
                return;
            if(! expect(info.spill_count > 0))
                return;
        }
        // Rekey
        for(std::size_t n = 1;; ++n)
        {
            error_code ec;
            fail_counter fc{n};
            rekey<xxhasher, fail_file<native_file>>(
                dp, kp2, lp, blockSize, loadFactor,
                    N, bufferSize, ec, no_progress{}, fc);
            if(! ec)
                break;
            if(! expect(ec == test::test_error::failure, ec.message()))
                return;
            ec = {};
            recover<xxhasher, native_file>(dp, kp2, lp, ec);
            if(ec == error::no_key_file)
                continue;
            if(! expect(! ec, ec.message()))
                return;
            native_file::erase(kp2, ec);
            if(ec == errc::no_such_file_or_directory)
                ec = {};
            if(! expect(! ec, ec.message()))
                return;
            verify_info info;
            verify<xxhasher>(
                info, dp, kp, bufferSize, no_progress{}, ec);
            if(! expect(! ec, ec.message()))
                return;
            if(! expect(info.value_count == N))
                return;
        }
        // Verify
        {
            error_code ec;
            verify_info info;
            verify<xxhasher>(
                info, dp, kp, bufferSize, no_progress{}, ec);
            if(! expect(! ec, ec.message()))
                return;
            if(! expect(info.value_count == N))
                return;
        }
        {
            error_code ec;
            verify_info info;
            verify<xxhasher>(
                info, dp, kp2, bufferSize, no_progress{}, ec);
            if(! expect(! ec, ec.message()))
                return;
            if(! expect(info.value_count == N))
                return;
        }
    }

    void
    run() override
    {
        enum
        {
            N =             50000,
            block_size =    256
        };

        float const load_factor = 0.95f;

        do_recover(N, block_size, load_factor);
    }
};

BEAST_DEFINE_TESTSUITE(rekey, test, nudb);

} // test
} // nudb
