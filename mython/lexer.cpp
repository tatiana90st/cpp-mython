#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <array>
#include <functional>
#include <string>
#include <iostream>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Lexer::Lexer(std::istream& input)
    :input_(input)
{
    curr_token_ = NextToken();
}

const Token& Lexer::CurrentToken() const {

    return curr_token_;
}

Token LoadString(std::istream& input) {
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            break;
        }
        const char ch = *it;
        if (ch == '"') {
            ++it;
            break;
        }
        else if (ch == '\\') {
            ++it;
            const char escaped_char = *(it);
            switch (escaped_char) {
            case 'n':
                s.push_back('\n');
                break;
            case 't':
                s.push_back('\t');
                break;
            case 'r':
                s.push_back('\r');
                break;
            case '\'':
                s.push_back('\'');
                break;
            case '\\':
                s.push_back('\\');
                break;
            case '\"':
                s.push_back('\"');
                break;
            }
        }
        else {
            s.push_back(ch);
        }
        ++it;
    }

    return Token(token_type::String{ std::move(s) });
}

Token LoadSingleQuotedString(std::istream& input) {
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            break;
        }
        const char ch = *it;
        if (ch == '\'') {
            ++it;
            break;
        }
        else if (ch == '\\') {
            ++it;
            const char escaped_char = *(it);
            switch (escaped_char) {
            case 'n':
                s.push_back('\n');
                break;
            case 't':
                s.push_back('\t');
                break;
            case 'r':
                s.push_back('\r');
                break;
            case '"':
                s.push_back('\"');
                break;
            case '\\':
                s.push_back('\\');
                break;
            case '\'':
                s.push_back('\'');
                break;
            }
        }
        else {
            s.push_back(ch);
        }
        ++it;
    }

    return Token(token_type::String{ std::move(s) });
}

void IgnoreComment(std::istream& input) {
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();

    while (true) {

        if (it == end) {
            break;
        }
        const char ch = *it;
        if (ch == '\n') {
            break;
        }
        ++it;
    }
}

int CountSpaces(std::istream& input) {
    int i = 0;
    while (input.peek() == ' ') {
        input.get();
        ++i;
    }
    if (input.peek() == '\n') {
        return 0;
    }
    return i / 2;
}

Token LoadNumber(std::istream& input) {
    std::string parsed_num;

    while (std::isdigit(input.peek())) {
        parsed_num += input.get();
    }

    return Token(token_type::Number{ std::stoi(parsed_num) });

}

bool IsSymbol(char c) {
    int i = static_cast<int>(c);
    return (i > 39 && i < 48) || i == 58;
}

bool IsCompareSymbol(char c) {
    return (c == '=' || c == '<' || c == '>' || c == '!');
}

std::string LoadLiteral(std::istream& input) {
    std::string res;

    while (input) {
        char c = input.peek();
        if (c < 0) {
            break;
        }
        if (c != ' ' && c != '#' && c != '\n' && !IsSymbol(c) && !IsCompareSymbol(c)) {
            res.push_back(input.get());
        }
        else {
            break;
        }
    }
    return res;
}

Token LoadIdOrElse(std::istream& input) {
    std::string s = LoadLiteral(input);
    if (s == "True"s) {
        return Token(token_type::True{});
    }
    else if (s == "False"s) {
        return Token(token_type::False{});
    }
    else if (s == "None"s) {
        return Token(token_type::None{});
    }
    else if (s == "class"s) {
        return Token(token_type::Class{});
    }
    else if (s == "return"s) {
        return Token(token_type::Return{});
    }
    else if (s == "if"s) {
        return Token(token_type::If{});
    }
    else if (s == "else"s) {
        return Token(token_type::Else{});
    }
    else if (s == "def"s) {
        return Token(token_type::Def{});
    }
    else if (s == "print"s) {
        return Token(token_type::Print{});
    }
    else if (s == "and"s) {
        return Token(token_type::And{});
    }
    else if (s == "or"s) {
        return Token(token_type::Or{});
    }
    else if (s == "not"s) {
        return Token(token_type::Not{});
    }
    else {
        return Token(token_type::Id{ std::move(s) });
    }
}

Token LoadChar(char c) {
    return Token(token_type::Char{ c });
}

Token LoadCompareSymbol(std::istream& input) {
    char c = input.get();
    char next = input.peek();
    if (next == '=') {
        next = input.get();
        if (c == '=') {
            return Token(token_type::Eq{});
        }
        if (c == '>') {
            return Token(token_type::GreaterOrEq{});
        }
        if (c == '<') {
            return Token(token_type::LessOrEq{});
        }
        if (c == '!') {
            return Token(token_type::NotEq{});
        }

    }
    return Token(token_type::Char{ c });
}

Token Lexer::NextToken() {
    char c = input_.peek();

    if (spaces_ > 0) {

        curr_token_ = Token(token_type::Indent{});
        ++indent_index_;
        --spaces_;
    }
    else if (spaces_ < 0) {
        curr_token_ = Token(token_type::Dedent{});
        --indent_index_;
        ++spaces_;
    }
    else {
        if (c > 0) {
            if (c == '#') {
                c = input_.get();
                IgnoreComment(input_);
                curr_token_ = NextToken();
            }

            //строки
            else if (c == '"') {
                c = input_.get();
                curr_token_ = LoadString(input_);
            }
            else if (c == '\'') {
                c = input_.get();
                curr_token_ = LoadSingleQuotedString(input_);
            }

            //числа
            else if (std::isdigit(c)) {
                curr_token_ = LoadNumber(input_);
            }
            else if (std::isalpha(c) || c == '_') {
                curr_token_ = LoadIdOrElse(input_);

            }

            else if (IsCompareSymbol(c)) {
                curr_token_ = LoadCompareSymbol(input_);

            }
            else if (IsSymbol(c)) {
                curr_token_ = LoadChar(c);
                c = input_.get();
            }
            else if (c == '\n') {
                c = input_.get();
                if (!curr_token_.Is<token_type::Newline>() && !first_) {
                    curr_token_ = Token(token_type::Newline{});
                    if (input_.peek() == '\n') {
                        while (input_.peek() == '\n') {
                            c = input_.get();
                        }
                    }
                    if (input_.peek() != ' ' && indent_index_ > 0) {
                        spaces_ = -indent_index_;
                    }
                }
                else {
                    curr_token_ = NextToken();
                }
            }
            else if (c == ' ') {
                //пробелы в середине строки
                if (!curr_token_.Is<token_type::Newline>() && !curr_token_.Is<token_type::Dedent>()) {
                    while (input_.peek() == ' ') {
                        c = input_.get();
                    }
                    curr_token_ = NextToken();
                }

                else if (curr_token_.Is<token_type::Newline>()) {
                    spaces_ = CountSpaces(input_) - indent_index_;
                    curr_token_ = NextToken();
                }

            }
        }
        //if c<0
        else {
            if (!curr_token_.Is<token_type::Eof>()) {

                if (indent_index_ > 0) {
                    curr_token_ = Token(token_type::Dedent{});
                    --indent_index_;
                }
                else {
                    if (!curr_token_.Is<token_type::Newline>() && !curr_token_.Is<token_type::Dedent>() && !first_) {
                        curr_token_ = Token(token_type::Newline{});
                    }
                    else {
                        curr_token_ = Token(token_type::Eof{});
                    }
                }
            }
        }
    }
    first_ = false;
    return curr_token_;
}


}  // namespace parse