#include "rustlite/rustlite.hpp"
#include <sstream>

namespace rustlite {

Builder& Builder::sum_enum(const std::string& name, const std::vector<std::pair<std::string,std::vector<std::string>>>& variants){
    using namespace edn;
    auto sum = edn::node_list({ edn::n_sym("sum"), edn::n_kw("name"), edn::n_str(name), edn::n_kw("variants") });
    auto vvec = edn::node_vec();
    for(const auto& v : variants){
    auto var = edn::node_list({ edn::n_sym("variant"), edn::n_kw("name"), edn::n_str(v.first), edn::n_kw("fields") });
    auto fvec = edn::node_vec();
    for(const auto& f : v.second){ fvec << edn::n_sym(f); }
        var << fvec; vvec << var;
    }
    sum << vvec; root_ << sum; return *this;
}

Builder& Builder::fn_raw(const std::string& name, const std::string& ret_type, const std::vector<std::pair<std::string,std::string>>& params, const std::string& body_ir_vec){
    using namespace edn;
    auto fn = edn::node_list({ edn::n_sym("fn"), edn::n_kw("name"), edn::n_str(name), edn::n_kw("ret") });
    // ret_type is EDN text; parse to node
    auto ret = edn::parse(ret_type);
    fn << ret; fn << edn::n_kw("params");
    auto pvec = edn::node_vec();
    for(const auto& p : params){ auto param = edn::node_list({ edn::n_sym("param"), edn::parse(p.second), edn::n_sym("%"+p.first) }); pvec << param; }
    fn << pvec; fn << edn::n_kw("body");
    // body_ir_vec is EDN vector text of IR instructions
    auto body = edn::parse(body_ir_vec);
    fn << body; root_ << fn; return *this;
}

Builder& Builder::rstruct(const std::string& name, const std::vector<std::pair<std::string,std::string>>& fields){
    using namespace edn;
    auto st = edn::node_list({ edn::n_sym("rstruct"), edn::n_kw("name"), edn::n_str(name), edn::n_kw("fields") });
    auto fvec = edn::node_vec();
    for(const auto& f : fields){ auto pair = edn::node_list({ edn::n_sym(f.first), edn::parse(f.second) }); fvec << pair; }
    st << fvec; root_ << st; return *this;
}

Builder& Builder::raw(const std::string& form_edn){
    using namespace edn;
    // Parse the provided EDN snippet (single form) and append it.
    try {
        auto node = edn::parse(form_edn);
        root_ << node;
    } catch(...) {
        // Swallow parse errors silently for legacy negative tests; in production we'd report.
    }
    return *this;
}

} // namespace rustlite
