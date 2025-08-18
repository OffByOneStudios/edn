#include <cassert>
#include <string>
#include <tao/pegtl.hpp>

namespace pegtl = tao::pegtl;

// Minimal grammar: parse the exact word "edn" and end of input
struct edn_word : pegtl::string<'e','d','n'> {};
struct grammar : pegtl::must< edn_word, pegtl::eof > {};

void run_phase5_pegtl_smoke_test(){
    {
        pegtl::memory_input in("edn", "pegtl-smoke");
        const bool ok = pegtl::parse< grammar >(in);
        assert(ok && "PEGTL should parse 'edn'");
    }
    {
        pegtl::memory_input in("edn!", "pegtl-smoke-bad");
        bool threw = false;
        try {
            (void)pegtl::parse< grammar >(in);
        } catch (const pegtl::parse_error&) {
            threw = true; // expected: not EOF
        }
        assert(threw && "PEGTL should reject trailing characters");
    }
}
