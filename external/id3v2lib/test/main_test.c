/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>

#include "compat_test.h"
#include "delete_test.h"
#include "get_test.h"
#include "set_test.h"

int main()
{
    get_test_main();
    set_test_main();
    delete_test_main();
    compat_test_main();
}
