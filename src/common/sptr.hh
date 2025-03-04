/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <memory>
// A shorthand for std::shared_ptr
template< class T >
using sptr = std::shared_ptr< T >;
