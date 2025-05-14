/*
 * Teng -- a general purpose templating engine.
 * Copyright (C) 2004  Seznam.cz, a.s.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Seznam.cz, a.s.
 * Naskove 1, Praha 5, 15000, Czech Republic
 * http://www.seznam.cz, mailto:teng@firma.seznam.cz
 *
 *
 * $Id: tenglex1.cc,v 1.1 2004-07-28 11:36:55 solamyl Exp $
 *
 * DESCRIPTION
 * Teng template level #1 lexical analyzer.
 *
 * AUTHORS
 * Stepan Skrob <stepan@firma.seznam.cz>
 *
 * HISTORY
 * 2003-09-18  (stepan)
 *             Created.
 */

#include <stdexcept>
#include <iostream>

#include "teng/error.h"
#include "configuration.h"
#include "logging.h"
#include "utf8.h"
#include "util.h"
#include "lex1.h"

namespace Teng {
namespace Parser {
namespace {

/** Unescape substring of input string variable.
 *
 * Transformations:
 *     $\{ -> ${
 *     #\{ -> #{
 *     %\{ -> %{
 *     ?\> -> ?>
 *     \}  -> {
 *     <\? -> <?
 */
std::string unescape(const Configuration_t *params, const string_view_t &str) {
    enum {
        initial,
        dollar_expected_backslash_lcurly,
        dollar_backslash_expected_lcurly,
        backslash_expected_rcurly,
        lt_expected_backslash_question,
        lt_backslash_expected_question,
        hash_expected_backslash_lcurly,
        hash_backslash_expected_lcurly,
        percent_expected_backslash_lcurly,
        percent_backslash_expected_lcurly,
        question_expected_backslash_gt,
        question_backslash_expected_gt,
    } state = initial;

    // the result (it can't be longer than source string)
    std::string res;
    res.resize(str.size());
    auto eres = res.begin();

    // boundaries of source string
    const char *istr = str.begin();
    const char *estr = str.end();

    // postion of last escape sequence in source string
    const char *eseq = istr;
    const char *iseq = istr;

    auto do_unescape = [&] (const char *ipos, string_view_t seq) {
        // copy all data from last escape sequence to recent one
        eres = std::copy(eseq, iseq, eres);
        // copy the unescaped data to result
        eres = std::copy(seq.begin(), seq.end(), eres);
        // and mark where the last escape sequence ends (for next unescaping)
        eseq = ++ipos;
    };

    for (auto ipos = istr; ipos != estr; ++ipos) {
        switch (*ipos) {
        case '$':
            //!< .$\{ -> $.\{
            state = dollar_expected_backslash_lcurly;
            iseq = ipos;
            break;

        case '#':
            //!< .#\{ -> #.\{
            state = hash_expected_backslash_lcurly;
            iseq = ipos;
            break;

        case '%':
            //!< .%\{ -> #.\{
            state = percent_expected_backslash_lcurly;
            iseq = ipos;
            break;

        case '<':
            //!< .<\? -> <.\?
            state = lt_expected_backslash_question;
            iseq = ipos;
            break;

        case '\\':
            switch (state) {
            // $.\{ -> $\.{
            case dollar_expected_backslash_lcurly:
                state = dollar_backslash_expected_lcurly;
                break;
            // <.\? -> <\.?
            case lt_expected_backslash_question:
                state = lt_backslash_expected_question;
                break;
            // #.\{ -> #\.{
            case hash_expected_backslash_lcurly:
                state = hash_backslash_expected_lcurly;
                break;
            // %.\{ -> %\.{
            case percent_expected_backslash_lcurly:
                state = percent_backslash_expected_lcurly;
                break;
            // ?.\> -> ?\.>
            case question_expected_backslash_gt:
                state = question_backslash_expected_gt;
                break;
            // .\{ -> \.{
            default:
                state = backslash_expected_rcurly;
                iseq = ipos;
                break;
            }
            break;

        case '?':
            switch (state) {
            // <\.? -> <\?.
            case lt_backslash_expected_question:
                do_unescape(ipos, "<?");
                state = initial;
                break;
            // .?\> -> ?.\>
            default:
                state = question_expected_backslash_gt;
                iseq = ipos;
                break;
            }
            break;

        case '>':
            switch (state) {
            // ?\.> -> ?\>.
            case question_backslash_expected_gt:
                do_unescape(ipos, "?>");
                break;
            // X.> -> X>.
            default:
                break;
            }
            state = initial;
            break;

        case '{':
            switch (state) {
            //!< $\.{ -> $\{.
            case dollar_backslash_expected_lcurly:
                do_unescape(ipos, "${");
                break;
            //!< #\.{ -> #\{.
            case hash_backslash_expected_lcurly:
                do_unescape(ipos, "#{");
                break;
            //!< %\.{ -> %\{.
            case percent_backslash_expected_lcurly:
                if (params->isPrintEscapeEnabled())
                    do_unescape(ipos, "%{");
                break;
            // X.{ -> X{.
            default:
                break;
            }
            state = initial;
            break;

        case '}':
            switch (state) {
            // X\.} -> X\}.
            case dollar_backslash_expected_lcurly:
            case lt_backslash_expected_question:
            case hash_backslash_expected_lcurly:
            case question_backslash_expected_gt:
                // remove first char from escape sequence
                ++iseq;
                // fall through
            // \.} -> \}.
            case backslash_expected_rcurly:
                do_unescape(ipos, "}");
                break;
            // \.X -> \X.
            default:
                break;
            }
            state = initial;
            break;
        default:
            state = initial;
            break;
        };
    }
    // copy all data from last unescaped escape sequence to end of string
    eres = std::copy(eseq, estr, eres);
    // and resize the result to real size
    res.resize(std::distance(res.begin(), eres));
    return res;
}

/** Reads input until end of quoted string is reached.
 */
template <char quoting_char, typename incr_pos_t, typename incr_col_pos_t>
bool incr_until(
    const flex_string_view_t &source_code,
    std::size_t &offset,
    incr_pos_t incr_pos,
    incr_col_pos_t incr_col_pos
) {
    while (offset < source_code.size()) {
        switch (source_code[offset]) {
        case quoting_char:
            incr_col_pos(1);
            return true;
        case '\\':
            incr_col_pos(1);
            if (offset == source_code.size())
                return false;
            incr_pos();
            break;
        default:
            incr_pos();
            break;
        }
    }
    return false;
};

/** Increments position in the input by the length of the given string.
 */
template <std::size_t n, typename incr_col_pos_t>
void incr_col_pos_by(const char (&)[n], incr_col_pos_t incr_col_pos) {
    incr_col_pos(n - 1); // the padding zero
};

} // namespace

std::string Lex1_t::unescape(const string_view_t &str) const {
    return Parser::unescape(params, str);
}

Lex1_t::Token_t Lex1_t::next() {
    // backup start values
    std::size_t start_pos = offset;
    int64_t start_line = pos.lineno;
    int64_t start_column = pos.colno;

    auto offset_incr = [&] (int n) {
        offset = std::min(source_code.size(), offset + n);
    };

    auto incr_pos = [&] {
        if (utf8) {
            auto byte = static_cast<uint8_t>(source_code[offset]);
            switch (byte >> 4) {
            case 0b0000 ... 0b1011: // 0xxxxxxx, 10xxxxxx
                pos.advance(source_code[offset]);
                offset += 1;
                break;
            case 0b1100 ... 0b1101: // 110xxxxx
                pos.advanceColumn(1);
                offset_incr(2);
                break;
            case 0b1110:            // 1110xxxx
                pos.advanceColumn(1);
                offset_incr(3);
                break;
            case 0b1111:            // 11110xxx, 111110xx, 1111110x
                switch ((byte & 0b00001100) >> 2) {
                case 0b00: pos.advanceColumn(1); offset_incr(4); break;
                case 0b01: pos.advanceColumn(1); offset_incr(4); break;
                case 0b10: pos.advanceColumn(1); offset_incr(5); break;
                case 0b11: pos.advanceColumn(1); offset_incr(6); break;
                }
                break;
            }

        } else {
            pos.advance(source_code[offset]);
            ++offset;
        }
    };

    auto incr_col_pos = [&] (int n) {
        pos.advanceColumn(n);
        offset += n;
    };

    // syntactic sugar
    auto incr_pos_until_single_quote = [&] () {
        return incr_until<'\''>(source_code, offset, incr_pos, incr_col_pos);
    };
    auto incr_pos_until_double_quote = [&] () {
        return incr_until<'"'>(source_code, offset, incr_pos, incr_col_pos);
    };
    auto incr_pos_until_right_curly = [&] () {
        return incr_until<'}'>(source_code, offset, incr_pos, incr_col_pos);
    };

    // returns true if source code at current offset matches desired string
    auto match_char = [&] (char ch, int i) {
        if ((offset + i) >= source_code.size())
            return false;
        return source_code[offset + i] == ch;
    };

    // returns true if source code at current offset matches isspace functor
    auto match_space = [&] (int i) -> bool {
        if ((offset + i) >= source_code.size())
            return false;
        return isspace(source_code[offset + i]);
    };

    // returns true if source code at current offset matches desired string
    auto match_str = [&] (string_view_t str, int i) {
        if ((offset + i + str.size()) > source_code.size())
            return false;
        string_view_t view(source_code.data() + i + offset, str.size());
        return view == str;
    };

    // returns end of input token
    auto make_end_of_input_token = [&] () -> Token_t {
        return {
            LEX1::END_OF_INPUT,
            {pos.filename, start_line, start_column},
            "End of input stream"
        };
    };

    // returns error token
    auto make_error_token = [&] (string_view_t error_text) -> Token_t {
        return {
            LEX1::ERROR,
            {pos.filename, start_line, start_column},
            error_text
        };
    };

    // returns text token built from gathered characters
    auto make_text_token = [&] () -> Token_t {
        return {
            LEX1::TEXT,
            {pos.filename, start_line, start_column},
            string_view_t{source_code.begin() + start_pos, offset - start_pos}
        };
    };

    // returns token built from gathered characters
    auto make_token = [&] (LEX1 id) -> Token_t {
        return {
            id,
            {pos.filename, start_line, start_column},
            {source_code, start_pos, offset - start_pos}
        };
    };

    // set lexer state and return text token
    auto accept_text_and_defer = [&] (state deferred_state) {
        current_state = deferred_state;
        return make_text_token();
    };

    // moves tht offset pointer to right curly if it is found
    auto read_expr_content = [&] () {
        // search for '}' and ignore such '}' that are in string literals
        while (offset < source_code.size()) {
            switch (source_code[offset]) {
            case '\'':
                incr_col_pos(1);
                incr_pos_until_single_quote();
                break;
            case '"':
                incr_col_pos(1);
                incr_pos_until_double_quote();
                break;
            case '}':
                incr_col_pos(1);
                return true;
            default:
                incr_pos();
                break;
            }
        }
        return false;
    };

    // returns teng token, it assumes that "<?" has been matched
    auto accept_teng_directive = [&] () {
        // search for '?>' and ignore such '?>' that are in string literals
        while (offset < source_code.size()) {
            switch (source_code[offset]) {
            case '\'':
                incr_col_pos(1);
                incr_pos_until_single_quote();
                break;
            case '"':
                incr_col_pos(1);
                incr_pos_until_double_quote();
                break;
            case '>':
                incr_col_pos(1);
                if (source_code[offset - 2] == '?')
                    if ((offset - start_pos) > strlen("<?>"))
                        return true;
                break;
            default:
                incr_pos();
                break;
            }
        }
        return false;
    };

    // returns esc expr token, it assumes that "${" has been matched
    auto accept_esc_expr_directive = [&] () -> Token_t {
        incr_col_pos_by("${", incr_col_pos);
        return read_expr_content()
            ? make_token(LEX1::ESC_EXPR)
            : make_error_token("Unterminated ${...} directive");
    };

    // returns raw expr token, it assumes that "%{" has been matched
    auto accept_raw_expr_directive = [&] () -> Token_t {
        incr_col_pos_by("%{", incr_col_pos);
        return read_expr_content()
            ? make_token(LEX1::RAW_EXPR)
            : make_error_token("Unterminated %{...} directive");
    };

    // returns dict token, it assumes that "#{" has been matched
    auto accept_dict_directive = [&] () -> Token_t {
        incr_col_pos_by("#{", incr_col_pos);
        return incr_pos_until_right_curly()
            ? make_token(LEX1::DICT)
            : make_error_token("Unterminated #{...} directive");
    };

    // returns teng token, it assumes that "<?" has been matched
    auto accept_short_directive = [&] () -> Token_t {
        incr_col_pos_by("<?", incr_col_pos);
        return accept_teng_directive()
            ? make_token(LEX1::TENG_SHORT)
            : make_error_token("Unterminated <?...?> directive");
    };

    // returns teng token, it assumes that "<?teng" has been matched
    auto accept_long_directive = [&] () -> Token_t {
        incr_col_pos_by("<?teng", incr_col_pos);
        return accept_teng_directive()
            ? make_token(LEX1::TENG)
            : make_error_token("Unterminated <?teng...?> directive");
    };

    // swallows comments, it assumes that "<!---" has been matched
    auto accept_comment_directive = [&] () {
        incr_col_pos_by("<!---", incr_col_pos);
        while (offset < source_code.size()) {
            switch (source_code[offset]) {
            case '>':
                if (source_code[offset - 1] != '-') {incr_col_pos(1); continue;}
                if (source_code[offset - 2] != '-') {incr_col_pos(1); continue;}
                if (source_code[offset - 3] != '-') {incr_col_pos(1); continue;}
                if ((offset - start_pos) <= 8) {incr_col_pos(1); continue;}
                start_pos = offset + 1;
                return true;
            default:
                incr_pos();
                continue;
            }
        }
        start_pos = offset;
        return false;
    };

    // parse and return deferred tokens
    switch (current_state) {
    case state::initial:
        break;
    case state::end_of_input:
        return make_end_of_input_token();
    case state::comment_directive:
        current_state = state::initial;
        accept_comment_directive();
        break;
    case state::long_directive:
        current_state = state::initial;
        return accept_long_directive();
    case state::short_directive:
        current_state = state::initial;
        return accept_short_directive();
    case state::esc_expr_directive:
        current_state = state::initial;
        return accept_esc_expr_directive();
    case state::raw_expr_directive:
        current_state = state::initial;
        return accept_raw_expr_directive();
    case state::dict_directive:
        current_state = state::initial;
        return accept_dict_directive();
    }

    // there is no deferred token then parse new one
    for (; offset < source_code.size(); incr_pos()) {
        switch (source_code[offset]) {
        // accept <!---.*--->, <?.*?>, <?teng.*?>
        case '<':
            if ((offset + 1) < source_code.size()) {
                switch (source_code[offset + 1]) {
                case '?':
                    if (match_str("teng", +2) && match_space(+6))
                        return offset == start_pos
                            ? accept_long_directive()
                            : accept_text_and_defer(state::long_directive);
                    if (params->isShortTagEnabled())
                        return offset == start_pos
                            ? accept_short_directive()
                            : accept_text_and_defer(state::short_directive);
                    continue;
                case '!':
                    if (!match_str("---", +2)) continue;
                    if (offset != start_pos)
                        return accept_text_and_defer(state::comment_directive);
                    if (!accept_comment_directive())
                        return make_end_of_input_token();
                    continue;
                default:
                    continue;
                }
            }
            continue;

        // accept ${[^}]*}
        case '$':
            if (!match_char('{', +1)) continue;
            return offset == start_pos
                ? accept_esc_expr_directive()
                : accept_text_and_defer(state::esc_expr_directive);

        // accept %{[^}]*}
        case '%':
            if (!match_char('{', +1)) continue;
            if (!params->isPrintEscapeEnabled()) continue;
            return offset == start_pos
                ? accept_raw_expr_directive()
                : accept_text_and_defer(state::raw_expr_directive);

        // accept #{[^}]*}
        case '#':
            if (!match_char('{', +1)) continue;
            return offset == start_pos
                ? accept_dict_directive()
                : accept_text_and_defer(state::dict_directive);

        // accept .
        default:
            break;
        }
    }

    // if there is no trailing text return END_OF_INPUT immediately
    // or return text token and defer emiting it
    return offset == start_pos
        ? make_end_of_input_token()
        : accept_text_and_defer(state::end_of_input);
}

const char *Lex1_t::Token_t::name() const {
    switch (token_id) {
    case LEX1::END_OF_INPUT: return "<EOF>";
    case LEX1::ERROR: return "ERROR";
    case LEX1::TEXT: return "TEXT";
    case LEX1::TENG: return "TENG";
    case LEX1::TENG_SHORT: return "TENG_SHORT";
    case LEX1::ESC_EXPR: return "ESC_EXPR";
    case LEX1::RAW_EXPR: return "RAW_EXPR";
    case LEX1::DICT: return "DICT";
    }
    throw std::runtime_error(__PRETTY_FUNCTION__);
}

std::ostream &operator<<(std::ostream &os, const Lex1_t::Token_t &token) {
    os << "(level=1"
       << ", id=" << static_cast<int>(token.token_id)
       << ", name=" << token.name()
       << ", view='" << token.view() << "'"
       << ", size=" << token.view().size()
       << ", at=" << token.pos << ")";
    return os;
}

} // namespace Parser
} // namespace Teng

