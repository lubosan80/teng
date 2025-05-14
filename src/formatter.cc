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
 * $Id: tengformatter.cc,v 1.1 2004-07-28 11:36:55 solamyl Exp $
 *
 * DESCRIPTION
 * Teng formatter (writer adapter) -- implementation.
 *
 * AUTHORS
 * Vaclav Blazek <blazek@firma.seznam.cz>
 *
 * HISTORY
 * 2003-09-17  (vasek)
 *             Created.
 */


#include <cctype>
#include <unordered_map>

#include "teng/stringview.h"
#include "formatter.h"

namespace Teng {
namespace {

/** @short Process sequence of spaces.
 *  @param str white string
 *  @return 0 OK, !0 error
 */
int
process(
    const std::stack<Formatter_t::Mode_t> &modeStack,
    std::string &str,
    Writer_t &writer
) {
    // ignore empty space block
    if (str.empty()) return 0;

    // process spaces according to current mode
    switch (modeStack.top()) {
    case Formatter_t::MODE_COPY_PREV:
    case Formatter_t::MODE_INVALID:
        // silently ignore invalid mode and pass it to the next
        // rule
        // NO BREAK!!!
    case Formatter_t::MODE_PASSWHITE:
        // pass whole string to the writer
        if (writer.write(str))
            return -1;
        break;
    case Formatter_t::MODE_NOWHITE:
        // no space to output
        break;
    case Formatter_t::MODE_ONESPACE:
        // pass just one space
        if (writer.write(" ", 1))
            return -1;
        break;
    case Formatter_t::MODE_STRIPLINES:
        {
            // find newline
            std::string::size_type nl = str.find('\n');
            if (nl == std::string::npos) {
                // no newline found => pass whole string
                if (writer.write(str))
                    return -1;
            } else {
                // newline found => pass just one newline
                if (writer.write("\n", 1))
                    return -1;
            }
        }
        break;
    case Formatter_t::MODE_JOINLINES:
        {
            // find newline
            std::string::size_type nl = str.find('\n');
            if (nl == std::string::npos) {
                // no newline found => pass whole string
                if (writer.write(str))
                    return -1;
            } else {
                // newline found => pass leading whitespaces upto the
                // newline
                if (writer.write(str.data(), nl))
                    return -1;
            }
        }
        break;
    case Formatter_t::MODE_NOWHITELINES:
        {
            // find newline
            std::string::size_type fnl = str.find('\n');
            if (fnl == std::string::npos) {
                // no newline found => pass whole string
                if (writer.write(str))
                    return -1;
                break;
            }
            // find newline from the end of string
            std::string::size_type lnl = str.rfind('\n');
            if (fnl == lnl) {
                // indices are the same => single newline
                // => pass whole string
                if (writer.write(str))
                    return -1;
                break;
            }
            // more newlines => pass leading whitespaces upto then
            // first newline and trailing whitespaces from the last
            // newline
            if (writer.write(str.data(), fnl + 1))
                return -1;
            if (writer.write(str.data() + lnl + 1, str.size() - lnl - 1))
                return -1;
        }
        break;
    }
    // erase buffer
    str.erase();
    return 0;
}

/** @short Process sequence of spaces.
 *  @param spaceBlock block of spaces
 *  @return 0 OK, !0 error
 */
int
process(
    const std::stack<Formatter_t::Mode_t> &modeStack,
    std::pair<const char *, const char *> spaceBlock,
    std::string &buffer,
    Writer_t &writer
) {
    if (modeStack.top() == Formatter_t::MODE_NOWHITE) {
        // output of spaces disabled
        buffer.erase();
        return 0;
    }
    // append buffer by space list
    buffer.append(spaceBlock.first, spaceBlock.second);
    // process buffer
    int ret = process(modeStack, buffer, writer);
    buffer.erase();
    return ret;
}

} // namespace

Formatter_t::Formatter_t(Writer_t &writer, Formatter_t::Mode_t initialMode)
    : writer(writer), modeStack(), buffer()
{
    // initialize mode stack with given initial mode
    modeStack.push(initialMode);
}

int Formatter_t::write(string_view_t str) {
    // pass whole string when passing mode active
    if (modeStack.top() == MODE_PASSWHITE)
        return writer.write(str.data(), str.size());

    // indicates that we are in block of spaces
    bool spaces = false;
    // block of spaces
    auto spaceblock = std::make_pair(str.begin(), str.begin());
    // block of other characters
    auto charblock = std::make_pair(str.begin(), str.begin());
    // run through input string
    for (auto istr = str.begin(); istr != str.end(); ++istr) {
        if (isspace(*istr)) {
            // current character is white space
            if (!spaces) {
                // we were in block of characters when some characters
                // are to be flushed flush them
                if (charblock.first != charblock.second)
                    if (writer.write(charblock.first, charblock.second))
                        return -1;
                // initialize space block
                spaceblock.first = istr;
                spaceblock.second = istr + 1;
                // change context
                spaces = true;
            } else {
                // advance one space
                ++spaceblock.second;
            }
        } else {
            if (istr == str.begin()) {
                // string begins with text => process buffer
                int ret = process(modeStack, buffer, writer);
                buffer.erase();
                if (ret) return ret;
            }
            if (spaces) {
                // we were in block of spaces when some characters are
                // to be processed process them
                if ((spaceblock.first != spaceblock.second) || !buffer.empty())
                    if (process(modeStack, spaceblock, buffer, writer))
                        return -1;
                // initialize character block
                charblock.first = istr;
                charblock.second = istr + 1;
                // change context
                spaces = false;
            } else {
                // advance one character
                ++charblock.second;
            }
        }
    }

    // postprocessing

    // get rid of current buffer
    buffer.erase();
    if (spaces) {
        // we were in block of spaces so we must remember them for
        // next round
        if (spaceblock.first != spaceblock.second)
            buffer.append(spaceblock.first, spaceblock.second);
    } else {
        // we were in block of characters when some characters are to
        // be flushed flush them
        if (charblock.first != charblock.second)
            if (writer.write(charblock.first, charblock.second))
                return -1;
    }

    // OK
    return 0;
}

int Formatter_t::flush() {
    // flush buffer
    if (!buffer.empty())
        if (process(modeStack, buffer, writer))
            return -1;
    // flush writer
    return writer.flush();
}

int Formatter_t::push(Mode_t mode) {
    // flush buffer
    if (!buffer.empty())
        if (process(modeStack, buffer, writer))
            return -1;
    // push new mode
    modeStack.push(mode);
    // OK
    return 0;
}

Formatter_t::Mode_t Formatter_t::pop() {
    // on attempt of removind last element return MODE_INVALID
    if (modeStack.size() <= 1)
        return MODE_INVALID;
    // flush buffer
    if (!buffer.empty())
        if (process(modeStack, buffer, writer))
            return MODE_INVALID;
    // get old mode
    Mode_t oldMode = modeStack.top();
    // remove old mode
    modeStack.pop();
    //  return old mode
    return oldMode;
}

Formatter_t::Mode_t resolveFormat(const string_view_t &name) {
    static const std::unordered_map<std::string, Formatter_t::Mode_t> modes = {
        {"nowhite", Formatter_t::MODE_NOWHITE},
        {"nospace", Formatter_t::MODE_NOWHITE},
        {"onespace", Formatter_t::MODE_ONESPACE},
        {"striplines", Formatter_t::MODE_STRIPLINES},
        {"joinlines", Formatter_t::MODE_JOINLINES},
        {"nowhitelines", Formatter_t::MODE_NOWHITELINES},
        {"noformat", Formatter_t::MODE_PASSWHITE},
    };
    auto imode = modes.find(name.str());
    return imode == modes.end()
        ? Formatter_t::MODE_INVALID
        : imode->second;
}

} // namespace Teng

