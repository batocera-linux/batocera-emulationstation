#!/bin/bash

if ! wget -q "http://translations.batocera.org/?q=updatable&type=es" -O - |
	while read L
	do
	    echo "updating ${L}..." >&2
	    if ! test -d "locale/lang/${L}"
	    then
		echo "directory locale/lang/${L} doesn't exist" >&2
		exit 1
	    fi
	    
	    if ! wget -q "http://translations.batocera.org/po/es_${L}.po" -O "locale/lang/${L}/LC_MESSAGES/emulationstation2.po"
	    then
		echo "unable to find file on translations.batocera.org" >&2
		exit 1
	    fi

	    dos2unix "locale/lang/${L}/LC_MESSAGES/emulationstation2.po"

	done
then
    echo "failed" >&2
    exit 1
fi
    
exit 0
