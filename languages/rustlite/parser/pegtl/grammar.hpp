#pragma once
#include <tao/pegtl.hpp>

namespace rustlite::pegtl_front::grammar {
using namespace tao::pegtl;

// Comments and whitespace
struct comment_line : seq< two<'/'>, until< eolf > > {};
struct block_comment : seq< one<'/'>, one<'*'>, until< seq< one<'*'>, one<'/'> > > > {};
struct space_or_comment : sor< space, comment_line, block_comment > {};

template<typename Rule>
using ws = pad< Rule, space_or_comment >;

// Minimal module rule for scaffold
struct module_rule : must< star< space_or_comment >, eof > {};

// TODO: progressively add tokens and statement rules here

} // namespace rustlite::pegtl_front::grammar
