Binding dynamic variables in theme elements
===========================================

How it works
============

Binding theme elements with data is allowed using dynamic variables.

Dynamic variables are to be written **{type:property}** ( ex: {game:name} ). 

They can be used in every theme element of types int/float/bool/string/path is bindable, except vector elements ( NORMALIZED_RECT & NORMALIZED_PAIR ).

They can't be use in "if" attributes which requires static variables.

You can also use dynamic variables into the "enabled" attribute in the storyboards animation element ( <animation enabled="{global:cheevos}" [...] /> ).

The Binding engine supports standard operators evaluation ( +, -, *, /, &&, ||, ! ) and also simple evaluation methods.

Be careful : Dynamic variables  should not be confused with static variables. Static Variables use the $ sign ( ${variableName} ). Static variables are evaluated when the theme loads only.


Data sources
============

**global:**
```
help				bool  
clock				bool
architecture			string
cheevos				bool
cheevosUser			string
netplay				bool
netplay.username		string
ip				string
battery				bool
batteryLevel			int
screenWidth			int
screenHeight			int
screenRatio			string
vertical			bool
```	
**system:**
```	
name				string
fullName                	string
manufacturer            	string
theme                   	string
releaseYear             	string
hardwareType            	string
command                 	string
group                   	string
collection			bool
subSystems			int
total				int
played                  	int
favorites               	int
hidden                  	int
gamesPlayed             	int
mostPlayed              	int
gameTime                	string
lastPlayedDate          	string
image				path
showManual			bool
showSaveStates			bool
showCheevos			bool
showFlags			bool
showFavorites			bool
showParentFolder	      bool
hasKeyboardMapping      bool
isCheevosSupported			bool
isNetplaySupported      bool
hasfilter			bool
filter			string
```

**game:**
```
system				system object     (properties can be accessed using {game:system:propertyname}
name                    	string
rom                     	string
path                    	string
favorite                	bool
hidden                  	bool
kidGame                 	bool
gunGame                 	bool
wheelGame               	bool
cheevos                 	bool
folder                  	bool
placeholder                	bool
playerCount             	int
hasManual               	bool
hasSaveState            	bool
gametime                	string
desc                    	string
image                   	path
thumbnail               	path
video                   	path
marquee                 	path
fanart                  	path
titleshot               	path
manual                  	path
magazine                	path
map                     	path
bezel                   	path
cartridge               	path
boxart                  	path
boxback                 	path
wheel                   	path
mix                     	path
rating                  	float
releasedate             	string
developer               	string
publisher               	string
genre                   	string
family                  	string
arcadesystemname        	string
players                 	string
playcount               	int
lastplayed              	string
crc32                   	string
md5                     	string	
lang                    	string
region                  	string
cheevosHash             	string
cheevosId               	int
directory                 string
type                      string
stars                     string
```

Methods
=======
```
exists(path)			bool
empty(string)			bool
upper(string)			string
lower(string)			string
X ? A : B			condition
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



Static variables in theme elements
==================================

Static variables are evaluated when the theme loads. Values can't change at runtime.

They can be used in every element or condition, and custom static variables can be created in the <variables> element.

Variables
=========
```
global.architecture
global.help					      bool
global.clock				      bool
global.cheevos				    bool
global.cheevos.username   string
global.netplay				    bool
global.netplay.username   string
global.language           string
screen.width				      float
screen.height				      float
screen.ratio              string
screen.vertical           bool
system.cheevos				    bool
system.netplay				    bool
system.savestates			    bool
system.fullName           string
system.group              string
system.hardwareType       string
system.manufacturer       string
system.name               string
system.releaseYear        string
system.releaseYearOrNull  string
system.sortedBy           string
system.theme              string
system.command            string
```
