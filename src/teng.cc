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
 * $Id: teng.cc,v 1.4 2005-06-22 07:16:07 romanmarek Exp $
 *
 * DESCRIPTION
 * Teng engine -- implementation.
 *
 * AUTHORS
 * Vaclav Blazek <blazek@firma.seznam.cz>
 *
 * HISTORY
 * 2003-09-17  (vasek)
 *             Created.
 * 2005-06-21  (roman)
 *             Win32 support.
 */


#include <unistd.h>

#include <stdexcept>
#include <memory>

#include "teng.h"
#include "tenglogging.h"
#include "tengstructs.h"
#include "tengprocessor.h"
#include "tengcontenttype.h"
#include "tengtemplate.h"
#include "tengformatter.h"
#include "tengplatform.h"

extern "C" int teng_library_present() {return 0;}

namespace Teng {
namespace {

std::string prependBeforeExt(const std::string &str, const std::string &prep) {
    // no prep or no str -> return str
    if (prep.empty() || str.empty()) return str;

    // find the last dot and the last slash
    std::string::size_type dot = str.rfind('.');
    std::string::size_type slash = str.rfind('/');
#ifdef WIN32
    std::string::size_type bslash = str.rfind('\\');
    if (bslash > slash)
        slash = bslash;
#endif //WIN32

    // if last slash exists and slash after dot or no dot
    // append prep at the end
    if (((slash != std::string::npos) && (slash > dot)) ||
        (dot == std::string::npos)) {
        return str + '.' + prep;

    } else {
        // else prepend prep before the last dot
        return str.substr(0, dot) + '.' + prep + str.substr(dot);
    }
}

int logErrors(const ContentType_t *ct, Writer_t &writer, Error_t &err) {
    if (!err) return 0;
    bool useLineComment = false;

    if (ct->blockComment.first.empty()) {
        useLineComment = true;
        if (!ct->lineComment.empty())
            writer.write(ct->lineComment + ' ');

    } else {
        if (!ct->blockComment.first.empty())
            writer.write(ct->blockComment.first + ' ');
    }

    writer.write("Error log:\n");
    for (auto &errorEntry: err.getEntries()) {
        if (useLineComment && !ct->lineComment.empty())
            writer.write(ct->lineComment + ' ');
        writer.write(errorEntry.getLogLine());
    }

    if (!useLineComment)
        writer.write(ct->blockComment.second + '\n');

    return 0;
}

} // namespace

Teng_t::Teng_t(const std::string &root, const Teng_t::Settings_t &settings)
    : root(root), templateCache(nullptr), err()
{
    // if not absolute path, prepend current working directory
    if (root.empty() || !ISROOT(root)) {
        char cwd[2048];
        if (!getcwd(cwd, sizeof(cwd))) {
            logFatal(err, {}, "Cannot get cwd (" + strerr() + ")");
            throw std::runtime_error("Cannot get cwd.");
        }
        this->root = std::string(cwd) + '/' + root;
    }

    // create template cache
    templateCache = std::make_unique<TemplateCache_t>(
        root, settings.programCacheSize, settings.dictCacheSize
    );
}

Teng_t::~Teng_t() {}

int Teng_t::generatePage(const std::string &templateFilename,
                         const std::string &skin,
                         const std::string &dict,
                         const std::string &lang,
                         const std::string &param,
                         const std::string &scontentType,
                         const std::string &encoding,
                         const Fragment_t &data,
                         Writer_t &writer,
                         Error_t &err)
{
    // find contentType desciptor for given contentType
    const ContentType_t *contentType
        = ContentType_t::findContentType(scontentType, err)->contentType.get();

    // create template
    std::unique_ptr<Template_t> templ(templateCache->createTemplate(
        prependBeforeExt(templateFilename, skin),
        prependBeforeExt(dict, lang),
        param,
        TemplateCache_t::SRC_FILE
    ));

    // append error logs of dicts and program
    err.append(templ->langDictionary->getErrors());
    err.append(templ->paramDictionary->getErrors());
    err.append(templ->program->getErrors());

    // if program is valid (not empty) execute it
    if (!templ->program->empty()) {
        Formatter_t output(writer);
        Processor_t(
            err,
            *templ->program,
            *templ->langDictionary,
            *templ->paramDictionary,
            encoding,
            contentType
        ).run(data, output);
    }

    // log error into log, if said
    if (templ->paramDictionary->isLogToOutputEnabled())
        logErrors(contentType, writer, err);

    // flush writer to output
    writer.flush();

    // append writer errors
    err.append(writer.getErrors());

    // return error level from error log
    return err.max_level;
}

int Teng_t::generatePage(const std::string &templateString,
                         const std::string &dict,
                         const std::string &lang,
                         const std::string &param,
                         const std::string &scontentType,
                         const std::string &encoding,
                         const Fragment_t &data,
                         Writer_t &writer,
                         Error_t &err)
{
    // find contentType desciptor for given contentType
    const ContentType_t *contentType
        = ContentType_t::findContentType(scontentType, err)->contentType.get();

    // create template
    std::unique_ptr<Template_t> templ(templateCache->createTemplate(
        templateString,
        prependBeforeExt(dict, lang),
        param,
        TemplateCache_t::SRC_STRING
    ));

    // append error logs of dicts and program
    err.append(templ->langDictionary->getErrors());
    err.append(templ->paramDictionary->getErrors());
    err.append(templ->program->getErrors());

    // if program is valid (not empty) execute it
    if (!templ->program->empty()) {
        Formatter_t output(writer);
        Processor_t(
            err,
            *templ->program,
            *templ->langDictionary,
            *templ->paramDictionary,
            encoding,
            contentType
        ).run(data, output);
    }

    // log error into log, if said
    if (templ->paramDictionary->isLogToOutputEnabled())
        logErrors(contentType, writer, err);

    // flush writer to output
    writer.flush();

    // append writer errors
    err.append(writer.getErrors());

    // return error level from error log
    return err.max_level;
}

int Teng_t::dictionaryLookup(const std::string &config,
                             const std::string &dict,
                             const std::string &lang,
                             const std::string &key,
                             std::string &value)
{
    // find value for key
    auto path = prependBeforeExt(dict, lang);
    auto *dictionary = templateCache->createDictionary(config, path);
    auto *foundValue = dictionary->lookup(key);
    if (!foundValue) {
        // not fount => error
        value.erase();
        return -1;
    }
    // found => assign
    value = *foundValue;
    return 0;
}

std::vector<std::pair<std::string, std::string>>
Teng_t::listSupportedContentTypes() {
    return ContentType_t::listSupported();
}

} // namespace Teng

