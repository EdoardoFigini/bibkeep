# Bibkeep

Keep track of your references.

`bibkeep` is a small command-line tool for keeping track of research sources.
Each library stores titles, authors, DOIs, URLs, abstracts, keywords and much more,
so you can organize a your references in plain text.

## Build

Build using clang:

`clang -Ithird_party src\*.c -o bibkeep.exe` 

## Usage

```
USAGE
     bibkeep <command> [OPTIONS]

ARGS
     command         str      Command to be executed.

OPTIONS
     --help                   Displays this page and exits with 0.

COMMANDS
     list                     Lists all sources in a library.
     info                     Displays informations on a source.
     search                   Search for a source.
     add                      Adds a source to a library.
     bibtex                   Outputs a BibTeX entry for a source.
     help                     Displays this page and exits with 0.
```
