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
 * $Id: tengfragmentstack.h,v 1.7 2006-06-13 10:04:16 vasek Exp $
 *
 * DESCRIPTION
 * Teng stack fragment frame.
 *
 * AUTHORS
 * Vaclav Blazek <blazek@firma.seznam.cz>
 *
 * HISTORY
 * 2004-05-10  (vasek)
 *             Created.
 */

#ifndef TENGFRAGEMENTSTACK_H
#define TENGFRAGEMENTSTACK_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>

#include "tengerror.h"
#include "tengstructs.h"
#include "tengparservalue.h"
#include "tenginstruction.h"

namespace Teng {

const std::string ERROR_FRAG_NAME("_error");

const std::string FILENAME("filename");

const std::string LINE("line");

const std::string COLUMN("column");

const std::string LEVEL("level");

const std::string MESSAGE("message");

enum Status_t {
    S_OK =               0,
    S_NOT_FOUND =       -1,
    S_BAD =             -2,
    S_OUT_OF_CONTEXT =  -3,
    S_ALREADY_DEFINED = -4,
    S_TYPE_MISMATCH =   -5,
    S_NO_ITERATIONS =   -6,
};

class FragmentFrame_t {
public:
    inline FragmentFrame_t() {
    }

    virtual ~FragmentFrame_t() {}

    virtual const FragmentList_t *
    findSubFragment(const std::string &name) const = 0;

    virtual Status_t
    findVariable(const std::string &name, Parser::Value_t &var) const = 0;

    virtual bool nextIteration() = 0;

    virtual bool overflown() = 0;

    virtual unsigned int size() const = 0;

    virtual unsigned int iteration() const = 0;

    virtual bool
    exists(const std::string &name, bool onlyData = false) const = 0;

    virtual const Fragment_t *getCurrentFragment() const = 0;

    bool localExists(const std::string &name) const {
        return (locals.find(name) != locals.end());
    }

    Status_t
    findLocalVariable(const std::string &name, Parser::Value_t &var,
                                      bool testExistence = false)
        const
    {
        // try to find variable
        std::map<std::string, Parser::Value_t>::const_iterator flocals
            = locals.find(name);
        if (flocals == locals.end()) return S_NOT_FOUND;

        // check whether we are only checking for existence
        if (testExistence) return S_OK;

        // found => assing
        var = flocals->second;

        // OK
        return S_OK;
    }

    Status_t
    setVariable(const std::string &name, const Parser::Value_t &var) {
        // check whether there is non-local variable with given name
        // -- we are not allowed to override data from template
        if (exists(name, true)) return S_ALREADY_DEFINED;

        // try to insert value to local variables
        auto insertResult = locals.insert(make_pair(name, var));
        if (!insertResult.second) {
            // unsuccesful (already set) -- change its value
            insertResult.first->second = var;
        }

        // OK
        return S_OK;
    }

    inline void resetLocals() {
        // remove all local variables (only when non empty)
        if (!locals.empty())
            locals = std::map<std::string, Parser::Value_t>();
    }

private:
    std::map<std::string, Parser::Value_t> locals;
};

class RegularFragmentFrame_t : public FragmentFrame_t {
public:
    RegularFragmentFrame_t(const FragmentList_t *fragmentList = 0)
        : FragmentFrame_t(),
          fragment(fragmentList
                   ? (fragmentList->empty()
                      ? nullptr
                      : fragmentList->begin()->get())
                   : nullptr),
          data(fragmentList
               ? fragmentList->begin()
               : FragmentList_t::const_iterator()),
          dataEnd(fragmentList
                  ? fragmentList->end()
                  : FragmentList_t::const_iterator()),
          dataSize(fragmentList ? fragmentList->size() : 0),
          index(0)
    {}

    RegularFragmentFrame_t(const Fragment_t *fragment)
        : FragmentFrame_t(),
          fragment(fragment),
          data(FragmentList_t::const_iterator()),
          dataEnd(FragmentList_t::const_iterator()),
          dataSize(1), index(0)
    {}

    virtual bool exists(const std::string &name, bool onlyData = false) const {
        Fragment_t::const_iterator ffragment = fragment->find(name);
        if (ffragment != fragment->end()) {
            // we have found identifier in data
            if (auto *nested = ffragment->second->getNestedFragments()) {
                // identifier is fragment -- we have test whether it has any
                // iteration
                if (!nested->empty()) return true;
                // fragment is empty => there can be local variable of
                // this name
            } else return true; // identifier is variable => exists
        }
        return onlyData ? false : localExists(name);
    }

    virtual const FragmentList_t *
    findSubFragment(const std::string &name) const {
        auto subFragment = fragment->find(name);
        return subFragment == fragment->end()
             ? nullptr
             : subFragment->second->getNestedFragments();
    }

    virtual Status_t
    findVariable(const std::string &name, Parser::Value_t &var) const {
        // try to find variable in the associated fragment
        auto ielement = fragment->find(name);

        // when not found => try to find local variable
        if (ielement == fragment->end())
            return findLocalVariable(name, var);

        // check whether found element is value (has no nested fragments)
        if (ielement->second->getNestedFragments())
            return S_TYPE_MISMATCH;

        // OK we have variable's value from data tree!
        var = *ielement->second;
        return S_OK;
    }

    virtual bool nextIteration() {
        // check for end
        if (data == dataEnd) return false;

        // increment index
        ++index;

        // increment data iterator and check for overflow
        if (++data == dataEnd) return false;

        // set current fragment (dereference data)
        fragment = data->get();

        // reset local data;
        resetLocals();

        // ok, we have next iteration
        return true;
    }

    virtual bool overflown() {
        // we have only one iteration
        return data == dataEnd;
    }

    virtual unsigned int size() const {
        return dataSize;
    }

    virtual unsigned int iteration() const {
        return index;
    }

    virtual const Fragment_t *getCurrentFragment() const {
        return fragment;
    }

private:
    const Fragment_t *fragment;
    FragmentList_t::const_iterator data;
    FragmentList_t::const_iterator dataEnd;

    unsigned int dataSize;
    unsigned int index;
};

class ErrorFragmentFrame_t : public FragmentFrame_t {
public:
    inline ErrorFragmentFrame_t(const Error_t &error)
        : FragmentFrame_t(), errors(error.getEntries()),
          errorSize(error.count()), index(0)
    {}

    virtual bool exists(const std::string &name, bool onlyData = false) const {
        if ((name == FILENAME) || (name == LINE)
             || (name == COLUMN) || (name == LEVEL)
            || (name == MESSAGE)) return true;
        return onlyData ? false : localExists(name);
    }

    virtual const FragmentList_t *
    findSubFragment(const std::string &name) const {
        // error fragment has no descendants
        return 0;
    }

    virtual Status_t
    findVariable(const std::string &name, Parser::Value_t &var) const {
        // try to match variable names
        if (name == FILENAME) {
            var = *errors[index].pos.filename;;
            return S_OK;
        } else if (name == LINE) {
            var = errors[index].pos.lineno;
            return S_OK;
        } else if (name == COLUMN) {
            var = errors[index].pos.colno;
            return S_OK;
        } else if (name == LEVEL) {
            var = static_cast<int>(errors[index].level);
            return S_OK;
        } else if (name == MESSAGE) {
            var = errors[index].msg;
            return S_OK;
        }

        // nothing matched, return local variable
        return findLocalVariable(name, var);
    }

    virtual bool nextIteration() {
        // check for end
        if (index == errorSize) return false;

        // reset local variables;
        resetLocals();

        // increment data iterator and return whether we have not runaway
        return (++index != errorSize);
    }

    virtual bool overflown() {
        // we have only one iteration
        return index == errorSize;
    }

    virtual unsigned int size() const {
        return errorSize;
    }

    virtual unsigned int iteration() const {
        return index;
    }

    virtual const Fragment_t *getCurrentFragment() const {
        return 0;
    }

private:
    const std::vector<Error_t::Entry_t> &errors;
    unsigned int errorSize;
    unsigned int index;
};

class FragmentChain_t {
public:
    FragmentChain_t(FragmentFrame_t *rootFrame) {
        frames.reserve(100);
        frames.push_back(rootFrame);
    }

    virtual ~FragmentChain_t() {
        // get rid of all remaining frames
        for (std::vector<FragmentFrame_t*>::iterator iframes = frames.begin();
             iframes != frames.end(); ++iframes)
            if (iframes != frames.begin())
                delete *iframes;
    }

    std::string getPath() const {
        std::string result;
        for (auto segment: path)
            result += "." + segment;
        return result;
    }

    void pushFrame(const std::string &name, FragmentFrame_t *frame) {
        path.push_back(name);
        frames.push_back(frame);
    }

    Status_t popFrame() {
        // check for underflow
        if (path.empty()) return S_OUT_OF_CONTEXT;

        // remove last path part
        path.pop_back();

        // remove and delete last frame
        delete frames.back();
        frames.pop_back();

        // OK
        return S_OK;
    }

    const FragmentList_t *
    findSubFragment(const std::string &name) const {
        return frames.back()->findSubFragment(name);
    }

    Status_t
    findVariable(const Identifier_t &name, Parser::Value_t &var) const {
        // check for range
        if (name.depth >= frames.size()) return S_OUT_OF_CONTEXT;
        return (*(frames.begin() + name.depth))->findVariable(name.name, var);
    }

    Status_t
    setVariable(const Identifier_t &name, const Parser::Value_t &var) {
        // check for range
        if (name.depth >= frames.size()) return S_OUT_OF_CONTEXT;
        return (*(frames.begin() + name.depth))->setVariable(name.name, var);
    }

    Status_t
    getFragmentSize(const Identifier_t &name, unsigned int &fragSize) const {
        // check for range
        if (name.depth >= frames.size()) return S_OUT_OF_CONTEXT;

        // get size
        fragSize = (*(frames.begin() + name.depth))->size();
        return S_OK;
    }

    Status_t
    getSubFragmentSize(const Identifier_t &name, unsigned int &fragSize) const {
        // check for range
        if (name.depth >= frames.size()) return S_OUT_OF_CONTEXT;

        // find subfragment by name
        const FragmentList_t *subFragment
            = (*(frames.begin() + name.depth))->findSubFragment(name.name);
        if (subFragment) {
            // get size
            fragSize = subFragment->size();
        } else {
            // subfragment not present, treating as zero size
            fragSize = 0;
        }

        // OK
        return S_OK;
    }

    Status_t getFragmentIndex(const Identifier_t &name,
                              unsigned int &fragmentIteration,
                              unsigned int *fragmentSize = 0)
        const
    {
        // check for range
        if (name.depth >= frames.size()) return S_OUT_OF_CONTEXT;

        FragmentFrame_t &frame = *(*(frames.begin() + name.depth));

        // get iteration and optional size
        fragmentIteration = frame.iteration();
        if (fragmentSize) *fragmentSize = frame.size();
        return S_OK;
    }

    bool exists(const Identifier_t &name) const {
        // check for range
        if (name.depth >= frames.size()) return false;

        // test for existence
        return (*(frames.begin() + name.depth))->exists(name.name);
    };

    bool empty() const {
        // check for empty path
        return path.empty();
    }

    unsigned int size() const {
        // check for empty path
        return path.size();
    }

    bool nextIteration() {
        return frames.back()->nextIteration();
    }

    bool overflown() {
        return frames.back()->overflown();
    }

    virtual const Fragment_t *getCurrentFragment() const {
        return frames.back()->getCurrentFragment();
    }

private:
    std::vector<std::string> path;
    std::vector<FragmentFrame_t*> frames;
};


class FragmentStack_t {
public:
    FragmentStack_t(const Fragment_t *data, Error_t &error,
                    bool enableErrorFragment = false)
        : data(data), error(error), enableErrorFragment(enableErrorFragment),
          root(data)
    {
        // reserve some space and create new fragment chain for whole
        // page
        chains.reserve(20);
        chains.push_back(&root);
    }

    std::size_t chainSize() const {return chains.size();}

    Status_t pushFrame(const Identifier_t &name) {
        if (name.context) {
            // check whether context has been changed
            chains.push_back(&root);
        }

        // get current chain
        FragmentChain_t &chain = chains.back();

         // create fragment frame
        FragmentFrame_t *frame;

        // create new frame
        if (enableErrorFragment && chain.empty()
            && (name.name == ERROR_FRAG_NAME)) {
            // error fragment
            frame = new ErrorFragmentFrame_t(error);
        } else {
            // regular fragment
            frame = new RegularFragmentFrame_t
                (chain.findSubFragment(name.name));
        }

        // check for empty fragment
        if (frame->overflown()) {
            // we have to remove new context (if just created)
            if (name.context) chains.pop_back();

            // get rid of frame
            delete frame;
            return S_NO_ITERATIONS;
        }

        // ok we have at least one iteration
        chain.pushFrame(name.name, frame);
        return S_OK;
    }

    virtual const Fragment_t *getCurrentFragment() const {
        return chains.back().getCurrentFragment();
    }

    inline bool nextIteration() {
        // process next iteration of current fragment
        return chains.back().nextIteration();
    }

    inline Status_t popFrame() {
        // get current chain
        FragmentChain_t &chain = chains.back();

        // pop frame from last frame chain
        if (Status_t s = chain.popFrame())
            return s;
        // if chain is empty and is not first root remove it
        if ((chains.size() > 1) && (chain.empty()))
            chains.pop_back();
        return S_OK;
    }

    Status_t
    findVariable(const Identifier_t &name, Parser::Value_t &var) const {
        // test whether we are in range
        if (chains.size() > name.context) {
            // get context's chain and try to find variable in it
            return (chains.begin() + name.context)->findVariable(name, var);
        }
        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    };

    Status_t setVariable(const Identifier_t &name, const Parser::Value_t &var) {
        // test whether we are in range
        if (chains.size() > name.context) {
            // get context's chain and try to set variable in it
            return (chains.begin() + name.context)->setVariable(name, var);
        }

        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    };

    Status_t
    getFragmentSize(const Identifier_t &name, unsigned int &fragSize) const {
        // test whether we are in range
        if (chains.size() > name.context) {
            // get context's chain and try to find variable in it
            return (chains.begin() + name.context)->
                getFragmentSize(name, fragSize);
        }
        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    }

    Status_t
    getSubFragmentSize(const Identifier_t &name, unsigned int &fragSize) const {
        // test whether we are in range
        if (chains.size() > name.context) {
            // check for error fragment;
            if (enableErrorFragment && !name.depth
                && (name.name == ERROR_FRAG_NAME)) {
                fragSize = error.count();
                return S_OK;
            }

            // get context's chain and try to find variable in it
            return (chains.begin() + name.context)
                ->getSubFragmentSize(name, fragSize);
        }
        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    }

    inline Status_t getFragmentIndex(const Identifier_t &name,
                                     unsigned int &fragmentIteration,
                                     unsigned int *fragmentSize = 0)
        const
    {
        // test whether we are in range
        if (chains.size() > name.context) {
            // get context's chain and try to find variable in it
            return (chains.begin() + name.context)->
                getFragmentIndex(name, fragmentIteration, fragmentSize);
        }
        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    }

    Status_t exists(const Identifier_t &name) const {
        // test whether we are in range
        if (chains.size() > name.context) {
            // get context's chain and try to find variable in it
            return ((chains.begin() + name.context)->exists(name)
                    ? S_OK : S_NOT_FOUND);
        }
        // not found (invalid context position) => probably badly composed
        // bytecode
        return S_OUT_OF_CONTEXT;
    };


    /** Repeat fragment.
     *
     * @param name of fragment to repeat
     * @param returnAddress return address
     */
    Status_t repeatFragment(const Identifier_t &name, int returnAddress) {
        return S_NO_ITERATIONS;
    }

    /** Returns current path from root fragment to current one.
     */
    std::string currentPath() const {
        return chains.back().getPath();
    }

private:
    FragmentStack_t(const FragmentStack_t&);
    FragmentStack_t operator= (const FragmentStack_t&);

    const Fragment_t *data;
    Error_t &error;
    bool enableErrorFragment;

    RegularFragmentFrame_t root;
    std::vector<FragmentChain_t> chains;
};

} // namespace Teng

#endif // TENGFRAGEMENTSTACK_H

