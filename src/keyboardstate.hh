/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

/// Since Qt doesn't provide a way to test for keyboard modifiers state
/// when the app isn't in focus, we have to implement this separately for
/// each platform.
class KeyboardState
{
public:

  enum Modifier {
    Alt   = 1,
    Ctrl  = 2,
    Shift = 4,
    Win   = 8, // Ironically, Linux only, since it's no use under Windows
  };

  /// Returns true if all Modifiers present within the given mask are pressed
  /// right now.
  bool static checkModifiersPressed( int mask );
};
