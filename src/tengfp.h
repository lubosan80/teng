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
 * $Id: tengprocessor.cc,v 1.15 2010-06-11 07:46:26 burlog Exp $
 *
 * DESCRIPTION
 * Teng floating point utils.
 *
 * AUTHORS
 * Jan Nemec <jan.nemec@firma.seznam.cz>
 * Vaclav Blazek <blazek@firma.seznam.cz>
 * Michal Bukovsky <michal.bukovsky@firma.seznam.cz
 *
 * HISTORY
 * 2018-07-07  (burlog)
 *             Extracted from tengprocessor.cc.
 */

#ifndef TENGFP_H
#define TENGFP_H

#ifdef HAVE_FENV_H
#include <fenv.h>
#endif /* HAVE_FENV_H */

namespace Teng {

/** Does floating point calculation and checks fe exception flags. If
 * calculation violates any of them then function invokes error callback.
 */
template <typename fp_calculation_t, typename error_t>
auto fp_safe(fp_calculation_t calculation, error_t error) {
#ifdef HAVE_FENV_H
        feclearexcept(FE_ALL_EXCEPT);
#endif /* HAVE_FENV_H */
        auto result = calculation();
#ifdef HAVE_FENV_H
        if (fetestexcept(FE_ALL_EXCEPT) & (~FE_INEXACT) & (~FE_INVALID))
            return error();
#endif /* HAVE_FENV_H */
        return result;
}

} // namespace Teng

#endif /* TENGFP_H */

