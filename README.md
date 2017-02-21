# dts2obj
The dts2obj tool is a command-line application used to convert Torque shape (DTS) files to Wavefront OBJ models. The tool is invoked from the windows command prompt as follows:

```
dts2obj [options] dtsFilename
```

Where dtsFilename is the path to the Torque shape (.dts) file to convert (the DTS file does not need to be in the same folder as the dts2obj tool). The tool exit code is zero on success or non-zero on failure, making it suitable for use within a larger batch conersion process. The following options are available:

**--output objFilename**

  Set the output OBJ filename. If not specified, the output will have the same base name as the input DTS file, but with OBJ extension.
