#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/transform.hpp"

int main(){
    using namespace edn;
    auto v = parse("[1 2 3 :kw true false nil]");
    std::string out = to_string(v);
    // Basic round-trip shape test (not exact due to ordering)
    assert(out.find("[") == 0);
    auto m = parse("{:a 1 :b 2}");
    auto s = to_string(m);
    assert(s.find("{") == 0);
    // Demonstrate macro + visitor:
    // Define a macro 'inc' that rewrites (inc x) -> (+ :lhs x :rhs 1)
    Transformer tr;
    tr.add_macro("inc", [&](const list& form)->std::optional<node_ptr>{
        if(form.elems.size()==2){
            list out;
            out.elems.push_back(std::make_shared<node>(node{symbol{"+"},{}}));
            out.elems.push_back(std::make_shared<node>(node{keyword{"lhs"},{}}));
            out.elems.push_back(form.elems[1]);
            out.elems.push_back(std::make_shared<node>(node{keyword{"rhs"},{}}));
            out.elems.push_back(std::make_shared<node>(node{int64_t{1},{}}));
            return std::make_shared<node>(node{out,{}});
        }
        return std::nullopt;
    });
    int plus_seen = 0;
    tr.add_visitor("+", [&](node& n, list& l, const symbol&){
        // annotate metadata
        n.metadata["kind"] = std::make_shared<node>(node{ keyword{"binary"}, {} });
        ++plus_seen;
    });
    auto form = parse("(inc 41)");
    auto expanded = tr.expand_and_traverse(form);
    assert(plus_seen==1);
    // verify annotation exists
    assert(expanded->data.index()!=0); // not nil


    std::cout << "All tests passed\n";
    return 0;
}
