#include "compat.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
namespace ofo {
static void utf8(std::string &out, unsigned cp) {
    if (cp < 128)
        out += char(cp);
    else if (cp < 2048) {
        out += char(0xc0 | (cp >> 6));
        out += char(0x80 | (cp & 63));
    } else {
        out += char(0xe0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 63));
        out += char(0x80 | (cp & 63));
    }
}
std::string text_decode(const Bytes &b) {
    std::string s(b.begin(), b.end());
    try {
        Json(s).dump();
        if (s.size() >= 3 && s.substr(0, 3) == "\xef\xbb\xbf")
            s.erase(0, 3);
        return s;
    } catch (...) {
        static const unsigned table[64] = {
            0x402,  0x403,  0x201a, 0x453,  0x201e, 0x2026, 0x2020, 0x2021, 0x20ac, 0x2030, 0x409,
            0x2039, 0x40a,  0x40c,  0x40b,  0x40f,  0x452,  0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
            0x2013, 0x2014, 0xfffd, 0x2122, 0x459,  0x203a, 0x45a,  0x45c,  0x45b,  0x45f,  0xa0,
            0x40e,  0x45e,  0x408,  0xa4,   0x490,  0xa6,   0xa7,   0x401,  0xa9,   0x404,  0xab,
            0xac,   0xad,   0xae,   0x407,  0xb0,   0xb1,   0x406,  0x456,  0x491,  0xb5,   0xb6,
            0xb7,   0x451,  0x2116, 0x454,  0xbb,   0x458,  0x405,  0x455,  0x457};
        s.clear();
        for (auto c : b)
            utf8(s, c < 128 ? c : c >= 192 ? 0x410 + c - 192 : table[c - 128]);
        return s;
    }
}
struct Token {
    char kind;
    std::string text;
};
class DataParser {
    std::vector<Token> tokens;
    size_t at = 0;
    [[noreturn]] void error(const std::string &s) {
        throw std::runtime_error("Data parser: " + s);
    }
    Token take() {
        if (at >= tokens.size())
            error("unexpected end");
        return tokens[at++];
    }
    void expect(const std::string &s) {
        if (take().text != s)
            error("expected " + s);
    }
    Json value(int depth) {
        if (depth > 80)
            error("nesting limit");
        auto t = take();
        if (t.kind == 'n') {
            try {
                size_t n = 0;
                double v = std::stod(t.text, &n);
                if (n != t.text.size() || !std::isfinite(v))
                    error("invalid number");
                return v;
            } catch (...) {
                error("non-finite or invalid number");
            }
        }
        if (t.kind == 's')
            return t.text;
        if (t.kind == 'w') {
            auto name = lower(t.text);
            if (name == "true")
                return true;
            if (name == "false")
                return false;
            if (t.text == "nil")
                return nullptr;
            return Json{{"symbol", t.text}};
        }
        if (t.text != "{")
            error("expected literal value");
        Json obj = Json::object();
        int index = 1;
        while (at < tokens.size() && tokens[at].text != "}") {
            std::string key;
            if (tokens[at].text == "[") {
                take();
                auto k = value(depth + 1);
                if (k.is_string())
                    key = k.get<std::string>();
                else if (k.is_number()) {
                    auto v = k.get<double>();
                    if (std::floor(v) != v)
                        error("non-integral key");
                    key = std::to_string((long long)v);
                } else
                    error("invalid key");
                expect("]");
                expect("=");
            } else if (at + 1 < tokens.size() && tokens[at].kind == 'w' &&
                       tokens[at + 1].text == "=") {
                key = take().text;
                take();
            } else
                key = std::to_string(index++);
            obj[key] = value(depth + 1);
            if (at < tokens.size() && (tokens[at].text == "," || tokens[at].text == ";"))
                take();
        }
        expect("}");
        return obj;
    }

  public:
    explicit DataParser(const std::string &s) {
        size_t p = 0;
        while (p < s.size()) {
            if (std::isspace((unsigned char)s[p])) {
                ++p;
                continue;
            }
            if (s.compare(p, 4, "--[[") == 0) {
                auto end = s.find("]]", p + 4);
                if (end == s.npos)
                    error("unterminated comment");
                p = end + 2;
                continue;
            }
            if (s[p] == '#' || s.compare(p, 2, "--") == 0) {
                auto end = s.find('\n', p);
                p = end == s.npos ? s.size() : end + 1;
                continue;
            }
            char c = s[p];
            if (c == '"' || c == '\'') {
                ++p;
                std::string out;
                bool closed = false;
                while (p < s.size()) {
                    char x = s[p++];
                    if (x == c) {
                        closed = true;
                        break;
                    }
                    if (x == '\\') {
                        if (p == s.size())
                            error("unterminated escape");
                        x = s[p++];
                        if (x >= '0' && x <= '9') {
                            unsigned n = x - '0';
                            int k = 1;
                            while (k < 3 && p < s.size() && std::isdigit((unsigned char)s[p])) {
                                n = n * 10 + (s[p++] - '0');
                                k++;
                            }
                            utf8(out, n);
                            continue;
                        }
                        if (x == 'n')
                            x = '\n';
                        else if (x == 'r')
                            x = '\r';
                        else if (x == 't')
                            x = '\t';
                    }
                    out += x;
                }
                if (!closed)
                    error("unterminated string");
                tokens.push_back({'s', out});
            } else if (std::isalpha((unsigned char)c) || c == '_') {
                size_t begin = p++;
                while (p < s.size() &&
                       (std::isalnum((unsigned char)s[p]) || s[p] == '_' || s[p] == '.'))
                    ++p;
                tokens.push_back({'w', s.substr(begin, p - begin)});
            } else if (std::isdigit((unsigned char)c) || c == '+' || c == '-' || c == '.') {
                size_t begin = p++;
                while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '.' ||
                                        s[p] == 'e' || s[p] == 'E' || s[p] == '+' || s[p] == '-'))
                    ++p;
                tokens.push_back({'n', s.substr(begin, p - begin)});
            } else if (std::string("{}[]=,;").find(c) != std::string::npos) {
                tokens.push_back({'p', std::string(1, c)});
                ++p;
            } else
                error("executable expression or unsupported syntax at byte " + std::to_string(p));
            if (tokens.size() > 300000)
                error("token limit");
        }
    }
    Json parse() {
        Json out = Json::object();
        while (at < tokens.size()) {
            auto key = take();
            if (key.kind != 'w')
                error("expected assignment");
            expect("=");
            out[key.text] = value(0);
            if (at < tokens.size() && (tokens[at].text == "," || tokens[at].text == ";"))
                take();
        }
        return out;
    }
};
Json parse_data(const std::string &s) {
    return DataParser(s).parse();
}

}
