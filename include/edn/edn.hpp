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
#include <functional>

namespace edn
{

    struct parse_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct keyword
    {
        std::string name;
    };
    struct symbol
    {
        std::string name;
    };
    struct list;
    struct vector_t;
    struct set;
    struct map;
    struct tagged_value;
    struct node; // forward declarations

    using node_ptr = std::shared_ptr<node>;

    struct list
    {
        std::vector<node_ptr> elems;
    };
    struct vector_t
    {
        std::vector<node_ptr> elems;
    };
    struct set
    {
        std::vector<node_ptr> elems;
    };
    struct map
    {
        std::vector<std::pair<node_ptr, node_ptr>> entries;
    };
    struct tagged_value
    {
        symbol tag;
        node_ptr inner;
    };

    using node_data = std::variant<std::monostate, bool, int64_t, double, std::string, keyword, symbol, list, vector_t, set, map, tagged_value>;

    struct node
    {
        node_data data;
        std::map<std::string, node_ptr> metadata;
    };

    // Parse a single EDN form (entire input) into a node tree.
    node_ptr parse_one(std::string_view src);

    // Structural deep equality of two EDN nodes. If ignore_metadata is true, metadata maps are ignored.
    bool equal(const node_ptr& a, const node_ptr& b, bool ignore_metadata = true);

    namespace detail
    {
        struct reader
        {
            std::string_view d;
            size_t p = 0;
            int line = 1, col = 1;
            int last_line = 1, last_col = 1;
            explicit reader(std::string_view s) : d(s) {}
            bool eof() const { return p >= d.size(); }
            char peek() const { return eof() ? '\0' : d[p]; }
            char get()
            {
                if (eof())
                    return '\0';
                last_line = line;
                last_col = col;
                char c = d[p++];
                if (c == '\n')
                {
                    ++line;
                    col = 1;
                }
                else
                {
                    ++col;
                }
                return c;
            }
            void skip_ws()
            {
                while (!eof())
                {
                    char c = peek();
                    if (c == ';')
                    {
                        while (!eof() && get() != '\n')
                            continue;
                    }
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')
                    {
                        get();
                        continue;
                    }
                    break;
                }
            }
        };
        inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
        inline bool is_symbol_start(char c) { return std::isalpha((unsigned char)c) || c == '*' || c == '!' || c == '_' || c == '?' || c == '-' || c == '+' || c == '/' || c == '<' || c == '>' || c == '=' || c == '$' || c == '%' || c == '&'; }
        inline bool is_symbol_char(char c) { return is_symbol_start(c) || is_digit(c) || c == '.' || c == '#'; }

        inline node_ptr make_node(node_data d) { return std::make_shared<node>(node{std::move(d), {}}); }
        inline node_ptr make_int(int64_t v) { return make_node(node_data{v}); }
        inline void attach_pos(node &n, int sl, int sc, int el, int ec)
        {
            n.metadata["line"] = make_int(sl);
            n.metadata["col"] = make_int(sc);
            n.metadata["end-line"] = make_int(el);
            n.metadata["end-col"] = make_int(ec);
        }

        inline node_ptr parse_value(reader &);

        inline node_ptr parse_list_like(reader &r, char end, int sl, int sc, bool tagged_set = false)
        {
            std::vector<node_ptr> elems;
            r.skip_ws();
            while (!r.eof() && r.peek() != end)
            {
                elems.push_back(parse_value(r));
                r.skip_ws();
            }
            if (r.get() != end)
                throw parse_error("unterminated collection");
            node_ptr out;
            if (tagged_set)
            {
                set s;
                s.elems = std::move(elems);
                out = make_node(s);
            }
            else if (end == ')')
            {
                list l;
                l.elems = std::move(elems);
                out = make_node(l);
            }
            else if (end == ']')
            {
                vector_t v;
                v.elems = std::move(elems);
                out = make_node(v);
            }
            else if (end == '}')
            {
                if (elems.size() % 2)
                    throw parse_error("map requires even number of forms");
                map m;
                for (size_t i = 0; i < elems.size(); i += 2)
                    m.entries.emplace_back(elems[i], elems[i + 1]);
                out = make_node(m);
            }
            else
            {
                list l;
                out = make_node(l);
            }
            attach_pos(*out, sl, sc, r.last_line, r.last_col);
            return out;
        }

        inline node_ptr parse_string(reader &r)
        {
            int sl = r.line, sc = r.col;
            if (r.get() != '"')
                throw parse_error("expected \"");
            std::string out;
            while (!r.eof())
            {
                char c = r.get();
                if (c == '"')
                    break;
                if (c == '\\')
                {
                    if (r.eof())
                        throw parse_error("bad escape");
                    char e = r.get();
                    switch (e)
                    {
                    case 'n':
                        out += '\n';
                        break;
                    case 'r':
                        out += '\r';
                        break;
                    case 't':
                        out += '\t';
                        break;
                    case '"':
                        out += '"';
                        break;
                    case '\\':
                        out += '\\';
                        break;
                    default:
                        out += e;
                        break;
                    }
                }
                else
                    out += c;
            }
            auto n = make_node(out);
            attach_pos(*n, sl, sc, r.last_line, r.last_col);
            return n;
        }

        inline node_ptr parse_number(reader &r)
        {
            int sl = r.line, sc = r.col;
            std::string num;
            if (r.peek() == '+' || r.peek() == '-')
                num += r.get();
            bool is_float = false;
            while (is_digit(r.peek()))
                num += r.get();
            if (r.peek() == '.')
            {
                is_float = true;
                num += r.get();
                while (is_digit(r.peek()))
                    num += r.get();
            }
            if (r.peek() == 'e' || r.peek() == 'E')
            {
                is_float = true;
                num += r.get();
                if (r.peek() == '+' || r.peek() == '-')
                    num += r.get();
                while (is_digit(r.peek()))
                    num += r.get();
            }
            node_ptr n;
            try
            {
                if (is_float)
                    n = make_node(std::stod(num));
                else
                {
                    long long v = std::stoll(num);
                    n = make_node((int64_t)v);
                }
            }
            catch (...)
            {
                throw parse_error("invalid number");
            }
            attach_pos(*n, sl, sc, r.last_line, r.last_col);
            return n;
        }

        inline node_ptr parse_symbol_or_keyword(reader &r)
        {
            int sl = r.line, sc = r.col;
            bool kw = false;
            if (r.peek() == ':')
            {
                kw = true;
                r.get();
            }
            std::string s;
            while (is_symbol_char(r.peek()))
                s += r.get();
            node_ptr n;
            if (s == "nil" && !kw)
                n = make_node(std::monostate{});
            else if (s == "true" && !kw)
                n = make_node(true);
            else if (s == "false" && !kw)
                n = make_node(false);
            else if (kw)
                n = make_node(keyword{s});
            else
                n = make_node(symbol{s});
            attach_pos(*n, sl, sc, r.last_line, r.last_col);
            return n;
        }

        inline node_ptr parse_tagged(reader &r)
        {
            int sl = r.line, sc = r.col;
            if (r.peek() == '{')
            {
                r.get();
                return parse_list_like(r, '}', sl, sc, true);
            }
            std::string tag;
            while (is_symbol_char(r.peek()))
                tag += r.get();
            r.skip_ws();
            auto inner = parse_value(r);
            auto n = make_node(tagged_value{symbol{tag}, inner});
            attach_pos(*n, sl, sc, r.last_line, r.last_col);
            return n;
        }

        inline node_ptr parse_value(reader &r)
        {
            r.skip_ws();
            char c = r.peek();
            switch (c)
            {
            case '"':
                return parse_string(r);
            case '(':
            {
                int sl = r.line, sc = r.col;
                r.get();
                return parse_list_like(r, ')', sl, sc);
            }
            case '[':
            {
                int sl = r.line, sc = r.col;
                r.get();
                return parse_list_like(r, ']', sl, sc);
            }
            case '{':
            {
                int sl = r.line, sc = r.col;
                r.get();
                return parse_list_like(r, '}', sl, sc);
            }
            case '#':
            {
                r.get();
                return parse_tagged(r);
            }
            default:
                break;
            }
            if (is_digit(c) || c == '+' || c == '-')
                return parse_number(r);
            if (c == ':' || is_symbol_start(c))
                return parse_symbol_or_keyword(r);
            throw parse_error("unexpected character");
        }
        // Feature flags sourced from environment
        inline bool env_flag_enabled(const char *name)
        {
            const char *v = std::getenv(name);
            return v && (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y');
        }

    }

    inline node_ptr parse(std::string_view input)
    {
        detail::reader r(input);
        r.skip_ws();
        auto v = detail::parse_value(r);
        r.skip_ws();
        if (!r.eof())
            throw parse_error("unexpected trailing characters");
        return v;
    }

    inline std::string to_string(const node &n);
    inline std::string to_string(const node_ptr &p) { return to_string(*p); }
    // Pretty printer with newlines and indentation for readability
    inline std::string to_pretty_string(const node &n, int indentWidth = 2);
    inline std::string to_pretty_string(const node_ptr &p, int indentWidth = 2) { return to_pretty_string(*p, indentWidth); }
    inline std::string to_string(const node &n)
    {
        struct V
        {
            std::string operator()(std::monostate) const { return "nil"; }
            std::string operator()(bool b) const { return b ? "true" : "false"; }
            std::string operator()(int64_t i) const { return std::to_string(i); }
            std::string operator()(double d) const
            {
                std::ostringstream oss;
                oss << d;
                return oss.str();
            }
            std::string operator()(const std::string &s) const { return '"' + s + '"'; }
            std::string operator()(const keyword &k) const { return ':' + k.name; }
            std::string operator()(const symbol &s) const { return s.name; }
            std::string operator()(const list &l) const
            {
                std::string out = "(";
                bool first = true;
                for (auto &ch : l.elems)
                {
                    if (!first)
                        out += ' ';
                    first = false;
                    out += to_string(ch);
                }
                out += ')';
                return out;
            }
            std::string operator()(const vector_t &v) const
            {
                std::string out = "[";
                bool first = true;
                for (auto &ch : v.elems)
                {
                    if (!first)
                        out += ' ';
                    first = false;
                    out += to_string(ch);
                }
                out += ']';
                return out;
            }
            std::string operator()(const set &s) const
            {
                std::string out = "#{";
                bool first = true;
                for (auto &ch : s.elems)
                {
                    if (!first)
                        out += ' ';
                    first = false;
                    out += to_string(ch);
                }
                out += '}';
                return out;
            }
            std::string operator()(const map &m) const
            {
                std::string out = "{";
                bool first = true;
                for (auto &kv : m.entries)
                {
                    if (!first)
                        out += ' ';
                    first = false;
                    out += to_string(kv.first) + ' ' + to_string(kv.second);
                }
                out += '}';
                return out;
            }
            std::string operator()(const tagged_value &tv) const { return '#' + tv.tag.name + ' ' + to_string(tv.inner); }
        };
        return std::visit(V{}, n.data);
    }

    inline std::string to_pretty_string(const node &n, int indentWidth)
    {
        // Strategy: produce compact single-line forms for simple/short collections.
        // Only introduce newlines for: nested collections, long lines, or control-ish forms
        // (e.g., lists whose head symbol is one of a known set or which contain keyword sections).

        auto indentStr = [](int spaces) -> std::string {
            if (spaces < 0) spaces = 0;
            return std::string(static_cast<size_t>(spaces), ' ');
        };

        auto is_atomic = [](const node &x) -> bool {
            return std::holds_alternative<std::monostate>(x.data) ||
                   std::holds_alternative<bool>(x.data) ||
                   std::holds_alternative<int64_t>(x.data) ||
                   std::holds_alternative<double>(x.data) ||
                   std::holds_alternative<std::string>(x.data) ||
                   std::holds_alternative<keyword>(x.data) ||
                   std::holds_alternative<symbol>(x.data);
        };

        const size_t MAX_INLINE_LEN = 90; // heuristic

        std::function<std::string(const node&, int)> pp = [&](const node &x, int indent) -> std::string {
            // Scalars
            if (std::holds_alternative<std::monostate>(x.data)) return "nil";
            if (std::holds_alternative<bool>(x.data)) return std::get<bool>(x.data)?"true":"false";
            if (std::holds_alternative<int64_t>(x.data)) return std::to_string(std::get<int64_t>(x.data));
            if (std::holds_alternative<double>(x.data)) { std::ostringstream oss; oss<<std::get<double>(x.data); return oss.str(); }
            if (std::holds_alternative<std::string>(x.data)) return '"'+std::get<std::string>(x.data)+'"';
            if (std::holds_alternative<keyword>(x.data)) return ':'+std::get<keyword>(x.data).name;
            if (std::holds_alternative<symbol>(x.data)) return std::get<symbol>(x.data).name;

            auto join_inline_list = [&](const std::vector<node_ptr>& elems, char openC, char closeC)->std::string {
                std::string out; out+=openC; bool first=true; for(auto &e: elems){ if(!first) out+=' '; first=false; out+=pp(*e, indent); if(out.size()>MAX_INLINE_LEN) return std::string(); } out+=closeC; return out; };

            // list
            if (std::holds_alternative<list>(x.data)) {
                const auto &elems = std::get<list>(x.data).elems;
                if (elems.empty()) return "()";
                bool allAtomic = true; for(auto &e: elems){ if(!is_atomic(*e)){ allAtomic=false; break;} }
                // Determine if head symbol suggests multiline (control / fn / module)
                bool forceMulti = false;
                if(!elems.empty() && std::holds_alternative<symbol>(elems[0]->data)){
                    static const char* controlSyms[] = {"module","fn","if","while","for","switch","match","block","rif","rwhile","rloop"};
                    std::string head = std::get<symbol>(elems[0]->data).name;
                    for(auto s: controlSyms){ if(head==s){ forceMulti=true; break; } }
                }
                if(allAtomic && !forceMulti){
                    auto inlineForm = join_inline_list(elems,'(',')');
                    if(!inlineForm.empty()) return inlineForm; // fits single line
                }
                // Multiline fallback
                std::string out="(\n"; size_t i=0; for(auto &e: elems){
                    out += indentStr(indent + indentWidth) + pp(*e, indent + indentWidth);
                    if(++i<elems.size()) out += '\n';
                }
                out += '\n' + indentStr(indent) + ')';
                return out;
            }
            // vector
            if (std::holds_alternative<vector_t>(x.data)) {
                const auto &elems = std::get<vector_t>(x.data).elems;
                if (elems.empty()) return "[]";
                bool allAtomic=true; for(auto &e: elems){ if(!is_atomic(*e)){ allAtomic=false; break;} }
                if(allAtomic){ auto inlineForm = join_inline_list(elems,'[',']'); if(!inlineForm.empty()) return inlineForm; }
                std::string out="[\n"; size_t i=0; for(auto &e: elems){ out += indentStr(indent+indentWidth)+pp(*e, indent+indentWidth); if(++i<elems.size()) out+='\n'; }
                out += '\n'+indentStr(indent)+']'; return out;
            }
            // set
            if (std::holds_alternative<set>(x.data)) {
                const auto &elems = std::get<set>(x.data).elems; if(elems.empty()) return "#{}"; bool allAtomic=true; for(auto &e: elems){ if(!is_atomic(*e)){ allAtomic=false; break;} }
                if(allAtomic){ auto inlineForm = join_inline_list(elems,'{','}'); if(!inlineForm.empty()) return std::string("#")+inlineForm; }
                std::string out = "#{\n"; size_t i=0; for(auto &e: elems){ out += indentStr(indent+indentWidth)+pp(*e, indent+indentWidth); if(++i<elems.size()) out+='\n'; }
                out += '\n'+indentStr(indent)+'}'; return out;
            }
            // map
            if (std::holds_alternative<map>(x.data)) {
                const auto &entries = std::get<map>(x.data).entries; if(entries.empty()) return "{}";
                bool allAtomic=true; for(auto &kv: entries){ if(!is_atomic(*kv.first) || !is_atomic(*kv.second)){ allAtomic=false; break;} }
                if(allAtomic){
                    std::string inlineOut = "{"; bool first=true; for(auto &kv: entries){ if(!first) inlineOut+=' '; first=false; inlineOut+=pp(*kv.first, indent)+' '+pp(*kv.second, indent); if(inlineOut.size()>MAX_INLINE_LEN) { inlineOut.clear(); break; } }
                    if(!inlineOut.empty()){ inlineOut+='}'; return inlineOut; }
                }
                std::string out="{\n"; size_t i=0; for(auto &kv: entries){ out += indentStr(indent+indentWidth)+pp(*kv.first, indent+indentWidth)+' '+pp(*kv.second, indent+indentWidth); if(++i<entries.size()) out+='\n'; }
                out += '\n'+indentStr(indent)+'}'; return out;
            }
            if (std::holds_alternative<tagged_value>(x.data)) {
                const auto &tv = std::get<tagged_value>(x.data); return std::string("#")+tv.tag.name+" "+pp(*tv.inner, indent); }
            return std::string("<unknown>");
        };
        return pp(n, 0);
    }

    inline bool is_symbol(const node &n) { return std::holds_alternative<symbol>(n.data); }
    inline bool is_keyword(const node &n) { return std::holds_alternative<keyword>(n.data); }
    inline bool is_list(const node &n) { return std::holds_alternative<list>(n.data); }
    inline const list *as_list(const node &n) { return is_list(n) ? &std::get<list>(n.data) : nullptr; }
    inline const symbol *as_symbol(const node &n) { return is_symbol(n) ? &std::get<symbol>(n.data) : nullptr; }
    inline int meta_int(const node &n, const std::string &k, int def = -1)
    {
        auto it = n.metadata.find(k);
        if (it == n.metadata.end())
            return def;
        auto &nd = *it->second;
        if (std::holds_alternative<int64_t>(nd.data))
            return (int)std::get<int64_t>(nd.data);
        return def;
    }
    inline int line(const node &n) { return meta_int(n, "line"); }
    inline int col(const node &n) { return meta_int(n, "col"); }
    inline int end_line(const node &n) { return meta_int(n, "end-line"); }
    inline int end_col(const node &n) { return meta_int(n, "end-col"); }

    // ------ Ergonomic helpers and insertion operators ------

    // Factory helpers (prefixed to avoid colliding with existing helpers in tests/expanders)
    inline node_ptr n_sym(std::string name) { return detail::make_node(symbol{std::move(name)}); }
    inline node_ptr n_kw(std::string name) { return detail::make_node(keyword{std::move(name)}); }
    inline node_ptr n_str(std::string s) { return detail::make_node(std::move(s)); }
    inline node_ptr n_i64(int64_t v) { return detail::make_node(v); }
    inline node_ptr n_f64(double v) { return detail::make_node(v); }
    inline node_ptr n_bool(bool b) { return detail::make_node(b); }

    inline node_ptr node_list() { return detail::make_node(list{}); }
    inline node_ptr node_vec() { return detail::make_node(vector_t{}); }
    inline node_ptr node_set() { return detail::make_node(set{}); }
    inline node_ptr node_map() { return detail::make_node(map{}); }

    inline node_ptr node_list(std::initializer_list<node_ptr> xs)
    {
        list l;
        l.elems.assign(xs.begin(), xs.end());
        return detail::make_node(std::move(l));
    }
    inline node_ptr node_vec(std::initializer_list<node_ptr> xs)
    {
        vector_t v;
        v.elems.assign(xs.begin(), xs.end());
        return detail::make_node(std::move(v));
    }
    inline node_ptr node_set(std::initializer_list<node_ptr> xs)
    {
        set s;
        s.elems.assign(xs.begin(), xs.end());
        return detail::make_node(std::move(s));
    }
    inline node_ptr node_map(std::initializer_list<std::pair<node_ptr, node_ptr>> xs)
    {
        map m;
        m.entries.assign(xs.begin(), xs.end());
        return detail::make_node(std::move(m));
    }

    inline std::pair<node_ptr, node_ptr> kvp(node_ptr k, node_ptr v) { return {std::move(k), std::move(v)}; }

    // Append operators for collection types
    inline list &operator<<(list &l, const node_ptr &n)
    {
        l.elems.push_back(n);
        return l;
    }
    inline vector_t &operator<<(vector_t &v, const node_ptr &n)
    {
        v.elems.push_back(n);
        return v;
    }
    inline set &operator<<(set &s, const node_ptr &n)
    {
        s.elems.push_back(n);
        return s;
    }
    inline map &operator<<(map &m, const std::pair<node_ptr, node_ptr> &kv)
    {
        m.entries.push_back(kv);
        return m;
    }

    // Generic appender for node_ptr collections
    inline node_ptr &operator<<(node_ptr &c, const node_ptr &n)
    {
        if (!c)
            throw std::invalid_argument("operator<<: null container node");
        if (std::holds_alternative<list>(c->data))
        {
            std::get<list>(c->data).elems.push_back(n);
        }
        else if (std::holds_alternative<vector_t>(c->data))
        {
            std::get<vector_t>(c->data).elems.push_back(n);
        }
        else if (std::holds_alternative<set>(c->data))
        {
            std::get<set>(c->data).elems.push_back(n);
        }
        else
        {
            throw std::invalid_argument("operator<<: container is not list/vector/set");
        }
        return c;
    }
    inline node_ptr &operator<<(node_ptr &c, const std::pair<node_ptr, node_ptr> &kv)
    {
        if (!c)
            throw std::invalid_argument("operator<<: null container node");
        if (std::holds_alternative<map>(c->data))
        {
            std::get<map>(c->data).entries.push_back(kv);
        }
        else
        {
            throw std::invalid_argument("operator<<: container is not a map");
        }
        return c;
    }

} // namespace edn
