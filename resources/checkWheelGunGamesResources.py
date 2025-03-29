#!/usr/bin/python3

import xml.etree.ElementTree as ET


def xml2systemsArrays(path):
    res = dict()
    tree = ET.parse(path)
    root = tree.getroot()

    for nodesystem in root.findall(".//system"):
        system = nodesystem.get("id")
        if system in res:
            raise Exception("duplicated system found")
        res[system] = []
        for nodegame in nodesystem:
            res[system].append(nodegame.get("id"))
    return res

def checkSystemGames(system, games):
    success = True
    for x in range(len(games)): # for each game, we check it is not a part of the next ones
        for y in range(x+1, len(games)):
            if games[x] in games[y]:
                if games[x] == games[y]:
                    print("system {} failed : duplicated game {}".format(system, games[x]))
                    success = False
                else:
                    if system not in ["arcade"]: # arcade has exact matching, no need to sort them
                        print("system {} failed : game {} should be before game {} in the list".format(system, games[y], games[x]))
                        success = False
    return success

hasError = False

for file in ["resources/gamesdb.xml"]:
    systems = xml2systemsArrays(file)
    for system in systems:
        if checkSystemGames(system, systems[system]) == False:
            hasError = True

if hasError:
    raise Exception("checking systems failed")
