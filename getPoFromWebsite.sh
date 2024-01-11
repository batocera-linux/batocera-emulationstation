#!/bin/bash

if ! wget -q "http://translations.batocera.org/?q=updatable&type=es" -O - |
	while read L
	do
	    echo -n "updating ${L}..." >&2

	    if ! test -d "locale/lang/${L}"
	    then
		echo "directory locale/lang/${L} doesn't exist" >&2
		exit 1
	    fi

	    TARGET="locale/lang/${L}/LC_MESSAGES/emulationstation2.po"
	    if ! wget -q "http://translations.batocera.org/po/es_${L}.po" -O "${TARGET}"
	    then
		echo "unable to find file on translations.batocera.org" >&2
		exit 1
	    fi
	    printf "\t"
	    dos2unix "${TARGET}" 2>/dev/null
	    msgfmt --statistics "${TARGET}"

	done
then
    echo "failed" >&2
    exit 1
fi
    
exit 0
