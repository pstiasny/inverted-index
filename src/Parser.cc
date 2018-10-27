#include "inverted_index.h"


shared_ptr<InputExpr> Parser::parse(string input) {
    auto tokens = tokenize(input);
    return parse_tokens(tokens);
};

shared_ptr<InputExpr> Parser::parse_tokens(deque<string> &tokens) {
    if (tokens.empty())
        throw ParseError("Premature end of intput");
    auto tok = tokens.front();
    tokens.pop_front();

    if (tok == "(") {
        vector<shared_ptr<InputExpr>> items;
        while (tokens.front() != ")")
            items.push_back(parse_tokens(tokens));
        tokens.pop_front();

        return make_shared<List>(items);
    } else if (tok.front() == '"' && tok.back() == '"') {
        return make_shared<String>(tok.substr(1, tok.size() - 2));
    } else {
        return make_shared<Symbol>(tok);
    }
}

deque<string> Parser::tokenize(string input) {
    deque<string> tokens = {};
    string buf;
    auto is_str = false;

    for (const char c : input) {
        if (is_str) {
            buf += c;
            if (c == '"') {
                is_str = false;
                tokens.push_back(buf);
                buf = "";
            }
        } else {
            if (c == '(' || c == ')' || c == ' ') {
                if (!buf.empty())
                    tokens.push_back(buf);
                buf = "";
            }

            if (c == '(')
                tokens.push_back("(");
            else if (c == ')')
                tokens.push_back(")");
            else if (c == '"') {
                buf += c;
                is_str = true;
            } else if (c != ' ')
                buf += c;
        }
    }
    if (!buf.empty())
        tokens.push_back(buf);
    return tokens;
}
