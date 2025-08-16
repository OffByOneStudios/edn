#include "edn/diagnostics_json.hpp"
#include <sstream>
#include <cstdlib>
#include <cstdio>

namespace edn {

std::string json_escape(const std::string& s){
    std::ostringstream o; o<<'"';
    for(char c: s){
        switch(c){
            case '"': o<<"\\\""; break; case '\\': o<<"\\\\"; break;
            case '\n': o<<"\\n"; break; case '\r': o<<"\\r"; break; case '\t': o<<"\\t"; break;
            default:
                if(static_cast<unsigned char>(c) < 0x20){ char buf[7]; std::snprintf(buf,sizeof(buf),"\\u%04X", (unsigned char)c); o<<buf; }
                else { o<<c; }
                break;
        }
    }
    o<<'"';
    return o.str();
}

static void append_notes_json(std::ostringstream& os, const std::vector<TypeNote>& notes){
    os<<"[";
    for(size_t i=0;i<notes.size(); ++i){
        if(i) os<<",";
        os<<"{\"message\":"<<json_escape(notes[i].message)
          <<",\"line\":"<<notes[i].line
          <<",\"col\":"<<notes[i].col
          <<"}";
    }
    os<<"]";
}

std::string diagnostics_to_json(const TypeCheckResult& r){
    std::ostringstream os;
    os<<"{\"success\":"<<(r.success?"true":"false")<<",\"errors\":[";
    for(size_t i=0;i<r.errors.size(); ++i){
        const auto &e=r.errors[i]; if(i) os<<",";
        os<<"{"
            "\"code\":"<<json_escape(e.code)
            <<",\"message\":"<<json_escape(e.message)
            <<",\"hint\":"<<json_escape(e.hint)
            <<",\"line\":"<<e.line
            <<",\"col\":"<<e.col
            <<",\"notes\":";
        append_notes_json(os,e.notes);
        os<<"}";
    }
    os<<"],\"warnings\":[";
    for(size_t i=0;i<r.warnings.size(); ++i){
        const auto &w=r.warnings[i]; if(i) os<<",";
        os<<"{"
            "\"code\":"<<json_escape(w.code)
            <<",\"message\":"<<json_escape(w.message)
            <<",\"hint\":"<<json_escape(w.hint)
            <<",\"line\":"<<w.line
            <<",\"col\":"<<w.col
            <<",\"notes\":";
        append_notes_json(os,w.notes);
        os<<"}";
    }
    os<<"]}";
    return os.str();
}

void maybe_print_json(const TypeCheckResult& r){
    if(const char* env = std::getenv("EDN_DIAG_JSON")){
        if(env[0]=='1'){
            auto js=diagnostics_to_json(r);
            std::fprintf(stderr, "%s\n", js.c_str());
        }
    }
}

} // namespace edn
