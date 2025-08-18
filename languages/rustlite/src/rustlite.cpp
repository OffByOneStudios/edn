#include "rustlite/rustlite.hpp"
#include <sstream>

namespace rustlite {

Builder& Builder::sum_enum(const std::string& name, const std::vector<std::pair<std::string,std::vector<std::string>>>& variants){
    std::ostringstream os; os << "(sum :name " << name << " :variants [ "; bool first=true; for(auto &v : variants){ if(!first) os << ' '; first=false; os << "(variant :name " << v.first << " :fields ["; bool firstf=true; for(auto &f : v.second){ if(!firstf) os << ' '; firstf=false; os << ' ' << f; } os << " ])"; } os << " ]) ";
    edn_ += os.str(); return *this;
}

Builder& Builder::fn_raw(const std::string& name, const std::string& ret_type, const std::vector<std::pair<std::string,std::string>>& params, const std::string& body_ir_vec){
    std::ostringstream os; os << "(fn :name \"" << name << "\" :ret " << ret_type << " :params [ "; bool first=true; for(auto &p : params){ if(!first) os << ' '; first=false; os << "(param " << p.second << " %" << p.first << ")"; } os << " ] :body " << body_ir_vec << ") ";
    edn_ += os.str(); return *this;
}

Builder& Builder::rstruct(const std::string& name, const std::vector<std::pair<std::string,std::string>>& fields){
    std::ostringstream os; os << "(rstruct :name \"" << name << "\" :fields [ "; bool first=true; for(auto &f : fields){ if(!first) os << ' '; first=false; os << "(" << f.first << ' ' << f.second << ")"; } os << " ]) ";
    edn_ += os.str(); return *this;
}

} // namespace rustlite
