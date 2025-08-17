// Clean node-based EDN representation with metadata & source positions
#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <map>
#include <optional>
#include <cstdint>
#include <cstdlib>

namespace edn {

struct parse_error : std::runtime_error { using std::runtime_error::runtime_error; };

struct keyword { std::string name; };
struct symbol  { std::string name; };
struct list; struct vector_t; struct set; struct map; struct tagged_value; struct node; // forward declarations

using node_ptr = std::shared_ptr<node>;

struct list      { std::vector<node_ptr> elems; };
struct vector_t  { std::vector<node_ptr> elems; };
struct set       { std::vector<node_ptr> elems; };
struct map       { std::vector<std::pair<node_ptr,node_ptr>> entries; };
struct tagged_value { symbol tag; node_ptr inner; };

using node_data = std::variant<std::monostate,bool,int64_t,double,std::string,keyword,symbol,list,vector_t,set,map,tagged_value>;

struct node { node_data data; std::map<std::string,node_ptr> metadata; };

namespace detail {
    struct reader {
        std::string_view d; size_t p=0; int line=1,col=1; int last_line=1,last_col=1;
        explicit reader(std::string_view s):d(s){}
        bool eof() const { return p>=d.size(); }
        char peek() const { return eof() ? '\0' : d[p]; }
        char get(){ if(eof()) return '\0'; last_line=line; last_col=col; char c=d[p++]; if(c=='\n'){ ++line; col=1; } else { ++col; } return c; }
        void skip_ws(){ while(!eof()){ char c=peek(); if(c==';'){ while(!eof() && get()!='\n'); continue; } if(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v'){ get(); continue; } break; } }
    };
    inline bool is_digit(char c){ return c>='0'&&c<='9'; }
    inline bool is_symbol_start(char c){ return std::isalpha((unsigned char)c)||c=='*'||c=='!'||c=='_'||c=='?'||c=='-'||c=='+'||c=='/'||c=='<'||c=='>'||c=='='||c=='$'||c=='%'||c=='&'; }
    inline bool is_symbol_char(char c){ return is_symbol_start(c)||is_digit(c)||c=='.'||c=='#'; }

    inline node_ptr make_node(node_data d){ return std::make_shared<node>(node{ std::move(d), {} }); }
    inline node_ptr make_int(int64_t v){ return make_node(node_data{v}); }
    inline void attach_pos(node& n,int sl,int sc,int el,int ec){ n.metadata["line"]=make_int(sl); n.metadata["col"]=make_int(sc); n.metadata["end-line"]=make_int(el); n.metadata["end-col"]=make_int(ec); }

    inline node_ptr parse_value(reader&);

    inline node_ptr parse_list_like(reader& r,char end,int sl,int sc,bool tagged_set=false){
        std::vector<node_ptr> elems; r.skip_ws(); while(!r.eof() && r.peek()!=end){ elems.push_back(parse_value(r)); r.skip_ws(); }
        if(r.get()!=end) throw parse_error("unterminated collection");
        node_ptr out;
        if(tagged_set){ set s; s.elems=std::move(elems); out=make_node(s);} else if(end==')'){ list l; l.elems=std::move(elems); out=make_node(l);} else if(end==']'){ vector_t v; v.elems=std::move(elems); out=make_node(v);} else if(end=='}'){ if(elems.size()%2) throw parse_error("map requires even number of forms"); map m; for(size_t i=0;i<elems.size();i+=2) m.entries.emplace_back(elems[i],elems[i+1]); out=make_node(m);} else { list l; out=make_node(l);} attach_pos(*out,sl,sc,r.last_line,r.last_col); return out; }





    inline node_ptr parse_string(reader& r){ int sl=r.line,sc=r.col; if(r.get()!='"') throw parse_error("expected \""); std::string out; while(!r.eof()){ char c=r.get(); if(c=='"') break; if(c=='\\'){ if(r.eof()) throw parse_error("bad escape"); char e=r.get(); switch(e){ case 'n': out+='\n'; break; case 'r': out+='\r'; break; case 't': out+='\t'; break; case '"': out+='"'; break; case '\\': out+='\\'; break; default: out+=e; break;} } else out+=c; } auto n=make_node(out); attach_pos(*n,sl,sc,r.last_line,r.last_col); return n; }

    inline node_ptr parse_number(reader& r){ int sl=r.line,sc=r.col; std::string num; if(r.peek()=='+'||r.peek()=='-') num+=r.get(); bool is_float=false; while(is_digit(r.peek())) num+=r.get(); if(r.peek()=='.'){ is_float=true; num+=r.get(); while(is_digit(r.peek())) num+=r.get(); } if(r.peek()=='e'||r.peek()=='E'){ is_float=true; num+=r.get(); if(r.peek()=='+'||r.peek()=='-') num+=r.get(); while(is_digit(r.peek())) num+=r.get(); } node_ptr n; try { if(is_float) n=make_node(std::stod(num)); else { long long v=std::stoll(num); n=make_node((int64_t)v);} } catch(...) { throw parse_error("invalid number"); } attach_pos(*n,sl,sc,r.last_line,r.last_col); return n; }

    inline node_ptr parse_symbol_or_keyword(reader& r){ int sl=r.line,sc=r.col; bool kw=false; if(r.peek()==':'){ kw=true; r.get(); } std::string s; while(is_symbol_char(r.peek())) s+=r.get(); node_ptr n; if(s=="nil" && !kw) n=make_node(std::monostate{}); else if(s=="true" && !kw) n=make_node(true); else if(s=="false" && !kw) n=make_node(false); else if(kw) n=make_node(keyword{s}); else n=make_node(symbol{s}); attach_pos(*n,sl,sc,r.last_line,r.last_col); return n; }

    inline node_ptr parse_tagged(reader& r){ int sl=r.line,sc=r.col; if(r.peek()=='{'){ r.get(); return parse_list_like(r,'}',sl,sc,true);} std::string tag; while(is_symbol_char(r.peek())) tag+=r.get(); r.skip_ws(); auto inner=parse_value(r); auto n=make_node(tagged_value{symbol{tag},inner}); attach_pos(*n,sl,sc,r.last_line,r.last_col); return n; }

    inline node_ptr parse_value(reader& r){ r.skip_ws(); char c=r.peek(); switch(c){ case '"': return parse_string(r); case '(': {int sl=r.line,sc=r.col; r.get(); return parse_list_like(r,')',sl,sc);} case '[': {int sl=r.line,sc=r.col; r.get(); return parse_list_like(r,']',sl,sc);} case '{': {int sl=r.line,sc=r.col; r.get(); return parse_list_like(r,'}',sl,sc);} case '#': { r.get(); return parse_tagged(r);} default: break; } if(is_digit(c)||c=='+'||c=='-') return parse_number(r); if(c==':'||is_symbol_start(c)) return parse_symbol_or_keyword(r); throw parse_error("unexpected character"); }
// Feature flags sourced from environment
inline bool env_flag_enabled(const char* name) {
    const char* v = std::getenv(name);
    return v && (v[0]=='1' || v[0]=='t' || v[0]=='T' || v[0]=='y' || v[0]=='Y');
}

}

inline node_ptr parse(std::string_view input){ detail::reader r(input); r.skip_ws(); auto v=detail::parse_value(r); r.skip_ws(); if(!r.eof()) throw parse_error("unexpected trailing characters"); return v; }

inline std::string to_string(const node& n); inline std::string to_string(const node_ptr& p){ return to_string(*p); }
inline std::string to_string(const node& n){ struct V { std::string operator()(std::monostate)const{return "nil";} std::string operator()(bool b)const{return b?"true":"false";} std::string operator()(int64_t i)const{return std::to_string(i);} std::string operator()(double d)const{ std::ostringstream oss; oss<<d; return oss.str(); } std::string operator()(const std::string& s)const{ return '"'+s+'"'; } std::string operator()(const keyword& k)const{ return ':'+k.name; } std::string operator()(const symbol& s)const{ return s.name; } std::string operator()(const list& l)const{ std::string out="("; bool first=true; for(auto& ch:l.elems){ if(!first) out+=' '; first=false; out+=to_string(ch); } out+=')'; return out;} std::string operator()(const vector_t& v)const{ std::string out="["; bool first=true; for(auto& ch:v.elems){ if(!first) out+=' '; first=false; out+=to_string(ch);} out+=']'; return out;} std::string operator()(const set& s)const{ std::string out="#{"; bool first=true; for(auto& ch:s.elems){ if(!first) out+=' '; first=false; out+=to_string(ch);} out+='}'; return out;} std::string operator()(const map& m)const{ std::string out="{"; bool first=true; for(auto& kv:m.entries){ if(!first) out+=' '; first=false; out+=to_string(kv.first)+' '+to_string(kv.second);} out+='}'; return out;} std::string operator()(const tagged_value& tv)const{ return '#'+tv.tag.name+' '+to_string(tv.inner); } }; return std::visit(V{}, n.data); }

inline bool is_symbol(const node& n){ return std::holds_alternative<symbol>(n.data); }
inline bool is_keyword(const node& n){ return std::holds_alternative<keyword>(n.data); }
inline bool is_list(const node& n){ return std::holds_alternative<list>(n.data); }
inline const list* as_list(const node& n){ return is_list(n)? &std::get<list>(n.data): nullptr; }
inline const symbol* as_symbol(const node& n){ return is_symbol(n)? &std::get<symbol>(n.data): nullptr; }
inline int meta_int(const node& n,const std::string& k,int def=-1){ auto it=n.metadata.find(k); if(it==n.metadata.end()) return def; auto& nd=*it->second; if(std::holds_alternative<int64_t>(nd.data)) return (int)std::get<int64_t>(nd.data); return def; }
inline int line(const node& n){ return meta_int(n,"line"); }
inline int col(const node& n){ return meta_int(n,"col"); }
inline int end_line(const node& n){ return meta_int(n,"end-line"); }
inline int end_col(const node& n){ return meta_int(n,"end-col"); }

} // namespace edn
