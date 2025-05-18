Binding dynamic variables in theme elements
===========================================

How it works
============

Binding theme elements with data is allowed using dynamic variables.

Dynamic variables are to be written **{type:property}** ( ex: {game:name} ). 

They can be used in every theme element of types int/float/bool/string/path is bindable, except vector elements ( NORMALIZED_RECT & NORMALIZED_PAIR ).

They can't be use in "if" attributes which requires static theme variables.

You can also use dynamic variables in the "enabled" attribute in the storyboards animation element ( <animation enabled="{global:cheevos}" [...] /> ).

The Binding engine supports standard operators evaluation ( +, -, *, /, &&, ||, ! ) and also simple evaluation methods.

Be careful : Dynamic variables  should not be confused with static variables. Static Variables use the $ sign ( ${variableName} ). Static variables are evaluated when the theme loads only.


Data sources
============

**system:**
```	
name                        string
fullName                    string
manufacturer                string
theme                       string
releaseYear                 string
hardwareType                string
command                     string
group                       string                 Name of the group
collection                  bool
subSystems                  int                    Number of susbystems (collections / groups)
total                       int                    Number of games / elements
played                      int
favorites                   int
hidden                      int
gamesPlayed                 int
mostPlayed                  int
gameTime                    string
lastPlayedDate              string
image                       path
showManual                  bool
showSaveStates              bool
showCheevos                 bool
showFlags                   bool
showFavorites               bool
showParentFolder            bool
showGun                     bool
showWheel                   bool
hasKeyboardMapping          bool
isCheevosSupported          bool
isNetplaySupported          bool
hasfilter                   bool
filter                      string
ascollection                system                 Returns current system only if it's a collection, otherwise null
random                      random                 Return a systemrandom object 
```

**collection:**
The collection object is of type system, and has the same properties. 
It exists only when the view is in a collection and represent the current collection system.
It can be tested with the expression : empty({collection:name})

**game:**
```
system                      system                object of type system. Properties can be accessed using {game:system:propertyname}
name                        string                Scrapped name of the game
rom                         string                Filename
stem                        string                Filename, without extension
path                        string                Full path
favorite                    bool
hidden                      bool
kidGame                     bool
gunGame                     bool
wheelGame                   bool
cheevos                     bool
folder                      bool
placeholder                 bool                  True if it's a placeholder ( item when a system is empty )
playerCount                 int
hasManual                   bool
hasSaveState                bool
hasKeyboardMapping          bool
gametime                    int	                  time played in seconds
desc                        string
image                       path
thumbnail                   path
video                       path
marquee                     path
fanart                      path
titleshot                   path
manual                      path
magazine                    path
map                         path
bezel                       path
cartridge                   path
boxart                      path
boxback                     path
wheel                       path
mix                         path
rating                      float
releasedate                 date
releaseyear                 string
developer                   string
publisher                   string
genre                       string
family                      string
arcadesystemname            string
players                     string
playcount                   int
lastplayed                  date
crc32                       string
md5                         string	
lang                        string
region                      string
cheevosHash                 string
cheevosId                   int
directory                   string
type                        string
stars                       string                returns a string composed of rating stars
nameShort                   string                name, before any parenthesis
nameExtra                   string                name, after any parenthesis
```

**global:**
```
help                        bool  
clock                       bool
architecture                string
cheevos                     bool
cheevosUser                 string
netplay                     bool
netplay.username            string
ip                          string
battery                     bool
batteryLevel                int
screenWidth                 int
screenHeight                int
screenRatio                 string
vertical                    bool
```	

**systemrandom:**
systemrandom class is only accessible with {system:random} or {system:ascollection:random}
It can be used to find a random media from the system's games
```
image                       string
marquee                     string
thumbnail                   string
fanart                      string
titleshot                   string
video                       string
```

Methods & expressions
=====================

Evaluation is done with a basic evaluation engine.
Expressions are standard expressions, supporting common operators ( +, -, *, ^, /, ==, !=, !, &&, ||, ... )
You can do maths, or combine strings.

The engine also supports methods and conditions.
You can apply methods to every object to transform them.
Methods can be called as global methods 'exists({game:path})' or as object methods '{game:path}.exists()'

```

File and paths methods :
------------------------

exists(path)                bool                  checks if a path exists.  ex: exists({game:marquee})
isdirectory(path)           bool                  checks the path is a directory.
getextension(path)          string                get file extension ( lower )
filename(path)              string                extract filename from a path
stem(path)                  string                extract the stem ( filename without extension ) from a path.     ex : <path>"${themePath}/art/gamemedias/" + stem({game:rom}) +".png"</path>
directory(path)             string                extract the directory from a path
filesize(path)              long                  gets the file size in bytes
filesizekb(path)            long                  gets a string with file size, formatted with kilobytes
filesizemb(path)            long                  gets a string with file size, formatted with megabytes
firstfile(path, path...)    string                returns the first path that exists. unlimited number of arguments.

String methods :
----------------
empty(string)               bool                  checks if a string is empty. 
upper(string)               string                makes a string uppercase
lower(string)               string                makes a string lowercase                                          ex : lower(translate({game:cheevos} ? "YES" : "NO"))
trim(string)                string                trims spaces at begin and end of a string
proper(string)              string                make the first char uppercase, then lowercase.
tointeger(string)           int                   converts a string to an integer ( for evaluation purpose )
toboolean(string)           bool                  parse a string and convert to boolean value
translate(string)           string                translate an english string to current locale. The source string must exist in the resources\locale po/mo files. ex : translate("never"), translate("Unknown"), translate("YES"),  translate("NO")
contains(string, string)    bool                
startswith(string, string)  bool                
endswith(string, string)    bool                
default(string)             string                default text for empty values. "None" when a value is 0 or false. "Unknown" when the value is empty

Math :
------
min(int, int)               int
max(int, int)               int
clamp(int, int, int)        int
tostring(int)               string                converts an integer to a string ( for evaluation purpose )

Date methods :
--------------
elapsed(date)               string                converts a date to a duration from now, to a string "8 days ago". ex : empty({game:lastplayed}) ? translate("never") : elapsed({game:lastplayed})
expandseconds(int)          string                formats a duration in seconds, to a string "3 hours ago".         ex : {game:gametime} == 0 ? translate("Unknown") : expandseconds({game:gametime})
formatseconds(int)          string                formats a duration in seconds, to a string with format "03:10"    ex : {game:gametime} == 0 ? "00:00" : formatseconds({game:gametime})
year(date)                  string                extract the year of a date to a string
month(date)                 string                extract the month of a date to a string
day(date)                   string                extract the day of a date to a string
date(datetime)              string                keep only the date part of a timetime ex : date({game:lastplayed})
time(datetime)              string                keep only the time part of a timetime ex : time({game:lastplayed})

Conditions :
------------
X ? A : B                   condition
```

Static variables in Theme elements
==================================

Static variables are evaluated when the theme loads. Values can't change at runtime.
They are documented in THEMES.md

They can be used in every element or condition, and custom static variables can be created in the <variables> element.
They are used with the syntax ${XXX} ( ex : ${global.clock} )

Static variables list
=====================
```
global.architecture
global.help                       bool
global.clock                      bool
global.cheevos                    bool
global.cheevos.username           string
global.netplay                    bool
global.netplay.username           string
global.language                   string
screen.width                      float
screen.height                     float
screen.ratio                      string
screen.vertical                   bool
system.cheevos                    bool
system.netplay                    bool
system.savestates                 bool
system.fullName                   string
system.group                      string
system.hardwareType               string
system.manufacturer               string
system.name                       string
system.releaseYear                string
system.releaseYearOrNull          string
system.sortedBy                   string
system.theme                      string
system.command                    string
themePath                         string          root folder of the theme (not ending with /). 
currentPath                       string          folder of current theme xml file (not ending with /)
```

Examples :
==========

Show image only if the game image exists :
```
<image>
  <visible>exists({game:image})</visible>
  ...
```

Show image in color if it's a favorite, in gray if it's not :
```
<image>
  <saturation>{game:favorite} ? 1 : 0</saturation>
```

Display the current system name ( can be collection or group ) :
```
<text>
  <text>{system:name}</text>
```

Display the game's system name :
```
<text>
  <text>{game:system:name}</text>
```

Enable a storyboard animation only if the game has a video :

```
<storyboard>
  <animation enabled="exists({game:video})" property="opacity" from="0" to="1" duration="400"/>
 ```

Load an image for a game, based on the rom name :
```
<image>
  <path>"${themePath}/art/gamemedias/" + stem({game:rom}) +".png"</path>
 ```
