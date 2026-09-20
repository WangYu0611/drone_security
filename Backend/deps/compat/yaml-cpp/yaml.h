#pragma once

#include <cctype>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <iterator>
#include <utility>
#include <vector>

namespace YAML {

class Exception : public std::runtime_error
{
public:
    explicit Exception(const std::string& msg) : std::runtime_error(msg) {}
};

class Node
{
public:
    enum class Kind { Undefined, Scalar, Sequence, Map };
    using Sequence = std::vector<std::shared_ptr<Node>>;
    using Map = std::vector<std::pair<std::string, std::shared_ptr<Node>>>;

    struct NodePairIterator
    {
        using It = Map::const_iterator;
        It it;

        struct Pair
        {
            Node first;
            Node second;
        };

        Pair operator*() const
        {
            return Pair{
                Node(it->first),
                it->second ? *it->second : Node{}
            };
        }

        NodePairIterator& operator++()
        {
            ++it;
            return *this;
        }

        bool operator!=(const NodePairIterator& other) const { return it != other.it; }
    };

    Node() = default;
    explicit Node(std::string scalar) : kind_(Kind::Scalar), scalar_(std::move(scalar)) {}

    static Node SequenceNode(Sequence seq)
    {
        Node n;
        n.kind_ = Kind::Sequence;
        n.sequence_ = std::move(seq);
        return n;
    }

    static Node MapNode(Map map)
    {
        Node n;
        n.kind_ = Kind::Map;
        n.map_ = std::move(map);
        return n;
    }

    bool IsSequence() const { return kind_ == Kind::Sequence; }
    bool IsMap() const { return kind_ == Kind::Map; }
    bool IsDefined() const { return kind_ != Kind::Undefined; }
    explicit operator bool() const { return IsDefined(); }
    std::size_t size() const
    {
        if (IsSequence()) return sequence_.size();
        if (IsMap()) return map_.size();
        return 0;
    }

    Node operator[](const char* key) const
    {
        if (!IsMap()) return {};
        for (const auto& entry : map_) {
            if (entry.first == key) {
                return entry.second ? *entry.second : Node{};
            }
        }
        return {};
    }

    Node operator[](size_t index) const
    {
        if (!IsSequence() || index >= sequence_.size()) return {};
        const auto& node = sequence_[index];
        return node ? *node : Node{};
    }

    template <typename T>
    T as(const T& fallback) const
    {
        if (kind_ != Kind::Scalar) return fallback;
        return ParseAs<T>(scalar_, fallback);
    }

    template <typename T>
    T as() const
    {
        return as(T{});
    }

    NodePairIterator begin() const { return NodePairIterator{ map_.begin() }; }
    NodePairIterator end() const { return NodePairIterator{ map_.end() }; }
    static std::string TrimValue(const std::string& value) { return Trim(value); }

private:
    static std::string Trim(const std::string& value)
    {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
        return value.substr(start, end - start);
    }

    template <typename T>
    static T ParseAs(const std::string& text, const T& fallback)
    {
        try {
            const std::string v = Trim(Unquote(text));
            if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, bool>) {
                std::string l = v;
                for (auto& ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (l == "true" || l == "1" || l == "yes" || l == "on") return true;
                if (l == "false" || l == "0" || l == "no" || l == "off") return false;
                return fallback;
            } else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_signed_v<T>) {
                    return static_cast<T>(std::stoll(v));
                } else {
                    return static_cast<T>(std::stoull(v));
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(std::stod(v));
            } else {
                return fallback;
            }
        } catch (...) {
            return fallback;
        }
    }

    static std::string Unquote(const std::string& text)
    {
        if (text.size() >= 2) {
            if ((text.front() == '\"' && text.back() == '\"') ||
                (text.front() == '\'' && text.back() == '\'')) {
                return text.substr(1, text.size() - 2);
            }
        }
        return text;
    }

    Kind kind_ = Kind::Undefined;
    std::string scalar_;
    Sequence sequence_;
    Map map_;
};

struct Line
{
    int indent = 0;
    std::string text;
};

class Parser
{
public:
    explicit Parser(const std::string& content) { Tokenize(content); }

    Node Parse()
    {
        return ParseNode(0);
    }

private:
    std::vector<Line> lines_;
    std::size_t cursor_ = 0;

    Node ParseNode(int indent)
    {
        SkipEmpty();
        if (cursor_ >= lines_.size()) return {};
        if (lines_[cursor_].indent < indent) return {};

        const std::string text = Trim(lines_[cursor_].text);
        if (text.empty()) {
            ++cursor_;
            return ParseNode(indent);
        }
        if (StartsWithDash(text)) return ParseSequence(indent);
        return ParseMapping(indent);
    }

    Node ParseMapping(int indent)
    {
        Node::Map map;
        while (cursor_ < lines_.size()) {
            const auto& line = lines_[cursor_];
            if (line.indent < indent) break;
            const std::string text = Trim(line.text);
            if (text.empty() || line.indent > indent) {
                ++cursor_;
                continue;
            }
            if (StartsWithDash(text)) break;

            const auto colon_pos = text.find(':');
            if (colon_pos == std::string::npos) {
                ++cursor_;
                continue;
            }
            const std::string key = Trim(text.substr(0, colon_pos));
            std::string value = Trim(text.substr(colon_pos + 1));
            if (value.empty()) {
                ++cursor_;
                Node child = ParseNode(indent + 2);
                map.push_back({key, std::make_shared<Node>(child)});
            } else {
                ++cursor_;
                map.push_back({key, std::make_shared<Node>(ParseValue(value))});
            }
        }
        return Node::MapNode(std::move(map));
    }

    Node ParseSequence(int indent)
    {
        Node::Sequence seq;
        while (cursor_ < lines_.size()) {
            const auto& line = lines_[cursor_];
            if (line.indent < indent) break;
            std::string text = Trim(line.text);
            if (text.empty()) {
                ++cursor_;
                continue;
            }
            if (!StartsWithDash(text)) break;
            if (line.indent < indent) break;

            ++cursor_;
            std::string rest = Trim(text.substr(1));
            if (rest.empty()) {
                seq.push_back(std::make_shared<Node>(ParseNode(indent + 2)));
            } else if (rest.front() == '[' && rest.back() == ']') {
                seq.push_back(std::make_shared<Node>(ParseFlowSequence(rest)));
            } else if (rest.find(':') != std::string::npos) {
                const auto colon_pos = rest.find(':');
                const std::string key = Trim(rest.substr(0, colon_pos));
                const std::string value = Trim(rest.substr(colon_pos + 1));
                Node::Map inner;
                inner.push_back({key, std::make_shared<Node>(value.empty() ? Node{} : ParseValue(value))});
                seq.push_back(std::make_shared<Node>(Node::MapNode(std::move(inner))));
            } else {
                seq.push_back(std::make_shared<Node>(ParseValue(rest)));
            }
        }
        return Node::SequenceNode(std::move(seq));
    }

    Node ParseValue(const std::string& raw)
    {
        const std::string value = Trim(raw);
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
            return ParseFlowSequence(value);
        }
        return Node(value);
    }

    Node ParseFlowSequence(const std::string& flow)
    {
        std::string inner = flow.substr(1, flow.size() - 2);
        auto parts = SplitFlowItems(inner);
        Node::Sequence seq;
        for (auto& part : parts) {
            seq.push_back(std::make_shared<Node>(ParseValue(part)));
        }
        return Node::SequenceNode(std::move(seq));
    }

    static std::vector<std::string> SplitFlowItems(const std::string& input)
    {
        std::vector<std::string> items;
        std::string current;
        bool in_single = false;
        bool in_double = false;
        int nest = 0;

        for (size_t i = 0; i < input.size(); ++i) {
            const char ch = input[i];
            if (ch == '\'' && !in_double) in_single = !in_single;
            if (ch == '\"' && !in_single) in_double = !in_double;
            if (!in_single && !in_double) {
                if (ch == '[' || ch == '{' ) ++nest;
                if (ch == ']' || ch == '}') --nest;
            }
            if (!in_single && !in_double && nest == 0 && ch == ',') {
                items.push_back(Node::TrimValue(current));
                current.clear();
                continue;
            }
            current.push_back(ch);
        }
        if (!current.empty() || !items.empty()) {
            items.push_back(Node::TrimValue(current));
        }
        return items;
    }

    static std::string TrimLineComment(std::string text)
    {
        bool in_single = false;
        bool in_double = false;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\'' && !in_double) in_single = !in_single;
            else if (text[i] == '\"' && !in_single) in_double = !in_double;
            else if (!in_single && !in_double && text[i] == '#') {
                return text.substr(0, i);
            }
        }
        return text;
    }

    void Tokenize(const std::string& content)
    {
        std::string current;
        for (size_t i = 0; i <= content.size(); ++i) {
            if (i == content.size() || content[i] == '\n') {
                if (!current.empty() && current.back() == '\r') current.pop_back();
                current = TrimLineComment(current);
                const std::string trimmed = Trim(current);
                if (!trimmed.empty() || (!current.empty() && line_has_indent_only(current))) {
                    lines_.push_back({static_cast<int>(IndentOf(current)), current});
                }
                current.clear();
            } else {
                current.push_back(content[i]);
            }
        }
    }

    void SkipEmpty()
    {
        while (cursor_ < lines_.size()) {
            const std::string text = Trim(lines_[cursor_].text);
            if (text.empty()) {
                ++cursor_;
                continue;
            }
            break;
        }
    }

    static bool line_has_indent_only(const std::string& line)
    {
        return !line.empty() && Trim(line).empty();
    }

    static bool StartsWithDash(const std::string& text)
    {
        return !text.empty() && text[0] == '-';
    }

    static int IndentOf(const std::string& text)
    {
        int count = 0;
        for (char ch : text) {
            if (ch == ' ') ++count;
            else break;
        }
        return count;
    }

    static std::string Trim(const std::string& text)
    {
        std::size_t l = 0;
        while (l < text.size() && std::isspace(static_cast<unsigned char>(text[l]))) ++l;
        std::size_t r = text.size();
        while (r > l && std::isspace(static_cast<unsigned char>(text[r - 1]))) --r;
        return text.substr(l, r - l);
    }
};

inline Node Load(const std::string& text)
{
    Parser parser(text);
    return parser.Parse();
}

inline Node LoadFile(const std::string& file_path)
{
    std::ifstream in(file_path);
    if (!in) throw Exception("Unable to open YAML file: " + file_path);
    std::string content((std::istreambuf_iterator<char>(in)), {});
    return Load(content);
}

} // namespace YAML
