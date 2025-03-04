/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

// Support for Russian transliteration
namespace RussianTranslit {

sptr< Dictionary::Class > makeDictionary();
}
