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
 * $Id: tengfunction.cc,v 1.18 2008-11-20 23:32:29 burlog Exp $
 *
 * DESCRIPTION
 * Teng processor function (like len, substr, round or date)
 *
 * AUTHORS
 * Jan Nemec <jan.nemec@firma.seznam.cz>
 * Stepan Skrob <stepan@firma.seznam.cz>
 * Michal Bukovsky <michal.bukovsky@firma.seznam.cz>
 *
 * HISTORY
 * 2003-09-26  (honza)
 *             Created.
 * 2004-05-08  (vasek)
 *             Polished code.
 * 2005-06-21  (roman)
 *             Win32 support.
 * 2018-06-14  (burlog)
 *             Moved from tengfunction.cc and polished.
 */

#ifndef TENGFUNCTIONSTRING_H
#define TENGFUNCTIONSTRING_H

#include <string>
#include <cstdint>
#include <limits>

#include <tengutf8.h>
#include <tengplatform.h>
#include <tengfunctionutil.h>
#include <tengfunction.h>

namespace Teng {
namespace builtin {
namespace {

/** Arguments for substring functions.
 */
struct SubstrArgs_t {
    std::string text;
    int start;
    int end;
    std::string prefix;
    std::string suffix;
};

/** Parses arguments for substring functions and invokes given substr
 * implementation.
 */
template <typename impl_type>
Result_t
do_substr(Ctx_t &ctx, const char *fun, const Args_t &args, impl_type impl) {
    if ((args.size() < 2) || (args.size() > 5))
        return wrongNumberOfArgs(ctx, fun, 2, 5);

    // 1: text
    SubstrArgs_t s;
    auto iarg = args.rbegin();
    s.text = (iarg++)->str();

    // 2: start
    if (!iarg->is_integral()) {
        static auto *msg = "Second arg must be an int as it's start index";
        return failed(ctx, fun, msg);
    }
    s.start = (iarg++)->as_int();

    // 3: end [optional]
    s.end = std::numeric_limits<int>::max();
    if (iarg != args.rend()) {
        if (iarg->is_integral()) {
            s.end = (iarg++)->as_int();
        } else if (args.size() == 5) {
            static auto *msg = "Third arg must be an int as it's end index";
            return failed(ctx, fun, msg);
        }
    }

    // 4: prefix [optional]
    if (iarg != args.rend())
        s.prefix = (iarg++)->str();

    // 5: suffix [optional]
    s.suffix = iarg != args.rend()
        ? (iarg++)->str()
        : s.prefix;

    // done
    return impl(s);
}

namespace ascii {

/** Adjusts index (even negative) to interval <0, text_size>.
 */
std::size_t idx2offset(std::size_t text_size, int i) {
    if (i >= 0)
        return std::min<std::size_t>(i, text_size);
    i += text_size;
    if (i >= 0)
        return std::min<std::size_t>(i, text_size);
    return 0;
}

/** Substr implementation for ascii/onebyte encodings.
 */
std::string substr(const SubstrArgs_t &args) {
    // adjust indicies
    std::size_t start = idx2offset(args.text.size(), args.start);
    std::size_t end = idx2offset(args.text.size(), args.end);

    // compose result
    std::string result;
    if (start != 0)
        result.append(args.prefix);
    if (start < end)
        result.append(args.text, start, (end - start));
    if (end != args.text.size())
        result.append(args.suffix);
    return result;
}

} // namespace ascii
} // namespace

/** Like strlen. Must handle utf-8 if used.
 *
 * arg0 string
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t len(Ctx_t &ctx, const Args_t &args) {
    if (args.size() != 1)
        return wrongNumberOfArgs(ctx, __FUNCTION__, 1);

    // the string length according to encoding
    return Result_t(
        ctx.encoding == "utf-8"
        ? utf8::strlen(str(args[0]))
        : str(args[0])->size()
    );
}

/** Replaces all unix newlines with "\n<br />".
 *
 * arg0 string
 *
 * @param args Function arguments (list of values).
 * @param ctx Teng function ctx.
 * @param result Function's result value.
 */
Result_t nl2br(Ctx_t &ctx, const Args_t &args) {
    if (args.size() != 1)
        return wrongNumberOfArgs(ctx, "nl2br", 1);

    // prepare space for result
    auto arg0 = str(args[0]);
    std::string tmp;
    tmp.reserve(arg0->size() * 1.3);

    // replace
    for (char ch: *arg0) {
        tmp.push_back(ch);
        if (ch == '\n')
            tmp.append("<br />");
    }

    // done
    return Result_t(std::move(tmp));
}

/** Python-like substr
 *
 * arg0 string
 * arg1 starting index
 * arg2 the number of characters
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t substr(Ctx_t &ctx, const Args_t &args) {
    return do_substr(ctx, "substr", args, [&] (SubstrArgs_t &s) {
        return Result_t(
            ctx.encoding == "utf-8"
            ? utf8::substr(s.text, s.start, s.end, s.prefix, s.suffix)
            : ascii::substr(s)
        );
    });
}

/** Python-like "word safe" substr
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t wordsubstr(Ctx_t &ctx, const Args_t &args) {
    return do_substr(ctx, "wordsubstr", args, [&] (SubstrArgs_t &s) {
        // adjust indicies
        if (ctx.encoding == "utf-8") {
            utf8::substr(s.text, s.start, s.end);
        } else {
            s.start = ascii::idx2offset(s.text.size(), s.start);
            s.end = ascii::idx2offset(s.text.size(), s.end);
        }

        // if interval is empty
        if (s.start >= s.end)
            // don't ask, it's origin algorithm behaviour
            return Result_t(s.suffix);

        // find space or begin
        while ((s.start > 0) && !isspace(s.text[s.start]))
            --s.start;

        // find space or end
        while ((s.end < s.text.size()) && !isspace(s.text[s.end]))
            ++s.end;

        // strip whitespaces from left
        int stripped_start = 0;
        while (isspace(s.text[stripped_start]))
            ++stripped_start;

        // strip whitespaces from right
        int stripped_end = s.text.size();
        while (isspace(s.text[stripped_end - 1]))
            --stripped_end;

        // compose result
        std::string text;
        if (s.start > stripped_start)
            text.append(s.prefix);
        text.append(s.text, s.start, (s.end - s.start));
        if (s.end < stripped_end)
            text.append(s.suffix);

        // done
        return Result_t(std::move(text));
    });
}

/** Reorder - pastes strings into result, first arg is format
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t reorder(Ctx_t &ctx, const Args_t &args) {
    if (args.size() < 1)
        return atLeastXArg(ctx, "reorder", 1);

    // format string
    auto format_arg = str(args.back());
    auto &format = *format_arg;

    // result (formated string) -- reserve some space
    std::string text;
    text.reserve(2 * format.size());

    // status of automaton
    enum class STATUS {
        DEFAULT, /** < default mode, normal string */
        FORMAT,  /** < format mode, we are after % character */
        NUMBER,  /** < number mode, we are parsing number (after %{}) */
    } status = STATUS::DEFAULT;

    // indicates end-of-string
    const int EOS = ~0;

    // parsed index in arguments
    unsigned int index = 0;

    // indicates that we want to replace %index by argument
    bool replace = false;

    // position of last %
    auto mark = format.begin();

    // process automaton
    for (auto iformat = format.begin(); /* forever */; ++iformat) {
        // get next character of EOS when at the string end
        int c = ((iformat == format.end()) ? EOS : *iformat);
        switch (c) {
        case '%':
            switch (status) {
            case STATUS::FORMAT:
                text.push_back('%');
                status = STATUS::DEFAULT;
                break;

            case STATUS::DEFAULT:
                status = STATUS::FORMAT;
                mark = iformat;
                index = 0;
                break;

            case STATUS::NUMBER:
                logError(
                    ctx.err,
                    {},
                    "reorder(): '%' not allowed inside '%{}'"
                );
                status = STATUS::DEFAULT;
                text.append(mark, iformat + 1);
                break;
            }
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            // digit
            switch (status) {
            case STATUS::FORMAT:
                // we have single digit after % => get index and reset
                // automaton
                index = (c - '0');
                replace = true;
                status = STATUS::DEFAULT;
                break;

            case STATUS::DEFAULT:
                // normal operation => just pass digit
                text.push_back(c);
                break;

            case STATUS::NUMBER:
                // we are in number => get next digit
                index *= 10;
                index += (c - '0');
                break;
            }
            break;

        case '{':
            switch (status) {
            case STATUS::FORMAT:
                // { after % => prepare for number parsing
                status = STATUS::NUMBER;
                index = 0;
                break;

            case STATUS::DEFAULT:
                // OK
                text.push_back(c);
                break;

            case STATUS::NUMBER:
                // ? inside number => error
                logError(
                    ctx.err,
                    {},
                    "reorder(): '{' not allowed inside '%{}'"
                );
                status = STATUS::DEFAULT;
                text.append(mark, iformat + 1);
                break;
            }
            break;

        case '}':
            switch (status) {
            case STATUS::FORMAT:
                // } after % => error
                logError(
                    ctx.err,
                    {},
                    "reorder(): '}' not allowed after '%'"
                );
                status = STATUS::DEFAULT;
                text.append(mark, iformat + 1);
                break;

            case STATUS::DEFAULT:
                // OK
                text.push_back(c);
                break;

            case STATUS::NUMBER:
                // end of number
                replace = true;
                status = STATUS::DEFAULT;
                break;
            }
            break;

        case EOS:
            // end of string
            switch (status) {
            case STATUS::DEFAULT:
                // OK
                break;

            case STATUS::FORMAT:
            case STATUS::NUMBER:
                // unterminated format
                logError(
                    ctx.err,
                    {},
                    "reorder(): runaway argument"
                );
                text.append(mark, iformat);
                break;
            }
            break;

        default:
            // other characters
            switch (status) {
            case STATUS::FORMAT:
            case STATUS::NUMBER: {
                // ? after % => error
                char tc = c;
                logError(
                    ctx.err,
                    {},
                    "reorder(): '" + std::string(&tc, 1)
                    + "' not allowed inside %{} or after %"
                );
                status = STATUS::DEFAULT;
                text.append(mark, iformat + 1);
                break;
                }

            case STATUS::DEFAULT:
                // OK
                text.push_back(c);
                break;
            }
            break;
        }

        if (replace) {
            if (!index || (index >= args.size())) {
                // invalid index => do not expand and report error
                logError(
                    ctx.err,
                    {},
                    "reorder(): invalid or missing index in format '"
                    + std::string(mark, iformat + 1) + "'"
                );
                text.append(mark, iformat + 1);
            } else {
                text.append(str(args[args.size() - 1 - index]));
            }
            replace = false;
        }

        // on EOS return
        if (c == EOS) break;
    }

    // set result and return error indicator
    return Result_t(std::move(text));
}

/** Replace - replace all occurences of a substring.
 *
 * arg2 string where pattern is searched
 * arg1 pattern
 * arg0 to what should be replaced
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t replace(Ctx_t &ctx, const Args_t &args) {
    if (args.size() != 3)
        return wrongNumberOfArgs(ctx, "replace", 3);

    // cache args (potentially converted to string)
    auto repl = str(args[0]);
    auto pattern = str(args[1]);
    auto text = str(args[2]);

    // prepare place for result
    std::string tmp;
    tmp.reserve(2 * text->size());

    // empty pattern breaks algorithm bellow
    if (!pattern->empty()) {
        auto i = 0lu;
        for (auto upto = text->size() - pattern->size() + 1; i < upto;) {
            if (text->compare(i, pattern->size(), *pattern) == 0) {
                tmp.append(*repl);
                i += pattern->size();
            } else tmp.push_back((*text)[i++]);
        }

        // append text suffix (the tail that can't contain pattern)
        while (i < text->size()) tmp.push_back((*text)[i++]);
    }

    // done
    return Result_t(std::move(tmp));
}

/** Replace - replace all occurences of regex groups.
 *
 * arg2 string on which regex is applied
 * arg1 regex pattern
 * arg0 to what should be replaced
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t regex_replace(Ctx_t &ctx, const Args_t &args) {
    return args.size() != 3
        ? wrongNumberOfArgs(ctx, "regex_replace", 3)
        : Result_t(utf8::regex_replace(str(args[2]), str(args[1]), str(args[0])));
}

/** Tolower function with utf-8 support.
 *
 * arg0 - string to convert
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t strtolower(Ctx_t &ctx, const Args_t &args) {
    return args.size() != 1
        ? wrongNumberOfArgs(ctx, "strtolower", 1)
        : Result_t(utf8::tolower(str(args[0])));
}

/** Toupper function with utf-8 support.
 *
 * arg0 - string to convert
 *
 * @param args Teng function arguments
 * @param ctx Teng function ctx
 * @param result Teng function result
 */
Result_t strtoupper(Ctx_t &ctx, const Args_t &args) {
    return args.size() != 1
        ? wrongNumberOfArgs(ctx, "strtoupper", 1)
        : Result_t(utf8::toupper(str(args[0])));
}

} // namespace builtin
} // namespace Teng

#endif /* TENGFUNCTIONSTRING_H */

