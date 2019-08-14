# id3v2lib

id3v2lib is a library written in C to read and edit id3 tags from mp3 files. It is focused on the ease of use. As it is a personal project, the software is provided "as is" without warranty of any kind.

It is licensed under BSD License. See LICENSE for details.

It is a work in progress so some functionality is not implemented yet.

## What it can do?

id3v2lib can read and edit id3 v2.3 and v2.4 tags from mp3 files. It can read and edit a small subset of tags by default. Specifically, the most used tags:

* Title
* Album
* Artist
* Album Artist
* Comment
* Genre
* Year
* Track
* Disc Number
* Album Cover

However, it can be extended, in a very easy manner, to read other id3 tags. 

## Building id3v2lib

### Building using CMake

The id3v2lib library is built using CMake 2.6+ on all platforms. On most systems you can build and install the library using the following commands:

	$ mkdir build && cd build
	$ cmake .. 
	$ make && make install
	
Most of the times, you need to run the `make install` command with *su* privileges.

### Building using Microsoft Visual Studio

Microsoft Visual Studio needs a slightly different way of building.
Open the Visual Studio Developers Console, ch into the id3v2lib folder and execute the following:

	$ mkdir build && cd build
	$ cmake ..

This should leave you with several MSVS projects in the \build directory. Open ALL_BUILD.vcxproj with Visual Studio and build it as usual.
The resulting lib file can be found in \build\src\Debug\id3v2.lib

## Usage

You only have to include the main header of the library:
```C
#include <id3v2lib.h>
	
int main(int argc, char* argv[])
{
	// Etc..
}
```
And when compiling, link against the static library:

	$ gcc -o example example.c -lid3v2

## Main functions

### File functions

This functions interacts directly with the file to edit. This functions are:

* `ID3v2_tag* load_tag(const char* filename)`
* `void remove_tag(const char* filename)`
* `void set_tag(const char* filename, ID3v2_tag* tag)`

### Tag functions

This functions interacts with the tags in the file. They are classified in three groups:

#### Getter functions

Retrieve information from a tag, they have the following name pattern:

* `tag_get_[frame]` where frame is the name of the desired field from which to obtain information. It can be one of the previously mentioned tags. 

#### Setter functions

Set the information of a field, they have the following name pattern:

* `tag_set_[frame]` where frame is the name of the desired field to edit. It can be one of the previously mentioned tags.

#### Delete functions

Delete individual fields in the tag, they have the following name pattern:

* `tag_delete_[frame]` where frame is the name of the desired field to delete. It can be one of the previously mentioned tags.

## Examples

#### Load tags

```C
ID3v2_tag* tag = load_tag("file.mp3"); // Load the full tag from the file
if(tag == NULL)
{
	tag = new_tag();
}
	
// Load the fields from the tag
ID3v2_frame* artist_frame = tag_get_artist(tag); // Get the full artist frame
// We need to parse the frame content to make readable
ID3v2_frame_text_content* artist_content = parse_text_frame_content(artist_frame); 
printf("ARTIST: %s\n", artist_content->data); // Show the artist info
	
ID3v2_frame* title_frame = tag_get_title(tag);
ID3v2_frame_text_content* title_content = parse_text_frame_content(title_frame);
printf("TITLE: %s\n", title_content->data);
```
	
#### Edit tags

```C
ID3v2_tag* tag = load_tag("file.mp3"); // Load the full tag from the file
if(tag == NULL)
{
	tag = new_tag();
}

// Set the new info
tag_set_title("Title", 0, tag);
tag_set_artist("Artist", 0, tag);

// Write the new tag to the file
set_tag("file.mp3", tag);
```

#### Delete tags

```C
ID3v2_tag* tag = load_tag("file.mp3"); // Load the full tag from the file
if(tag == NULL)
{
	tag = new_tag();
}

// We can delete single frames
tag_delete_title(tag);
tag_delete_artist(tag);

// Write the new tag to the file
set_tag("file.mp3", tag);

// Or we can delete the full tag
remove_tag("file.mp3")
```
	
## Extending functionality

#### Read new frames

Suppose we want to read the frame that stores the copyright message (TCOP). We have to do the following:

```C
ID3v2_tag* tag = load_tag("file.mp3"); // Load the full tag from the file
if(tag == NULL)
{
	tag = new_tag();
}

ID3v2_frame* copyright_frame = tag_get_frame(tag, "TCOP"); // Get the copyright message frame
// We need to parse the frame content to make it readable, as the copyright message is a text frame,
// we use the parse_text_frame_content function.
ID3v2_frame_text_content* copyright_content = parse_text_frame_content(copyright_frame); 
printf("COPYRIGHT: %s\n", copyright_content->data); // Show the copyright info
```
	
#### Edit new frames

Suppose that now, we want to edit the copyright frame. We have to do the following:

```C
ID3v2_tag* tag = load_tag("file.mp3"); // Load the full tag from the file
if(tag == NULL)
{
	tag = new_tag();
}

ID3v2_frame* copyright_frame = NULL;
if( ! (copyright_frame = tag_get_frame(tag, "TCOP")))
{
    copyright_frame = new_frame();
    add_to_list(tag->frames, copyright_frame);
}

set_text_frame("A copyright message", 0, "TCOP", copyright_frame);
```

## Projects

If your project is using this library, let me know it and I will put it here.
	
## Copyright

Copyright (c) 2013 Lorenzo Ruiz. See LICENSE for details.
	
## Questions?

If you have any questions, please feel free to ask me. I will try to answer it ASAP.
