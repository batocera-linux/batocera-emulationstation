#!/bin/bash

if test $# -ne 1
then
    echo "${0} <language>" >&2
    exit 1
fi

L=$1
if ! test -d "locale/lang/${L}"
then
    echo "directory locale/lang/${L} doesn't exist" >&2
    exit 1
fi

if ! wget "http://translations.batocera.org/po/es_${L}.po" -O "locale/lang/${L}/LC_MESSAGES/emulationstation2.po"
then
    echo "unable to find file on translations.batocera.org" >&2
    exit 1
fi

exit 0
