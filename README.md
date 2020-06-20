# ubvff

Analyser and SVG convertor for some unusual binary vector file formats. 
These files have been found inside container files in a number of Print Studio applications. 
(The program [mmex](https://github.com/david47k/mmex) can be used to extract
from the container files).

* In Type 1 files, the vector data is contained in a single file.
* In Type 2 files, the vector data is dispersed over multiple files.
* The file format is different between the two types. 

## Type 1 files (ubvff1)

These files can be identified by a header of:
```
00 00 00 03 00 00 00 00 00 00 00 00
```

and a footer of:

```
00 00 00 0D 00 00 00 0A 00 00 00 0C 00 00 00 02 00 00 00 15
```

Example filenames:

``` 
  tscp001.BIN
  BWtscp001.BIN
  TZcp001.BIN
  TZcpBW001.BIN
  006pooh.BIN
``` 

### Usage

```
ubvff1 inputFile [-svgdump outputFile] [-more] [-less]
  inputFile              File name of compatible input file.
  -svgdump ouputFile     Create an svg file. Can be "auto".
  -more                  Display more analysis information.
  -less                  Display less analysis information.
```

### Example batch usage (using bash)

```
for f in *.bin; do ./ubvff1 "$f" -svgdump "${f%.*}.svg" -less; done
```

## Type 2 files (ubvff2 and vecass)

Type 2 files have only been found in one application, A Bugs Li\*e Print Studio, in container file Bugsai.mms.

The vector data is split into several files. One is a list of points. Another contains the 
commands that use these points. Another contains metadata such as a layer name. Yet another may contain
references to multiple files to assemble multiple layers into a single image.

Example filenames:

```
  00053.bin    single layer - commands
  00052.bin    single layer - points
  00100.bin    multiple layers - commands
```

Use the program ubvff2 to dump the single layers.
Use the program vecass to automatically assemble multiple layers into a single image (the layers should already have been converted to SVG using ubvff2).

### Usage

```
ubvff2 cmdFile pointsFile [-svgdump outputFile] [-more] [-less]
  cmdFile       File name of input file that contains vector commands.
  pointsFile    File name of input file that contains point data.
                Can be "auto" to guess "NNNNN.bin" e.g. "00123.bin".
  -svgdump      Create an svg file. File name can be "auto".
  -more         Display more analysis information.
  -less         Display less analysis information.
  
vecass cmdFile outputFile
  cmdFile       File name of input file that contains vector assemble cmds.
  outputFile    File name for SVG output. Can be auto.
```

### Example batch usage (using bash)

```
# Extract the single layers
for f in *.bin; do ./ubvff2 "$f" auto -svgdump auto -less; done

# Where multiple layers form an image, assemble those layers
for f in *.bin; do ./vecass "$f" auto; done
```

## License

GNU General Public License version 2 or any later version (GPL-2.0-or-later).




